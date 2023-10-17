#include <SoftwareSerial.h>
#include <Wire.h>
#include "DS1307.h"
#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <SdFat.h>

SdFat SD;
DS1307 clock;
SoftwareSerial SoftSerial(0, 1);
bool t;
Adafruit_BME280 bme;
unsigned long startTime = millis();
bool lp = false;
bool initialisation = true;
ChainableLED leds(7, 8, 1);
byte r = 2;
byte g = 3;

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
  leds.setColorRGB(0, R, G, B);
  delay(1000);
  leds.setColorRGB(0, R2, G2, B2);
  delay(d);
}

void Lecture()
{
  static bool gps;
  clock.getTime();
  Mesures->temps = String(clock.hour, DEC) + ":" + String(clock.minute, DEC) + ":" + String(clock.second, DEC);
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
  if (LUMIN)
  {
    Mesures->luminosite = String(analogRead(A0));
  }
  else
  {
    Mesures->luminosite = "";
  }
  if (TEMP_AIR)
  {
    // Lecture de la température en °C (la valeur est un float)
    Mesures->temperature = String(bme.readTemperature());
  }
  else
  {
    Mesures->temperature = "";
  }
  if (HYGR)
  {
    float t = bme.readTemperature();
    Mesures->humidite = String(bme.readHumidity());
    if (t < HYGR_MINT || t > HYGR_MAXT)
    {
      Mesures->humidite = "";
    }
    mode = 7;
    erreur(150, 0, 0, 0, 150, 0, 2000);
  }
  else
  {
    Mesures->humidite = "";
  }
  if (PRESSURE)
  {
    float p = bme.readPressure() / 100.0F;
    Mesures->pressure = String(p);
    if (p < PRESSURE_MIN || p > PRESSURE_MAX)
    {
      Mesures->pressure = "";
    }
    mode = 7;
    erreur(150, 0, 0, 0, 150, 0, 2000);
  }
  else
  {
    Mesures->pressure = "";
  }
  Mesures->pressure = bme.readPressure() / 100.0F;
}

void affichage()
{
  Lecture();
  Serial.print(Mesures->temps);
  Serial.print(" ; ");
  Serial.print(Mesures->gps);
  Serial.print(" ; ");
  Serial.print(Mesures->luminosite);
  Serial.print(" ; ");
  Serial.print(Mesures->temperature);
  Serial.print(" ; ");
  Serial.print(Mesures->humidite);
  Serial.print(" ; ");
  Serial.print(Mesures->pressure);
  Serial.println(" ");
}

void write()
{
  static int rev = 0;
  filename =
      File dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(Mesures->temps);
    dataFile.print(" ; ");
    dataFile.print(Mesures->gps);
    dataFile.print(" ; ");
    dataFile.print(Mesures->luminosite);
    dataFile.print(" ; ");
    dataFile.print(Mesures->temperature);
    dataFile.print(" ; ");
    dataFile.print(Mesures->humidite);
    dataFile.print(" ; ");
    dataFile.print(Mesures->pressure);
    dataFile.println(" ");
    dataFile.close();
  }
  else
  {
    erreur(150, 0, 0, 0, 0, 0, 1000);
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
      affichage();
      startTime = currentTime;
    }
    break;
  case 2:
    if (currentTime - startTime >= 5000)
    {
      affichage();
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
