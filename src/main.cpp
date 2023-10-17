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
unsigned long timerb1 = 0;
ChainableLED leds(7, 8, 1);
int mode = 0;

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

void switch1()
{
  static bool b1 = false;
  unsigned long s;
  unsigned long f;
  if (b1)
  {
    s = millis();
    b1 = false;
  }
  else if (!b1)
  {
    f = millis();
    b1 = true;
    if (f - s >= 5000)
    {
      s = f;
      if (mode == 0)
      {
        mode = 1;
        leds.setColorRGB(0, 0, 0, 150);
      }
      else
      {
        mode = 0;
        leds.setColorRGB(0, 0, 150, 0);
      }
    }
  }
  Serial.println(s);
  Serial.println(f);
  Serial.println(f - s);
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

  attachInterrupt(digitalPinToInterrupt(2), switch1, CHANGE);

  leds.setColorRGB(0, 0, 150, 0);
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
    if (currentTime - startTime >= 1000)
    {
      Serial.println("normal");
      // affichage(false);
      startTime = currentTime;
    }
    break;
  case 1:
    if (currentTime - startTime >= 1000)
    {
      Serial.println("eco");
      // affichage(true);
      startTime = currentTime;
    }
    break;
  }

  delay(1);
}
