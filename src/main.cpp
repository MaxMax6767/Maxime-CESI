#include <SoftwareSerial.h>
#include <Wire.h>
#include "DS1307.h"
#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <SdFat.h>

SdFat SD;
DS1307 clock;
SoftwareSerial SoftSerial(0, 1);
Adafruit_BME280 bme;
ChainableLED leds(7, 8, 1);
bool t;
unsigned long startTime = millis();
bool lp = false;
bool initialisation = true;
byte r = 2;
byte g = 3;
String sep = " ; ";
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
unsigned long LOG_INTERVAL = 10;
uint32_t FILE_MAX_SIZE = 4096;
int TIMEOUT = 30;

struct structure
{
  String temps;
  String gps;
  String luminosite;
  String temperature;
  String humidite;
  String pressure;
};
structure *Mesures = new structure();

void switchg()
{
  // Bascule mode Eco / Normal si appui long sur le bouton vert
  int i = 0;
  while (digitalRead(g) == LOW)
  {
    i += 1;
    delay(1);
    if (i >= 5000)
    {
      if (mode == 1)
      {
        // Son viens du mode normal, on passe en mode Eco
        mode = 2;
        leds.setColorRGB(0, 0, 0, 150);
      }
      else if (mode == 2)
      {
        // Son viens du mode Eco, on passe en mode Normal
        mode = 1;
        leds.setColorRGB(0, 0, 150, 0);
      }
      break;
    }
  }
}

void switchr()
{
  static int modep;
  int i = 0;
  while (digitalRead(r) == LOW)
  {
    i += 1;
    delay(1);
    if (i >= 5000)
    {
      if (initialisation)
      {
        // Si on est en mode initialisation et appui long sur le bouton rouge, on passe en mode config
        if (mode == 0)
        {
          mode = 3;
          leds.setColorRGB(0, 150, 0, 0);
          while (digitalRead(r) == LOW)
            ;
          break;
        }
        else
        {
          // Si on est en mode config et appui long sur le bouton rouge, on passe en mode normal
          initialisation = false;
          mode = 1;
          leds.setColorRGB(0, 0, 150, 0);
          while (digitalRead(r) == LOW)
            ;
          break;
        }
      }
      else
      {
        if (mode == 1 || mode == 2)
        {
          // Si on est en mode Eco ou Standard et que le bouton rouge est appuyé longtemps, on passe en mode maintenance
          modep = mode;
          mode = 4;
          leds.setColorRGB(0, 150, 150, 0);
          while (digitalRead(r) == LOW)
            ;
          break;
        }
        else
        {
          // Si on est en mode maintenance et que le bouton rouge est appuyé longtemps, on reviens au mode précédent
          mode = modep;
          if (mode == 1)
          {
            leds.setColorRGB(0, 0, 150, 0);
            while (digitalRead(r) == LOW)
              ;
            break;
          }
          else
          {
            leds.setColorRGB(0, 0, 0, 150);
            while (digitalRead(r) == LOW)
              ;
            break;
          }
        }
      }
    }
  }
}

void setup()
{
  // Initialisation de la communication serie, I2C, SPI et les LEDS
  Serial.begin(9600);
  Wire.begin();
  leds.init();
  SoftSerial.begin(9600);
  bme.begin(0x76);
  SD.begin(4);

  // Configuration de l'Horloge
  clock.begin();
  clock.fillByYMD(2023, 10, 16);
  clock.fillByHMS(15, 00, 00);
  clock.fillDayOfWeek(SAT);
  clock.setTime();

  // Définition des interruptions de changement de mode.
  attachInterrupt(digitalPinToInterrupt(g), switchg, LOW);
  attachInterrupt(digitalPinToInterrupt(r), switchr, LOW);

  // Initialisation de la LED en blanc pour le mode initialisation
  leds.setColorRGB(0, 150, 150, 150);
}

String getGps()
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

void erreur(int R, int G, int B, int R2, int G2, int B2, int d)
{
  while (true)
  {
    leds.setColorRGB(0, R, G, B);
    delay(1000);
    leds.setColorRGB(0, R2, G2, B2);
    delay(d);
  }
}

void getCapteur(bool o, float m, int l, int h, String n)
{
  if (o)
  {
    if (m < l || m > h)
    {
      n = "";
      mode = 7;
      erreur(150, 0, 0, 0, 150, 0, 2000);
    }
    else
    {
      n = String(m);
    }
  }
  else
  {
    n = "";
  }
}

void Lecture()
{
  static bool gps;

  // Lecture de l'heure (tout le temps)
  clock.getTime();
  Mesures->temps = String(clock.hour, DEC) + ":" + String(clock.minute, DEC) + ":" + String(clock.second, DEC);

  // Lecture du GPS (en fonction du mode Eco)
  if (mode != 2)
  {
    Mesures->gps = getGps();
  }
  else
  {
    if (gps)
    {
      Mesures->gps = getGps();
      gps = false;
    }
    else
    {
      Mesures->gps = "";
      gps = true;
    }
  }

  // Lecture de la luminosite (si activé)
  getCapteur(LUMIN, analogRead(A0), LUMIN_LOW, LUMIN_HIGH, Mesures->luminosite);

  // Lecture de la température de l'air (si activé)
  getCapteur(TEMP_AIR, bme.readTemperature(), MIN_TEMP_AIR, MAX_TEMP_AIR, Mesures->temperature);

  // Lecture de l'humidité (si activé)
  getCapteur(HYGR, bme.readHumidity(), HYGR_MINT, HYGR_MAXT, Mesures->humidite);

  // Lecture de la pression (si activé)
  getCapteur(PRESSURE, bme.readPressure() / 100.0F, PRESSURE_MIN, PRESSURE_MAX, Mesures->pressure);
}

void ecriture()
{
  static int rev = 0;
  String ecriture = String(clock.year, DEC) + String(clock.month, DEC) + String(clock.dayOfMonth, DEC) + "_0.log";

  // Verif de l'espace libre sur la SD
  if (SD.freeClusterCount() < 1)
  {
    mode = 5;
    erreur(150, 0, 0, 0, 150, 0, 2000);
  }
  else
  {
    // Verification de si le fichier d'écriture existe
    if (SD.exists(ecriture))
    {
      // Ouverture du fichier
      File dataFile = SD.open(ecriture, FILE_WRITE);
      // Verification de si le fichier d'écriture dépasse la taille max
      if (dataFile.size() >= FILE_MAX_SIZE)
      {
        // Fermeture du fichier
        dataFile.close();
        String fileName = String(clock.year, DEC) + String(clock.month, DEC) + String(clock.dayOfMonth, DEC) + "_" + String(rev) + ".log";
        // Verification du fichier dont le numero de revision est le plus grand sur la SD
        while (SD.exists(fileName))
        {
          rev += 1;
          fileName = String(clock.year, DEC) + String(clock.month, DEC) + String(clock.dayOfMonth, DEC) + "_" + String(rev) + ".log";
        }
        // Renommer le fichier d'écriture avec le numero de revision le plus grand
        SD.rename(ecriture, fileName);
        // Création & ouverture du fichier d'écriture
        File dataFile = SD.open(ecriture, FILE_WRITE);
        // Ecriture de l'entete du fichier
        dataFile.println("Temps ; GPS ; Luminosite ; Temperature ; Humidite ; Pression");
        // Ecriture des données
        dataFile.println(Mesures->temps + sep + Mesures->gps + sep + Mesures->luminosite + sep + Mesures->temperature + sep + Mesures->humidite + sep + Mesures->pressure);
        // Fermeture du fichier
        dataFile.close();
      }
    }
    else
    {
      // Ouverture du fichier
      File dataFile = SD.open(ecriture, FILE_WRITE);
      // Ecriture de l'entete du fichier
      dataFile.println("Temps ; GPS ; Luminosite ; Temperature ; Humidite ; Pression");
      // Ecriture des données
      dataFile.println(Mesures->temps + sep + Mesures->gps + sep + Mesures->luminosite + sep + Mesures->temperature + sep + Mesures->humidite + sep + Mesures->pressure);
      // Fermeture du fichier
      dataFile.close();
    }
  }
}

void loop()
{
  unsigned long currentTime = millis();

  switch (mode)
  {
  case 0:
    if (currentTime - startTime >= 10000)
    {
      Serial.println("### Initialisation ###");
      initialisation = false;
      mode = 1;
      leds.setColorRGB(0, 0, 150, 0);
      startTime = currentTime;
    }
    break;
  case 1:
    if (currentTime - startTime >= 5000)
    {
      Lecture();
      ecriture();
      startTime = currentTime;
    }
    break;
  case 2:
    if (currentTime - startTime >= 5000)
    {
      Lecture();
      ecriture();
      startTime = currentTime;
    }
    break;
  case 3:
    if (currentTime - startTime >= 5000)
    {
      Serial.println("### Config ###");
      startTime = currentTime;
    }
    break;
  case 4:
    if (currentTime - startTime >= 5000)
    {
      Serial.println("### Maintenance ###");
      startTime = currentTime;
    }
    break;
  }
  delay(1);
}
