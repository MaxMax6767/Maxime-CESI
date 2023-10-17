#include <SoftwareSerial.h>
#include <Wire.h>
#include "DS1307.h"
#include <Adafruit_BME280.h>
#include <ChainableLED.h>

DS1307 clock;
SoftwareSerial SoftSerial(0, 1);
bool t;
Adafruit_BME280 bme;
unsigned long startTime = millis();
bool lp = false;
bool initialisation = true;
ChainableLED leds(7, 8, 1);

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

byte r = 2;
byte g = 3;

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
  int i = 0;
  while (digitalRead(g) == LOW)
  {
    i += 1;
    delay(1);
    if (i >= 5000)
    {
      if (mode == 1)
      {
        mode = 2;
        leds.setColorRGB(0, 0, 0, 150);
      }
      else
      {
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
          modep = mode;
          mode = 4;
          leds.setColorRGB(0, 150, 150, 0);
          while (digitalRead(r) == LOW)
            ;
          break;
        }
        else
        {
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
  Serial.begin(9600);
  Wire.begin();
  leds.init();
  SoftSerial.begin(9600);

  clock.begin();
  clock.fillByYMD(2023, 10, 16);
  clock.fillByHMS(15, 00, 00);
  clock.fillDayOfWeek(SAT);
  clock.setTime();

  attachInterrupt(digitalPinToInterrupt(g), switchg, LOW);
  attachInterrupt(digitalPinToInterrupt(r), switchr, LOW);

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

void Lecture(bool e)
{
  static bool gps;
  clock.getTime();
  Mesures->temps = String(clock.hour, DEC) + ":" + String(clock.minute, DEC) + ":" + String(clock.second, DEC);
  if (!e)
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
  Mesures->luminosite = analogRead(A0);
  Mesures->temperature = bme.readTemperature();
  Mesures->humidite = bme.readHumidity();
  Mesures->pressure = bme.readPressure() / 100.0F;
}

void erreur(int R, int G, int B, int R2, int G2, int B2, int d)
{
  leds.setColorRGB(0, R, G, B);
  delay(1000);
  leds.setColorRGB(0, R2, G2, B2);
  delay(d);
}

void affichage(bool e)
{
  Lecture(e);
  Serial.println(Mesures->temps);
  Serial.println(Mesures->gps);
  Serial.println(Mesures->luminosite);
  Serial.println(Mesures->temperature);
  Serial.println(Mesures->humidite);
  Serial.println(Mesures->pressure);
  Serial.println();
}

void loop()
{
  unsigned long currentTime = millis();

  switch (mode)
  {
  case 0:
    if (currentTime - startTime >= 10000)
    {
      Serial.println("init");
      initialisation = false;
      mode = 1;
      leds.setColorRGB(0, 0, 150, 0);
      startTime = currentTime;
    }
    break;
  case 1:
    if (currentTime - startTime >= 1000)
    {
      Serial.println("Normal");
      // affichage(true);
      startTime = currentTime;
    }
    break;
  case 2:
    if (currentTime - startTime >= 1000)
    {
      Serial.println("Eco");
      // affichage(false);
      startTime = currentTime;
    }
    break;
  case 3:
    if (currentTime - startTime >= 1000)
    {
      Serial.println("Config");
      startTime = currentTime;
    }
    break;
  case 4:
    if (currentTime - startTime >= 1000)
    {
      Serial.println("Maintenance");
      startTime = currentTime;
    }
    break;
  }

  delay(1);
}
