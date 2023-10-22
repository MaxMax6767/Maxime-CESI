#include <SoftwareSerial.h> // Librairie pour le port série du GPS
#include <Wire.h>           // Librairie pour le bus I2C
#include "DS1307.h"         // Librairie pour l'horloge
#include <forcedClimate.h>  // Librairie pour le capteur BME280
#include <ChainableLED.h>   // Librairie pour la LED
#include <SdFat.h>          // Librairie pour la carte SD
#include <EEPROM.h>         // Librairie pour l'EEPROM

SdFat SD;                                      // SD card library
DS1307 clock;                                  // RTC library
SoftwareSerial SoftSerial(0, 1);               // RX, TX
ForcedClimate climateSensor = ForcedClimate(); // BME280 library
ChainableLED leds(7, 8, 1);                    // LED library
bool t;                                        // GPS variable
unsigned long startTime = millis();            // Temps de démarrage du programme

int mode = 0;
/*
+---+--------------------------------+
| 0 | Mode initialisation            |
+---+--------------------------------+
| 1 | Mode Normal                    |
+---+--------------------------------+
| 2 | Mode Eco                       |
+---+--------------------------------+
| 3 | Mode Config                    |
+---+--------------------------------+
| 4 | Mode Maintenance               |
+---+--------------------------------+
| 5 | Mode Erreur SD Pleine          |
+---+--------------------------------+
| 6 | Mode Erreur enregistrement SD  |
+---+--------------------------------+
| 7 | Mode Erreur Valeur Hors champs |
+---+--------------------------------+
| 8 | Mode Erreur GPS                |
+---+--------------------------------+
| 9 | Mode Erreur capteur            |
+---+--------------------------------+
*/

bool LUMIN = 1;
int LUMIN_LOW = 255;
int LUMIN_HIGH = 768;
bool TEMP_AIR = 1;
int MIN_TEMP_AIR = -10;
int MAX_TEMP_AIR = 60;
bool HYGR = 1;
int HYGR_MINT = 0;
int HYGR_MAXT = 50;
bool PRESSURE = 1;
int PRESSURE_MIN = 850;
int PRESSURE_MAX = 1080;
unsigned long LOG_INTERVAL = 40;
uint32_t FILE_MAX_SIZE = 4096;
int TIMEOUT = 30;
/*
+-----------------+-------------------+-------------------+--------------------------------------------------+
| Variable       | Ensemble de valeur | Valeur par défaut | Description                                      |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| LUMIN           | 0 ou 1            | 1                 | Allumage ou non de la lecture de la luminosité   |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| LUMIN_LOW       | 0 à 1023          | 255               | Valeur minimale de la luminosité                 |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| LUMIN_HIGH      | 0 à 1023          | 768               | Valeur maximale de la luminosité                 |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| TEMP_AIR        | 0 ou 1            | 1                 | Allumage ou non de la lecture de la température  |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| MIN_TEMP_AIR    | -40 à 85          | -10               | Valeur minimale de la température                |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| MAX_TEMP_AIR    | -40 à 85          | 60                | Valeur maximale de la température                |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| HYGR            | 0 ou 1            | 1                 | Allumage ou non de la lecture de l'humidité      |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| HYGR_MINT       | -40 à 85          | 0                 | Valeur minimale de l'humidité                    |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| HYGR_MAXT       | -40 à 85          | 50                | Valeur maximale de l'humidité                    |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| PRESSURE        | 0 ou 1            | 1                 | Allumage ou non de la lecture de la pression     |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| PRESSURE_MIN    | 300 à 1100        | 850               | Valeur minimale de la pression                   |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| PRESSURE_MAX    | 300 à 1100        | 1080              | Valeur maximale de la pression                   |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| LOG_INTERVAL    | ~                 | 10                | Intervalle de temps entre chaque enregistrement  |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| FILE_MAX_SIZE   | ~                 | 4096              | Taille maximale du fichier                       |
+-----------------+-------------------+-------------------+--------------------------------------------------+
| TIMEOUT         | ~                 | 30                | Temps avant de passer en mode Eco                |
+-----------------+-------------------+-------------------+--------------------------------------------------+
*/

void switchg() // Fonction de changement de mode quand appui sur le boutton vert
{
  int i = 0;
  while (digitalRead(3) == LOW) // Tant que le boutton est appuyé
  {
    i++;                     // Incrémentation de i
    delayMicroseconds(1000); // Attente de 1ms (en microsecondes car delay() ne fonctionne pas dans une interruption)
    if (i >= 5000)           // Si i est supérieur à 5000 (5s) alors on change de mode
    {
      i = 0;
      if (mode == 1) // Si on est en mode standard, on passe en mode Eco et on mets la led en bleu
      {
        mode = 2;
        leds.setColorRGB(0, 0, 0, 150);
        break;
      }
      else if (mode == 2) // Si on est en mode Eco, on passe en mode standard et on mets la led en vert
      {
        mode = 1;
        leds.setColorRGB(0, 0, 150, 0);
        break;
      }
    }
  }
}

void switchr() // Fonction de changement de mode quand appui sur le boutton rouge
{
  static int modep; // Variable de stockage du mode précédent
  int i = 0;
  while (digitalRead(2) == LOW) // Tant que le boutton est appuyé
  {
    i++;
    delayMicroseconds(1000); // Attente de 1ms (en microsecondes car delay() ne fonctionne pas dans une interruption)
    if (i >= 5000)           // Si i est supérieur à 5000 (5s) alors on change de mode
    {
      i = 0;
      if (mode == 0) // Si on est en mode initialisation, on passe en mode config et on mets la led en jaune (A changer parce que c'est pas la bonne couleur)
      {
        mode = 3;
        leds.setColorRGB(0, 150, 0, 0);
      }
      else if (mode == 3) // Si on est en mode config, on passe en mode standard et on mets la led en vert
      {
        mode = 1;
        leds.setColorRGB(0, 0, 150, 0);
      }
      else if (mode == 1 || mode == 2) // Si on est en mode standard ou eco, on passe en mode maintenance et on mets la led en jaune
      {
        modep = mode;
        mode = 4;
        leds.setColorRGB(0, 150, 150, 0);
      }
      else if (mode == 4) // Si on est en mode maintenance, on repasse en mode standard ou eco en fonction du mode précédent et on mets la led en vert ou bleu
      {
        mode = modep;
        leds.setColorRGB(0, 0, (mode == 1) ? 150 : 0, (mode == 1) ? 0 : 150);
      }
      break;
    }
  }
}

void setup()
{
  Serial.begin(9600);     // Initialisation du port série
  Wire.begin();           // Initialisation du bus I2C
  leds.init();            // Initialisation de la LED
  SoftSerial.begin(9600); // Initialisation du port série du GPS
  climateSensor.begin();  // Initialisation du capteur BME280
  if (!SD.begin(4))       // Initialisation de la carte SD
  {
    Serial.println(F("Card failed, or not present"));
    while (1)
      ;
  }

  clock.begin();                 // Initialisation de l'horloge
  clock.fillByYMD(2023, 10, 16); // Définition de la date (Année, Mois, Jour)
  clock.fillByHMS(15, 00, 00);   // Définition de l'heure (Heure, Minute, Seconde)
  clock.fillDayOfWeek(SAT);      // Définition du jour de la semaine (MON, TUE, WED, THU, FRI, SAT, SUN)
  clock.setTime();               // Envoi de la date et de l'heure à l'horloge

  attachInterrupt(digitalPinToInterrupt(3), switchg, LOW); // Interruption sur le boutton vert
  attachInterrupt(digitalPinToInterrupt(2), switchr, LOW); // Interruption sur le boutton rouge

  leds.setColorRGB(0, 150, 150, 150); // LED blanche au démarrage
}

String getGps() // Fonction de récupération des données GPS
{
  static bool gps; // Variable de stockage de l'état de la mesure du GPS
  if (mode != 2)   // Si on est pas en mode Eco, on récupère les données GPS
  {
  mesure:
    String gpsData = "";        // Variable de stockage des données GPS
    if (SoftSerial.available()) // Si le port série du GPS est disponible
    {
      t = true;
      while (t)
      {
        gpsData = SoftSerial.readStringUntil('\n'); // Lecture des données GPS
        if (gpsData.startsWith(F("$GPGGA"), 0))     // Si la ligne démare avec $GPGAA, il s'agit d'une mesure du GPS
        {
          t = false;
        }
      }
    }
    // Si l'acquisition des données à ratée (ex: pas de signal GPS), on réinitialise les données à vide
    Serial.println(gpsData);
    return gpsData; // Retourne les données GPS
  }
  else // Sinon on récupère les données GPS une fois sur deux
  {
    if (gps)
    {
      gps = !gps;
      goto mesure;
    }
    else
    {
      gps = !gps;
      return "";
    }
  }
}

void erreur(int R, int G, int B, int R2, int G2, int B2, int d) // Fonction d'erreur
{
  while (true)
  {
    leds.setColorRGB(0, R, G, B);
    delay(1000);
    leds.setColorRGB(0, R2, G2, B2);
    delay(d);
  }
}

float getCapteur(bool o, float m, int l, int h) // Fonction de récupération des données des capteurs
{
  float n;
  if (o) // Si le capteur est activé
  {
    if (m < l || m > h) // Si la valeur est hors des limites
    {
      n = NAN; // On retourne NAN
    }
    else
    {
      n = m; // Sinon on retourne la valeur
    }
  }
  else
  {
    n = NAN; // On retourne NAN
  }
  return n; // Retourne la valeur
}

String nom(int i) // Fonction de création du nom du fichier
{
  return String(clock.year) + String(clock.month) + String(clock.dayOfMonth) + "_" + String(i) + ".log"; // Retourne le nom du fichier
}

void prnt(File f) // Fonction d'écriture dans le fichier
{
  climateSensor.takeForcedMeasurement();
  clock.getTime();
  f.print(String(clock.hour) + F(":") + String(clock.minute, DEC) + F(":") + String(clock.second, DEC)); // Ecriture de l'heure
  f.print(F(" ; "));                                                                                     // Ecriture du séparateur
  f.print(getGps());                                                                                     // Ecriture des données GPS
  f.print(F(" ; "));                                                                                     // Ecriture du séparateur
  f.print(getCapteur(LUMIN, analogRead(A0), LUMIN_LOW, LUMIN_HIGH));                                     // Ecriture de la luminosité
  f.print(F(" ; "));                                                                                     // Ecriture du séparateur
  f.print(getCapteur(TEMP_AIR, climateSensor.getTemperatureCelcius(), MIN_TEMP_AIR, MAX_TEMP_AIR));      // Ecriture de la température
  f.print(F(" ; "));                                                                                     // Ecriture du séparateur
  f.print(getCapteur(HYGR, climateSensor.getRelativeHumidity(), HYGR_MINT, HYGR_MAXT));                  // Ecriture de l'humidité
  f.print(F(" ; "));                                                                                     // Ecriture du séparateur
  f.println(getCapteur(PRESSURE, climateSensor.getPressure() / 100.0F, PRESSURE_MIN, PRESSURE_MAX));     // Ecriture de la pression
  f.close();                                                                                             // Fermeture du fichier
}

void ecriture() // Fonction de gestion de l'écriture dans le fichier sur la carte SD
{
  static int rev = 0;          // Variable de stockage du numéro de révision du fichier
  static String work = nom(0); // Variable de stockage du nom du fichier

  if (SD.freeClusterCount() < 1) // Si la carte SD est pleine (ou presque) on passe en mode erreur
  {
    mode = 5;
    erreur(150, 0, 0, 0, 150, 0, 2000);
  }
  if (SD.exists(work)) // Si le fichier existe déjà on l'ouvre
  {
    // Open file
    File dataFile = SD.open(work, FILE_WRITE);

    // Check if file size is too large
    if (dataFile.size() >= 100) // Si le fichier dépasse la taille maximale
    {
      // Close file
      dataFile.close();

      String fileName = nom(rev); // Création du nom du nouveau fichier
      while (SD.exists(fileName)) // Tant que le fichier existe
      {
        rev += 1;            // Incrémentation du numéro de révision
        fileName = nom(rev); // Création du nom du nouveau fichier (le fichier va être nommé avec le numéro de révision le plus proche du précédent plus élevé disponible)
      };

      SD.rename(work, fileName); // Renommage du fichier
      goto ecriture;             // On crée un nouveau fichier d'écriture avec le bon nom
    }
    else // Si le fichier n'est pas trop gros, on écrit dedans les données mesurées
    {
      prnt(dataFile);
      dataFile.close();
    }
  }
  else
  {
  ecriture:
    File newDataFile = SD.open(work, FILE_WRITE);                                           // Création du fichier
    newDataFile.println(F("Temps ; GPS ; Luminosite ; Temperature ; Humidite ; Pression")); // Ecriture de l'entête
    prnt(newDataFile);                                                                      // Ecriture des données
    newDataFile.close();                                                                    // Fermeture du fichier
  }
}

void loop()
{
  unsigned long currentTime = millis(); // Récupération du temps actuel

  if (mode == 0 && currentTime - startTime >= 5000) // Si on est en mode initialisation et que le temps écoulé est supérieur à 5s
  {
    mode = 1;                       // On passe en mode standard
    leds.setColorRGB(0, 0, 150, 0); // On allume la led en vert
    startTime = currentTime;        // On réinitialise le temps de démarrage
  }
  else if ((mode == 1 || mode == 2) && currentTime - startTime >= 1000) // Si on est en mode standard ou eco et que le temps écoulé est supérieur à 1s
  {
    ecriture();              // On les écrit dans le fichier
    startTime = currentTime; // On réinitialise le temps de démarrage
  }
  else if (mode == 3 && currentTime - startTime >= 1000) // Si on est en mode config et que le temps écoulé est supérieur à 1s
  {
    startTime = currentTime;
  }
  else if (mode == 4 && currentTime - startTime >= 5000) // Si on est en mode maintenance et que le temps écoulé est suppérieur à 5s
  {

    climateSensor.takeForcedMeasurement();
    clock.getTime();
    Serial.print(String(clock.hour) + F(":") + String(clock.minute, DEC) + F(":") + String(clock.second, DEC)); // Ecriture de l'heure
    Serial.print(F(" ; "));                                                                                     // Ecriture du séparateur
    Serial.print(getGps());                                                                                     // Ecriture des données GPS
    Serial.print(F(" ; "));                                                                                     // Ecriture du séparateur
    Serial.print(getCapteur(LUMIN, analogRead(A0), LUMIN_LOW, LUMIN_HIGH));                                     // Ecriture de la luminosité
    Serial.print(F(" ; "));                                                                                     // Ecriture du séparateur
    Serial.print(getCapteur(TEMP_AIR, climateSensor.getTemperatureCelcius(), MIN_TEMP_AIR, MAX_TEMP_AIR));      // Ecriture de la température
    Serial.print(F(" ; "));                                                                                     // Ecriture du séparateur
    Serial.print(getCapteur(HYGR, climateSensor.getRelativeHumidity(), HYGR_MINT, HYGR_MAXT));                  // Ecriture de l'humidité
    Serial.print(F(" ; "));                                                                                     // Ecriture du séparateur
    Serial.println(getCapteur(PRESSURE, climateSensor.getPressure() / 100.0F, PRESSURE_MIN, PRESSURE_MAX));     // Ecriture de la pression
    startTime = currentTime;
  }
  delay(1);
}