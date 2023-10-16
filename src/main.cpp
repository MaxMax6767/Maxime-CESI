#include <SoftwareSerial.h>
#include <Wire.h>
#include "DS1307.h"
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <ezButton.h>

// Initialisation SD Card
const int chipSelect = 4;

// Initialisation Capteur Luminosite (A0)
const int analogPin = 0;

// Initialisation Horloge RTC
DS1307 clock;

// Initialisation GPS
SoftwareSerial SoftSerial(0, 1);
bool t;

// Initialisaion LEDs
ChainableLED leds(7, 8, 1);

// Initialisation BME280
Adafruit_BME280 bme;

// Initialisation Boutons
ezButton button1(2);
ezButton button2(3);

// Initialisation timer
unsigned long startTime = millis();

// Initialisations des variables
static int mode;
/*
+---+---------------------------------------------------+
| 0 | Mode Normal                                       |
+---+---------------------------------------------------+
| 1 | Mode Eco (Appui 5s sur le bouton Vert)            |
+---+---------------------------------------------------+
| 2 | Mode Maintennance                                 |
+---+---------------------------------------------------+
| 3 | Mode Config (Appui bouton rouge lors du démarage) |
+---+---------------------------------------------------+
| 4 | Mode erreur Timeout Capteur                       |
+---+---------------------------------------------------+
| 5 | Mode erreur Timeout GPS                           |
+---+---------------------------------------------------+
| 6 | Mode erreur SD pleine                             |
+---+---------------------------------------------------+
| 7 | Mode erreur enregistrement impossible             |
+---+---------------------------------------------------+
| 8 | Mode erreur données hors limites                  |
+---+---------------------------------------------------+
*/

// Initialisation de la config
int LUMIN = 1;
int LUMIN_LOW = 255;
int LUMIN_HIGH = 768;
int TEMP_AIR = 1;
int MIN_TEMP_AIR = -10;
int MAX_TEMP_AIR = 60;
int HYGR = 1;
int HYGR_MINT = 0;
int HYGR_MAXT = 50;
int PRESSURE = 1;
int PRESSURE_MIN = 850;
int PRESSURE_MAX = 1080;
int LOG_INTERVAL = 10;
int FILE_MAX_SIZE = 4096;
int TIMEOUT = 30;
/*
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| Paramètre     | Domaine de définition | Valeur par défaut | Desc                                                                     |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| LUMIN         | {0, 1}                | 1                 | Activation ou non du capteur de luminosité                               |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| LUMIN_LOW     | {0 - 1023}            | 255               | Valeur en dessous de laquelle la luminosité est basse                    |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| LUMIN_HIGH    | {0 - 1023}            | 768               | Valeur au dessus de laquelle la luminosité est haute                     |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| TEMP_AIR      | {0, 1}                | 1                 | Activation ou non du capteur de température                              |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| MIN_TEMP_AIR  | {-40 - 85}            | -10               | Valeur de température basse                                              |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| MAX_TEMP_AIR  | {-40 - 85}            | 60                | Valeur de température haute                                              |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| HYGR          | {0, 1}                | 1                 | Activation ou non du capteur d'humidité                                  |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| HYGR_MINT     | {0 - 100}             | 0                 | Valeur de température en dessous de laquelle on ne mesure pas l'humidité |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| HYGR_MAXT     | {0 - 100}             | 50                | Valeur de température au dessus de laquelle on ne mesure pas l'humidité  |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| PRESSURE      | {0, 1}                | 1                 | Activation ou non du capteur de pression                                 |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| PRESSURE_MIN  | {300 - 1100}          | 850               | Valeur de pression basse                                                 |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| PRESSURE_MAX  | {300 - 1100}          | 1080              | Valeur de pression haute                                                 |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| LOG_INTERVAL  | {1 - 60}              | 10                | Intervalle de temps entre deux enregistrements                           |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| FILE_MAX_SIZE | {1 - 4096}            | 4096              | Taille maximale du fichier                                               |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
| TIMEOUT       | {1 - 60}              | 30                | Temps avant de passer en mode erreur                                     |
+---------------+-----------------------+-------------------+--------------------------------------------------------------------------+
*/

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  leds.init();

  // Vérivication de la connexion au capteur BME280
  if (!bme.begin(0x76))
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

  // Initialisation allumage de la LED en blanc faible
  leds.setColorHSL(0, 0, 0, 0.1);

  // Initialisation du délai de rebond des boutons
  button1.setDebounceTime(10);
  button2.setDebounceTime(10);

  // Initialisation de la communication serie
  while (!Serial)
  {
  }

  // Verif de fonctionnement de la carte SD
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect))
  {
    Serial.println("Card failed, or not present");
    while (1)
      ;
  }
  Serial.println("card initialized.");

  // Emulation de communication serie pour le GPS
  SoftSerial.begin(9600);

  // Réglage de la RTC
  clock.begin();
  clock.fillByYMD(2023, 10, 16); // Jan 19,2013
  clock.fillByHMS(15, 00, 00);   // 15:28 30"
  clock.fillDayOfWeek(SAT);      // Saturday
  clock.setTime();               // write time to the RTC chip
}
String getGPS()
{
  String gpsData = "";
  if (SoftSerial.available())
  {
    t = true;
    while (t)
    {
      gpsData = SoftSerial.readStringUntil('\n');
      if (gpsData.startsWith("$GPGGA", 0))
      {
        t = false;
      }
    }
  }
  return gpsData;
}

String getTime()
{
  String time = "";
  clock.getTime();
  time += String(clock.hour, DEC);
  time += String(":");
  time += String(clock.minute, DEC);
  time += String(":");
  time += String(clock.second, DEC);
  time += String("  ");
  time += String(clock.month, DEC);
  time += String("/");
  time += String(clock.dayOfMonth, DEC);
  time += String("/");
  time += String(clock.year + 2000, DEC);
  time += String(" ");
  time += String(clock.dayOfMonth);
  time += String("*");
  switch (clock.dayOfWeek) // Friendly printout the weekday
  {
  case MON:
    time += String("LUN");
    break;
  case TUE:
    time += String("MAR");
    break;
  case WED:
    time += String("MER");
    break;
  case THU:
    time += String("JEU");
    break;
  case FRI:
    time += String("VEN");
    break;
  case SAT:
    time += String("SAM");
    break;
  case SUN:
    time += String("DIM");
    break;
  }
  time += String(" ");
  return time;
}

String Lecture()
{
  static bool gps = true;
  String dataString = "";
  dataString += getTime() + " ; ";
  if (mode == 1)
  {
    if (gps)
    {
      dataString += getGPS() + " ; ";
      gps = false;
    }
    else
    {
      gps = true;
    }
  }
  if (LUMIN == 1)
  {
    int l = analogRead(analogPin);
    if (LUMIN_LOW < l < LUMIN_HIGH)
    {
      dataString += String(l) + " ; ";
    }
    else
    {
      dataString += " ; ";
      mode = 8;
    }
  }
  else
  {
    dataString += " ; ";
  }
  if (TEMP_AIR == 1)
  {
    float t = bme.readTemperature();
    if (MIN_TEMP_AIR < t < MAX_TEMP_AIR)
    {
      dataString += String(t) + " ; ";
    }
    else
    {
      dataString += " ; ";
      mode = 8;
    }
    if (HYGR == 1)
    {
      float h = bme.readHumidity();
      if (HYGR_MINT < t < HYGR_MAXT)
      {
        dataString += String(h) + " ; ";
      }
      else
      {
        dataString += " ; ";
        mode = 8;
      }
    }
    else
    {
      dataString += " ; ";
    }
  }
  else
  {
    dataString += " ; ";
  }
  if (PRESSURE == 1)
  {
    float p = bme.readPressure() / 100.0F;
    if (PRESSURE_MIN < p < PRESSURE_MAX)
    {
      dataString += String(p) + " ; ";
    }
    else
    {
      dataString += " ; ";
      mode = 8;
    }
  }
  else
  {
    dataString += " ; ";
  }
  return dataString;
}

void enregistrement()
{
  String donnees = Lecture();
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(donnees);
    dataFile.close();
    // print to the serial port too:
    Serial.println(donnees);
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening datalog.txt");
  }
}

void loop()
{
  // Actualisation de l'état des boutons
  button1.loop();
  button2.loop();

  // Actualisation du temps
  unsigned long currentTime = millis();

  switch (mode)
  {
  case 0:
    // Mode Normal
    if (currentTime - startTime >= 1000)
    {
      startTime = currentTime;
    }
    break;
  }
}
