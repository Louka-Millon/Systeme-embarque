// --- Inclusion des librairies ---
#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <Wire.h> //fonctions pour l'I2C
#include <SPI.h> //fonctions pour le SPI
#include <SD.h> //lecteur de carte SD
#include <String.h>
#include "../lib/DS1307.h" //librairie pour l'horloge RTC
#include "../lib/Seeed_BME280.h" //librairie pour le capteur BME280


// --- Definition des valeurs preprocesseur ---
#define boutonr 2 //port INT0 pour le bouton rouge
#define boutonv 3 //port INT1 pour le bouton vert
#define lightsensor 9 //pin analogique pour le capteur de luminosite
#define redpin 11 //pin digital de la led rouge
#define greenpin 10 //pin digital de la led verte
#define bluepin 9 //pin digital de la led bleue


// --- Declaration des fonctions globales ---
//clock
String getTime();
//SD
void ecrire();
void sdpleine();
//capteurs
void mesurecapteur();
//boutons (interruptions)
void boutonrouge();
void boutonvert();
//système de commandes
void command();
//ecriture sur le port serie
void portserie();


// --- Declaration des variables globales ---
//parmetres globaux
int LOG_INTERVALL = 1; //delai entre chaque mesure
int FILE_MAX_SIZE = 2048; //taille d'un fichier LOG
int LUMIN = 1; //activation du capteur de luminosite
int TEMP_AIR = 0; //activation du capteur de temperature
int HYGR = 0; //activation du capteur d'hygrometrie
int PRESSURE = 0; //activation du capteur de pression
//clock
DS1307 clock;
int datenow;
int datebefore;
//SD
File fichier;
int i = 0;
unsigned long filenumber_day = 0;
unsigned long filenumber_oncard = 0;
unsigned long current_filesize = 2000;
//led RGB et modes
int statbefore = 0; //enregistre le mode precedent
int stat = 0; // 0 = standard / 1 = eco / 2 = maintenance / 3 = configuration
unsigned int temps;
//bme280 : recuperation des mesures capteur
BME280 bme280;
unsigned long tempsmesure;
//système de commandes
String commande;
String varia;
String valeur;


// --- Declaration des constantes ---
//SD
const byte chipSelect = 4; //chip correspondant au shield SD



void setup() {
  Serial.begin(9600); //ouverture du port serie

  //initialisation de la clock
  clock.begin();
  clock.fillByYMD(2020,10,24);
  clock.fillByHMS(23,30,30);
  clock.fillDayOfWeek(WED);
  clock.setTime();

  //declaration des entrees/sorties
  pinMode(redpin, OUTPUT);
  pinMode(bluepin, OUTPUT);
  pinMode(greenpin, OUTPUT);
  pinMode(boutonr, INPUT);
  pinMode(boutonv, INPUT);
  pinMode(lightsensor,OUTPUT);

  //boutons (interruptions)
  temps = millis(); //temps passé depuis le démarrage du système
  attachInterrupt(0, boutonrouge, RISING);
  attachInterrupt(1, boutonvert, RISING);

  //verification carte SD
  Serial.println("Initialisation de la carte SD...");
  if(!SD.begin(chipSelect)){ //si carte non detectee/ne fonctionne pas
    Serial.println("Echec de l'initialisation.");
    //extinction prealable des leds
    analogWrite(redpin,0);
    analogWrite(greenpin,0);
    analogWrite(bluepin,0);
    while(!SD.begin(chipSelect)){ //code d'erreur "1 temps rouge/2 temps blanc" jusqu'à l'insertion de la carte
      analogWrite(redpin,255);
      delay(1000);
      analogWrite(greenpin,255);
      analogWrite(bluepin,255);
      delay(2000);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
    }
  }
  Serial.println("Initialisation terminee.");
  String test = String(clock.year,DEC); //preparation du nom des fichiers LOG
  test += String(clock.month,DEC);
  test += String(clock.dayOfMonth,DEC);
  test += "_";
  test += String(filenumber_day);
  test += ".LOG"; 
  fichier = SD.open(test.c_str(), FILE_WRITE); //conversion du fichier en chaine de caracteres et ouverture
  datenow = clock.dayOfMonth;
  //verification BME280
  if(!bme280.init()){
    Serial.println("Erreur d'acces au capteur BME280.");
    //extinction prealable des leds
    analogWrite(redpin,0);
    analogWrite(greenpin,0);
    analogWrite(bluepin,0);
    while(!bme280.init()){ //code d'erreur "1 temps rouge/1 temps vert"
      analogWrite(redpin,255);
      delay(1000);
      analogWrite(redpin,0);
      analogWrite(greenpin,255);
      delay(1000);
      analogWrite(greenpin,0);
    }
  }

  tempsmesure = millis();


  
}

void loop() {
  switch(stat){ //TODO changer les couleurs RGB + implementer les fonctions des modes
    case 0: //mode standard
      //extinction prealable des leds
      //Serial.println("mode standard");
      analogWrite(redpin,0);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
      //allumage de la led verte
      analogWrite(greenpin,255);
      mesurecapteur();
      break;
    case 1: //mode economique
      //extinction prealable des leds
      //Serial.println("mode economique");
      analogWrite(redpin,0);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
      //allumage de la led bleue
      analogWrite(bluepin,255);
      mesurecapteur();
      break;
    case 2: //mode maintenance
      //extinction prealable des leds
      Serial.println("mode maintenance");
      analogWrite(redpin,0);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
      //allumage de la led orange
      analogWrite(redpin,244);
      analogWrite(greenpin,109);
      analogWrite(bluepin,27);
      mesurecapteur();
      break;
    case 3: //mode configuration
      //extinction prealable des leds
      Serial.println("mode configuration"); //TODO gérer le temps pour le mode config
      analogWrite(redpin,0);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
      //allumage de la led jaune
      analogWrite(greenpin,255);
      analogWrite(redpin,255);
      command();
      break;
  }
  
  if(stat == 3){
      if(millis() > (temps + 1000*10)){
        stat = 0;
      }
  }


  datebefore = datenow;
  datenow = clock.dayOfMonth;
  if(datebefore != datenow){
    filenumber_day = 0;
  }
  
  delay(1000);
}


String getTime(){
  String time="";
  clock.getTime();
  time+=String(clock.hour, DEC);
  time+=String(":");
  time+=String(clock.minute, DEC);
  time+=String(":");
  time+=String(clock.second, DEC);
  time+=String(" ");
  time+=String(clock.month, DEC);
  time+=String("/");
  time+=String(clock.dayOfMonth, DEC);
  time+=String("/");
  time+=String(clock.year+2000, DEC);
  time+=String(" ");
  time+=String(clock.dayOfMonth);
  time+=String("*");
  switch (clock.dayOfWeek)//determine le jour de la semaine
  {
    case MON:
    time+=String("MON");
    break;

    case TUE:
    time+=String("TUE");
    break;

    case WED:
    time+=String("WED");
    break;

    case THU:
    time+=String("THU");
    break;

    case FRI:
    time+=String("FRI");
    break;

    case SAT:
    time+=String("SAT");
    break;

    case SUN:
    time+=String("SUN");
    break;
  }
  time+=String(" ");
  return time;
}


void ecrire(float *temperature, float *pressure, float *humidity, int *lumen){
  /*if(String(clock.dayOfMonth,DEC) == NULL || String(clock.month,DEC) == NULL || String(clock.year,DEC) == NULL || String(filenumber_day) == NULL){ //si erreur de donnees de l'horloge
    Serial.println("Erreur d'acces a l'horloge RTC.");
    //extinction prealable des leds
    analogWrite(redpin,0);
    analogWrite(greenpin,0);
    analogWrite(bluepin,0);
    while(String(clock.dayOfMonth,DEC) == NULL || String(clock.month,DEC) == NULL || String(clock.year,DEC) == NULL || String(filenumber_day) == NULL){ //code d'erreur "1 temps rouge/1 temps bleu"
      analogWrite(redpin,255);
      delay(1000);
      analogWrite(redpin,0);
      analogWrite(bluepin,255);
      delay(1000);
      analogWrite(bluepin,0);
    }
  }*/

  sdpleine();
  //today = String(clock.dayOfMonth,DEC);
  fichier.flush();
  fichier.close();
  String test = String(clock.year,DEC);
  test += String(clock.month,DEC);
  test += String(clock.dayOfMonth,DEC);
  test += "_";
  test += String(filenumber_day);
  test += ".LOG"; 
  Serial.println(test);
  fichier = SD.open(test.c_str(), FILE_WRITE);
  if(fichier){

    if(fichier.size() > 2048){
      fichier.flush();
      fichier.close();
      filenumber_day++;
      filenumber_oncard++;
      String test = String(clock.year,DEC);
      test += String(clock.month,DEC);
      test += String(clock.dayOfMonth,DEC);
      test += "_";
      test += String(filenumber_day);
      test += ".LOG"; 
      fichier = SD.open(test.c_str(), FILE_WRITE);
    }
    fichier.print(getTime());
    fichier.print(*pressure);
    fichier.print("hPa ");
    fichier.print(*temperature);
    fichier.print("C ");
    fichier.print(*humidity);
    fichier.print("% ");
    fichier.print(*lumen);
    fichier.println(" lumen");    
  }/*else{
    while(!SD.begin(chipSelect)){ //code d'erreur "1 temps rouge/2 temps blanc" jusqu'à l'insertion de la carte
      analogWrite(redpin,255);
      delay(1000);
      analogWrite(greenpin,255);
      analogWrite(bluepin,255);
      delay(2000);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
    }
  }*/
  fichier.flush();
  fichier.close();
  
}

void sdpleine(){
  unsigned int maxfile =  72500000000 / current_filesize + 100; //72500000000
  if(maxfile <= filenumber_oncard){ //si memoire pleine
    //extinction prealable des leds
    analogWrite(redpin,0);
    analogWrite(greenpin,0);
    analogWrite(bluepin,0);
    while(filenumber_oncard >= maxfile){ //code d'erreur "1 temps rouge/1 temps blanc"
      analogWrite(redpin,255);
      delay(1000);
      analogWrite(greenpin,255);
      analogWrite(bluepin,255);
      delay(1000);
      analogWrite(greenpin,0);
      analogWrite(bluepin,0);
    }
  }
}

void portserie(float *temperature, float *pressure, float *humidity, int *val){
  //affiche la temperature
  Serial.print("Temperature : ");
  Serial.print(*temperature);
  Serial.println("C");
 
  //affiche la pression
  Serial.print("Pression : ");
  Serial.print(*pressure);
  Serial.println("Pa");
 
  //affiche le taux d'humidite
  Serial.print("Taux d'humidite : ");
  Serial.print(*humidity);
  Serial.println("%");
 
  //affiche la luminosite
  Serial.print("Luminosite : ");
  Serial.print(*val);
  Serial.println(" lumen");
}

void mesurecapteur(){
  //variables locales
  unsigned long INTERVALL;
  if(stat == 1 ){
    INTERVALL = LOG_INTERVALL*2;
  }else{
    INTERVALL = LOG_INTERVALL;
  }
  //Serial.println(tempsmesure);
  //Serial.println(millis());
  if(tempsmesure+INTERVALL*60000 < millis()){
    tempsmesure += INTERVALL*600000;
    float temperature = bme280.getTemperature();
    float pressure = bme280.getPressure();
    float humidity = bme280.getHumidity();
    int sensorValue = analogRead(0); 
    int val = map(sensorValue,0 ,768 ,0, 1023);

    // --- Test coherence des capteurs ---
    if(LUMIN == 1){
      if((val > 1023) || (val < 0 )){
        //extinction prealable des leds
        analogWrite(redpin,0);
        analogWrite(greenpin,0);
        analogWrite(bluepin,0);
        while(1){ //code d'erreur "1 temps rouge/2 temps vert"
          analogWrite(redpin,255);
          delay(1000);
          analogWrite(redpin,0);
          analogWrite(greenpin,255);
          delay(2000);
          analogWrite(greenpin,0);
        }
      } 
    }else{
      val = -255;
    }
    if(TEMP_AIR == 1){
      if((temperature > 85) || (temperature < -40 )){
        //extinction prealable des leds
        analogWrite(redpin,0);
        analogWrite(greenpin,0);
        analogWrite(bluepin,0);
        while(1){ //code d'erreur "1 temps rouge/2 temps vert"
          analogWrite(redpin,255);
          delay(1000);
          analogWrite(redpin,0);
          analogWrite(greenpin,255);
          delay(2000);
          analogWrite(greenpin,0);
        }
      } 
    }else{
      temperature = -255;
    }
    if(HYGR == 1){
      if((humidity > 85) || (humidity < -40 )){
        //extinction prealable des leds
        analogWrite(redpin,0);
        analogWrite(greenpin,0);
        analogWrite(bluepin,0);
        while(1){ //code d'erreur "1 temps rouge/2 temps vert"
          analogWrite(redpin,255);
          delay(1000);
          analogWrite(redpin,0);
          analogWrite(greenpin,255);
          delay(2000);
          analogWrite(greenpin,0);
        }
      } 
    }else{
      humidity = -255;
    }
    if(PRESSURE == 1){
      if((pressure > 300) || (pressure < 1100 )){
        //extinction prealable des leds
        analogWrite(redpin,0);
        analogWrite(greenpin,0);
        analogWrite(bluepin,0);
        while(1){ //code d'erreur "1 temps rouge/2 temps vert"
          analogWrite(redpin,255);
          delay(1000);
          analogWrite(redpin,0);
          analogWrite(greenpin,255);
          delay(2000);
          analogWrite(greenpin,0);
        }
      } 
    }else{
      pressure = -255;
    }

    //ecriture des donnees
    if(stat == 0 || stat == 1){ //en mode standard et economique : ecriture sur la carte SD
      ecrire(&temperature, &pressure, &humidity, &val);
    }else if(stat == 2){ //en mode maintenance : ecriture sur le port serie
      portserie(&temperature, &pressure, &humidity, &val);
    }
    Serial.println("Donnees ecrites.");
    
  }
  
}





void command(){
  int compter;
  char lu;
  int pos;
  if ( Serial.available() ) {
    
    while(Serial.available()){
      lu = Serial.read();
      commande += lu;
      Serial.print(lu);
    }
    Serial.println("");  
  }
  
  //séparer
  compter = commande.length();
  for(int i=0; i<compter; i++){
    if(commande[i] == '='){
      pos = i;
    }
  }
  if(pos == 0){
    pos = compter;  
  }
  
  //affecter
  varia = commande.substring(0,pos);
  valeur = commande.substring(pos+1,commande.length());
  
  if(varia == "LOG_INTERVALL"){
    LOG_INTERVALL = valeur.toInt();
    Serial.println(LOG_INTERVALL);
  }else if(varia == "FILE_MAX_SIZE"){
    FILE_MAX_SIZE = valeur.toInt();
    Serial.println(FILE_MAX_SIZE);
  }else if(varia == "VALUE"){
    Serial.print("LOG_INTERVAL = ");
    Serial.println(LOG_INTERVALL);
    Serial.print("FILE_MAX_SIZE = ");
    Serial.println(FILE_MAX_SIZE);
    commande = "";
    digitalWrite(13,HIGH);
  }else if(varia == "RESET"){
    LOG_INTERVALL = 10;
    FILE_MAX_SIZE = 2046;
    commande = "";
  }else if(varia == "VERSION"){
    Serial.println("La version de notre logiciel est V1.1");
    commande = "";  
  }

  
  varia = "";
  valeur = "";
  commande = "";
  pos = 0;


}



// --- Boutons/interruptions ---
void boutonrouge(){
  
  if(millis() < (temps + 5000)){ //si bouton rouge presse avant 5 secondes apres le démarrage
    stat = 3;
    temps = millis();
  }else{
    switch(stat){ //sinon plusieurs cas possibles
      case 0: //mode standard
        stat = 2; //passage en mode maintenance
        statbefore = 0; //mode precedent enregistre comme standard
        break;
      case 1: //mode economique
        stat = 0; //passage en mode standard
        statbefore = 1;
        break;
      case 2: //mode maintenance
        stat = statbefore; //retour au mode precedent
        break;
    } 
  }
  /*Serial.print("L'etat est : ");
  Serial.println(stat);*/
}

void boutonvert(){

  switch(stat){
    case 0: //mode standard
      stat = 1; //passage en mode economique
      statbefore = 0;
      break;
    case 1: //mode economique
      stat = 2; //passage au mode maintenance
      statbefore = 1; //mode precedent enregistre comme economique
      break;
  }
  /*Serial.print("L'etat est : ");
  Serial.println(stat);*/
}