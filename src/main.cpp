#include <SoftwareSerial.h> // Librairie pour le port série du GPS
#include <Wire.h>           // Librairie pour le bus I2C
#include "DS1307.h"         // Librairie pour le module RTC
#include <forcedClimate.h>  // Librairie pour le capteur BME280
#include <ChainableLED.h>   // Librairie pour la LED
#include <SdFat.h>          // Librairie pour la carte SD
#include <EEPROM.h>         // Librairie pour l'EEPROM

SdFat SD;                                      // SD card library
SoftwareSerial SoftSerial(8, 9);               // RX, TX
ForcedClimate climateSensor = ForcedClimate(); // BME280 library
ChainableLED leds(6, 7, 1);                    // LED library
DS1307 clock;                                  // RTC library

unsigned long startTime = millis(); // Temps de démarrage du programme
const char sep1 PROGMEM = ';';      // Séparateur de données
const char sep2 PROGMEM = ':';      // Séparateur d'heure
File dataFile;                      // Fichier de données

typedef enum
{
  initialisation,
  normal,
  eco,
  config,
  maintenance,
  erreur_sd,
  erreur_enregistrement,
  erreur_valeur,
  erreur_gps,
  erreur_capteur
} modes;

int erreurs[5] = {0, 0, 0, 0, 0};
/*
+----------+---------------------------------+
| Position | Type d'erreur                   |
+----------+---------------------------------+
| 0        | Erreur SD pleine                |
+----------+---------------------------------+
| 1        | Erreur lors de l'enregistrement |
+----------+---------------------------------+
| 2        | Erreur, valeur hors bornes      |
+----------+---------------------------------+
| 3        | Erreur d'accès au GPS           |
+----------+---------------------------------+
| 4        | Erreur d'accès à un capteur     |
+----------+---------------------------------+
*/

modes mode = initialisation; // Mode de fonctionnement

bool LUMIN = true;
int LUMIN_LOW = 0;
int LUMIN_HIGH = 1023;
bool TEMP_AIR = true;
int MIN_TEMP_AIR = -40;
int MAX_TEMP_AIR = 85;
bool HYGR = true;
int HYGR_MINT = -40;
int HYGR_MAXT = 85;
bool PRESSURE = true;
int PRESSURE_MIN = 300;
int PRESSURE_MAX = 1100;
unsigned long LOG_INTERVAL = 10;
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

void switchg()
{
  static bool EtatBoutonVert = false;
  static unsigned long TempsBoutonVert = 0;

  if ((digitalRead(3) == LOW) && (!EtatBoutonVert))
  {
    EtatBoutonVert = true;
    TempsBoutonVert = millis();
  }
  else if ((digitalRead(3) == HIGH) && (EtatBoutonVert))
  {
    EtatBoutonVert = false;
    if ((millis() - TempsBoutonVert) >= (5000))
    {
      switch (mode)
      {
      case config:
      case maintenance:
      case erreur_sd:
      case erreur_enregistrement:
      case erreur_valeur:
      case erreur_gps:
      case erreur_capteur:
      case initialisation:
        break;
      case normal:
        mode = eco;
        leds.setColorRGB(0, 0, 0, 150);
        break;
      case eco:
        mode = normal;
        leds.setColorRGB(0, 0, 150, 0);
        break;
      }
    }
  }
}

void switchr()
{
  static modes modep;
  static bool EtatBoutonRouge = false;
  static unsigned long TempsBoutonRouge = 0;

  if ((digitalRead(2) == LOW) && (!EtatBoutonRouge))
  {
    EtatBoutonRouge = true;
    TempsBoutonRouge = millis();
  }

  else if (digitalRead(2) == HIGH && (EtatBoutonRouge))
  {
    EtatBoutonRouge = false;
    if ((millis() - TempsBoutonRouge) >= 5000)
    {
      TempsBoutonRouge = millis();
      switch (mode)
      {
      case erreur_sd:
      case erreur_enregistrement:
      case erreur_valeur:
      case erreur_gps:
      case erreur_capteur:
        break;
      case initialisation:
        mode = config;
        leds.setColorRGB(0, 150, 150, 0);
        break;
      case config:
        mode = normal;
        leds.setColorRGB(0, 0, 150, 0);
        break;
      case normal:
      case eco:
        modep = mode;
        mode = maintenance;
        leds.setColorRGB(0, 150, 0, 0);
        break;
      case maintenance:
        mode = modep;
        leds.setColorRGB(0, 0, (modep == normal) ? 150 : 0, (modep == normal) ? 0 : 150);
        break;
      }
    }
  }
}

void setup()
{
  Serial.begin(9600);     // Initialisation du port série
  leds.init();            // Initialisation de la LED
  SoftSerial.begin(9600); // Initialisation du port série du GPS
  climateSensor.begin();  // Initialisation du capteur BME280
  if (!SD.begin(4))       // Initialisation de la carte SD
  {
    Serial.println(F("Card failed, or not present"));
    while (1)
      ;
  }

  clock.begin();
  clock.fillByYMD(2023, 10, 27);
  clock.fillByHMS(atoi(__TIME__), atoi(__TIME__ + 3), atoi(__TIME__ + 6));
  clock.setTime(); // Écrire l'heure dans la puce RTC

  attachInterrupt(digitalPinToInterrupt(3), switchg, CHANGE); // Interruption sur le boutton vert
  attachInterrupt(digitalPinToInterrupt(2), switchr, CHANGE); // Interruption sur le boutton rouge

  leds.setColorRGB(0, 150, 150, 150); // LED blanche au démarrage
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

String getGps() // Fonction de récupération de la trame GGA
{
  static char gpsData[85]; // Tableau statique de caractères pour stocker les données GPS

  // Initialisation du tableau
  for (int i = 0; i < 85; i++)
  {
    gpsData[i] = '\0';
  }

  // Lecture des données GPS
  int i = 0;
  while (true)
  {
    // Si le port série du GPS est disponible, on a reçu un nouveau caractère
    if (SoftSerial.available())
    {
      // Lecture du caractère
      gpsData[i] = (char)SoftSerial.read();

      // Si le caractère est un retour à la ligne, on a terminé la lecture de la trame
      if (gpsData[i] == '\n')
      {
        // Vérification de la trame
        if (strncmp(gpsData, "$GPGGA", 6) == 0)
        {
          // Trame GGA trouvée, on retourne les données
          if (gpsData[19] == ',' && gpsData[20] == ',' && gpsData[21] == ',')
          {
            erreurs[2]++;
            return "";
          }
          return gpsData;
        }
        else
        {
          // Trame non valide, on réinitialise le tableau
          for (int j = 0; j < 85; j++)
          {
            gpsData[j] = '\0';
          }
          i = 0;
        }
      }
      else
      {
        // On incrémente l'index du tableau
        i++;
      }
    }
  }
}

String gpsHandler() // Fonction de récupération des données GPS
{
  static bool gps; // Variable de stockage de l'état de la mesure du GPS
  if (mode != 2)   // Si on est pas en mode Eco, on récupère les données GPS
  {
  mesure:
    String val = getGps();
    if (val != "")
    {
    }
    else
    {
      erreurs[3]++;
      delay(TIMEOUT * 1000);
      goto mesure;
    }
    return val;
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
      return F("");
    }
  }
}

float getCapteur(bool *o, float m, int *l, int *h) // Fonction de récupération des données des capteurs
{
  if (*o) // Si le capteur est activé
  {
    if (m < *l || m > *h) // Si la valeur est hors des limites
    {
      return NAN; // On retourne NAN
    }
    else
    {
      return m; // Sinon on retourne la valeur
    }
  }
  else
  {
    return NAN;
  }
}

char *nom(int i) // Fonction de création du nom du fichier
{
  static char buff[15];
  sprintf(buff, "%02d%02d%02d_%d.log", clock.year, clock.month, clock.dayOfMonth, i);
  return buff;
}

void prnt(bool m) // Fonction d'écriture dans le fichier
{
  // Acquisition des données
  climateSensor.takeForcedMeasurement();
  clock.getTime();

  if (m)
  {
    Serial.print(String(clock.hour) + sep2 + String(clock.minute) + sep2 + String(clock.second));             // Ecriture de l'heure
    Serial.print(sep1);                                                                                       // Ecriture du séparateur
    Serial.print(getGps());                                                                                   // Ecriture des données GPS
    Serial.print(sep1);                                                                                       // Ecriture du séparateur
    Serial.print(getCapteur(&LUMIN, analogRead(A0), &LUMIN_LOW, &LUMIN_HIGH));                                // Ecriture de la luminosité
    Serial.print(sep1);                                                                                       // Ecriture du séparateur
    Serial.print(getCapteur(&TEMP_AIR, climateSensor.getTemperatureCelcius(), &MIN_TEMP_AIR, &MAX_TEMP_AIR)); // Ecriture de la température
    Serial.print(sep1);                                                                                       // Ecriture du séparateur
    Serial.print(getCapteur(&HYGR, climateSensor.getRelativeHumidity(), &HYGR_MINT, &HYGR_MAXT));             // Ecriture de l'humidité
    Serial.print(sep1);                                                                                       // Ecriture du séparateur
    Serial.println(getCapteur(&PRESSURE, climateSensor.getPressure(), &PRESSURE_MIN, &PRESSURE_MAX));         // Ecriture de la pression
  }
  else
  {
    dataFile.print(String(clock.hour) + sep2 + String(clock.minute) + sep2 + String(clock.second));             // Ecriture de l'heure
    dataFile.print(sep1);                                                                                       // Ecriture du séparateur
    dataFile.print(getGps());                                                                                   // Ecriture des données GPS
    dataFile.print(sep1);                                                                                       // Ecriture du séparateur
    dataFile.print(getCapteur(&LUMIN, analogRead(A0), &LUMIN_LOW, &LUMIN_HIGH));                                // Ecriture de la luminosité
    dataFile.print(sep1);                                                                                       // Ecriture du séparateur
    dataFile.print(getCapteur(&TEMP_AIR, climateSensor.getTemperatureCelcius(), &MIN_TEMP_AIR, &MAX_TEMP_AIR)); // Ecriture de la température
    dataFile.print(sep1);                                                                                       // Ecriture du séparateur
    dataFile.print(getCapteur(&HYGR, climateSensor.getRelativeHumidity(), &HYGR_MINT, &HYGR_MAXT));             // Ecriture de l'humidité
    dataFile.print(sep1);                                                                                       // Ecriture du séparateur
    dataFile.println(getCapteur(&PRESSURE, climateSensor.getPressure(), &PRESSURE_MIN, &PRESSURE_MAX));         // Ecriture de la pression
    dataFile.flush();                                                                                           // Ecriture des données dans le fichier
    dataFile.close();                                                                                           // Fermeture du fichier
  }
}

void ecriture() // Fonction de gestion de l'écriture dans le fichier sur la carte SD
{
  clock.getTime();
  static int rev = 0;         // Variable de stockage du numéro de révision du fichier
  String work = nom(0);       // Variable de stockage du nom du fichier
  String fileName = nom(rev); // Création du nom du nouveau fichier

  if (SD.freeClusterCount() < 1) // Si la carte SD est pleine (ou presque) on passe en mode erreur
  {
    mode = erreur_sd;
    erreur(150, 0, 0, 0, 150, 0, 2000);
  }
  if (SD.exists(work)) // Si le fichier existe déjà on l'ouvre
  {
    // Open file
    dataFile = SD.open(work, FILE_WRITE);

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
      Serial.println(fileName);
      SD.rename(work, fileName); // Renommage du fichier
      goto ecriture;             // On crée un nouveau fichier d'écriture avec le bon nom
    }
    else // Si le fichier n'est pas trop gros, on écrit dedans les données mesurées
    {
      prnt(false);
    }
  }
  else
  {
  ecriture:
    dataFile = SD.open(work, FILE_WRITE);                                      // Création du fichier
    dataFile.println(F("Temps;GPS;Luminosite;Temperature;Humidite;Pression")); // Ecriture de l'entête
    prnt(false);                                                               // Ecriture des données                                                                   // Fermeture du fichier
  }
}

void executeCommand(String command, int argument)
{
  String Tlumin = F("LUMIN");
  String Tlumin_low = F("LUMIN_LOW");
  String Tlumin_high = F("LUMIN_HIGH");
  String Ttemp_air = F("TEMP_AIR");
  String Tmin_temp_air = F("MIN_TEMP_AIR");
  String Tmax_temp_air = F("MAX_TEMP_AIR");
  String Thygr = F("HYGR");
  String Thygr_mint = F("HYGR_MINT");
  String Thygr_maxt = F("HYGR_MAXT");
  String Tpressure = F("PRESSURE");
  String Tpressure_min = F("PRESSURE_MIN");
  String Tpressure_max = F("PRESSURE_MAX");
  String Tlog_interval = F("LOG_INTERVAL");
  String Tfile_max_size = F("FILE_MAX_SIZE");
  String Ttimeout = F("TIMEOUT");
  String Tvariable = F("Variable ");
  String Tmodifiee = F(" modifiee : ");
  String Terreur = F("Erreur (valeur non valide ou hors limite)");

  // Utilisez un switch case pour gérer différentes commandes
  if (command == F("RESET"))
  {
    // Reset des variables en RAM
    EEPROM.get(0, LUMIN);
    EEPROM.get(1, LUMIN_LOW);
    EEPROM.get(3, LUMIN_HIGH);
    EEPROM.get(5, TEMP_AIR);
    EEPROM.get(6, MIN_TEMP_AIR);
    EEPROM.get(8, MAX_TEMP_AIR);
    EEPROM.get(10, HYGR);
    EEPROM.get(11, HYGR_MINT);
    EEPROM.get(13, HYGR_MAXT);
    EEPROM.get(15, PRESSURE);
    EEPROM.get(16, PRESSURE_MIN);
    EEPROM.get(18, PRESSURE_MAX);
    EEPROM.get(20, LOG_INTERVAL);
    EEPROM.get(24, FILE_MAX_SIZE);
    EEPROM.get(28, TIMEOUT);

    // Affichage utilisateur
    Serial.println(F("Reset Effectue"));
  }
  else if (command == Tlumin || command == Ttemp_air || command == Thygr || command == Tpressure)
  {
    int address = (command == Tlumin) ? 0 : ((command == Ttemp_air) ? 5 : ((command == Thygr) ? 10 : 15));
    if (argument == 0 || argument == 1)
    {
      EEPROM.put(address, argument);
      Serial.print(Tvariable);
      Serial.print(command);
      Serial.print(Tmodifiee);
      Serial.println(argument);
    }
    else
    {
      Serial.println(Terreur);
    }
  }
  else if (command == Tlumin_low || command == Tlumin_high || command == Tmin_temp_air || command == Tmax_temp_air || command == Thygr_mint || command == Thygr_maxt || command == Tpressure_min || command == Tpressure_max)
  {
    int address = (command == Tlumin_low) ? 1 : ((command == Tlumin_high) ? 3 : ((command == Tmin_temp_air) ? 6 : ((command == Tmax_temp_air) ? 8 : ((command == Thygr_mint) ? 11 : ((command == Thygr_maxt) ? 13 : ((command == Tpressure_min) ? 16 : 18))))));
    int minVal = (command == Tlumin_low) ? 0 : ((command == Tmin_temp_air || command == Thygr_mint || command == Tpressure_min) ? -40 : 300);
    int maxVal = (command == Tlumin_high) ? 1023 : ((command == Tmax_temp_air || command == Thygr_mint || command == Tpressure_max) ? 85 : 1100);
    if (argument >= minVal && argument <= maxVal)
    {
      EEPROM.put(address, argument);
      Serial.print(Tvariable);
      Serial.print(command);
      Serial.print(Tmodifiee);
      Serial.println(argument);
    }
    else
    {
      Serial.println(Terreur);
    }
  }
  else if (command == Tlog_interval || command == Tfile_max_size || command == Ttimeout)
  {
    int address = (command == Tlog_interval) ? 20 : ((command == Tfile_max_size) ? 24 : 28);
    if (argument > 0)
    {
      EEPROM.put(address, argument);
      Serial.print(Tvariable);
      Serial.print(command);
      Serial.print(Tmodifiee);
      Serial.println(argument);
    }
    else
    {
      Serial.println(Terreur);
    }
  }
  else
  {
    // Commande inconnue
    Serial.print(F("Commande inconnue : "));
    Serial.print(command);
  }
}

void loop()
{
  unsigned long currentTime = millis(); // Récupération du temps actuel

  switch (mode)
  {
  case initialisation:
    if ((currentTime - startTime) >= 5000)
    {
      mode = normal;
      leds.setColorRGB(0, 0, 150, 0);
      startTime = currentTime;
    }
    break;
  case normal:
    if ((currentTime - startTime) >= 5000)
    {
      ecriture();              // On les écrit dans le fichier
      startTime = currentTime; // On réinitialise le temps de démarrage
    }
    break;
  case eco:
    if ((currentTime - startTime) >= 5000)
    {
      ecriture();              // On les écrit dans le fichier
      startTime = currentTime; // On réinitialise le temps de démarrage
    }
    break;
  case config:
    if ((currentTime - startTime) >= 50)
    {
      if (Serial.available() > 0)
      {
        // Si des données sont disponibles sur la liaison série

        String input = Serial.readStringUntil('\n'); // Lire la ligne de commande
        if (input.length() > 0)
        {
          // Vérifier si la commande est non vide
          int equalsIndex = input.indexOf('=');
          String command;
          int argument = 0;
          if (equalsIndex != -1)
          {
            // Si le caractère '=' est présent dans la commande
            command = input.substring(0, equalsIndex);
            argument = input.substring(equalsIndex + 1).toInt();
          }
          else
          {
            // Si la commande ne contient pas d'argument
            command = input;
          }

          // Exécutez la commande avec l'argument
          executeCommand(command, argument);
        }
      }
      startTime = currentTime; // On réinitialise le temps de démarrage
    }
    break;
  case maintenance:
    if ((currentTime - startTime) >= 5000)
    {
      prnt(true);
    }
    break;
  case erreur_sd:
    erreur(150, 0, 0, 0, 150, 0, 2000);
    break;
  case erreur_enregistrement:
    erreur(150, 0, 0, 0, 150, 0, 2000);
    break;
  case erreur_valeur:
    erreur(150, 0, 0, 0, 150, 0, 2000);
    break;
  case erreur_gps:
    erreur(150, 0, 0, 0, 150, 0, 2000);
    break;
  case erreur_capteur:
    erreur(150, 0, 0, 0, 150, 0, 2000);
    break;
  }
}