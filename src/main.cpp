#include <AltSoftSerial.h> // Librairie pour le port série du GPS
#include <RTClib.h>        // Librairie pour le module RTC
#include <forcedClimate.h> // Librairie pour le capteur BME280
#include <ChainableLED.h>  // Librairie pour la LED
#include <SdFat.h>         // Librairie pour la carte SD
#include <EEPROM.h>        // Librairie pour l'EEPROM

SdFat SD;                                      // SD card library
AltSoftSerial gpsSerial(8, 9);                 // GPS library
ForcedClimate climateSensor = ForcedClimate(); // BME280 library
ChainableLED leds(7, 8, 1);                    // LED library
RTC_DS1307 rtc;                                // Utilisation du module DS1307 avec RTCLib

unsigned long startTime = millis(); // Temps de démarrage du programme

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

modes mode = initialisation; // Mode de fonctionnement

bool LUMIN;
int LUMIN_LOW;
int LUMIN_HIGH;
bool TEMP_AIR;
int MIN_TEMP_AIR;
int MAX_TEMP_AIR;
bool HYGR;
int HYGR_MINT;
int HYGR_MAXT;
bool PRESSURE;
int PRESSURE_MIN;
int PRESSURE_MAX;
unsigned long LOG_INTERVAL;
uint32_t FILE_MAX_SIZE;
int TIMEOUT;
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

void switchr() // Fonction de changement de mode quand appui sur le boutton rouge
{
  modes modep;
  int i = 0;
  while (digitalRead(2) == LOW) // Tant que le boutton est appuyé
  {
    i++;
    delayMicroseconds(1000); // Attente de 1ms (en microsecondes car delay() ne fonctionne pas dans une interruption)
    if (i >= 5000)           // Si i est supérieur à 5000 (5s) alors on change de mode
    {
      i = 0;
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
  Serial.begin(9600);    // Initialisation du port série
  gpsSerial.begin(9600); // Initialisation du port série du GPS
  leds.init();           // Initialisation de la LED
  climateSensor.begin(); // Initialisation du capteur BME280
  if (!SD.begin(4))      // Initialisation de la carte SD
  {
    Serial.println(F("Card failed, or not present"));
    while (1)
      ;
  }
  if (!rtc.begin())
  { // Initialisation du module RTC
    Serial.println(F("RTC not found"));
    while (1)
      ;
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  attachInterrupt(digitalPinToInterrupt(3), switchg, LOW); // Interruption sur le boutton vert
  attachInterrupt(digitalPinToInterrupt(2), switchr, LOW); // Interruption sur le boutton rouge

  // Lecture de la config depuis l'EEPROM
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

  // Ecriture de la config par défaut dans L'EEPROM
  EEPROM.put(30, 1);
  EEPROM.put(31, 255);
  EEPROM.put(33, 768);
  EEPROM.put(35, 1);
  EEPROM.put(36, -10);
  EEPROM.put(38, 60);
  EEPROM.put(40, 1);
  EEPROM.put(41, 0);
  EEPROM.put(43, 50);
  EEPROM.put(45, 1);
  EEPROM.put(46, 850);
  EEPROM.put(48, 1080);
  EEPROM.put(50, 10);
  EEPROM.put(54, 4096);
  EEPROM.put(58, 30);

  leds.setColorRGB(0, 150, 150, 150); // LED blanche au démarrage
}

String getGps() // Fonction de récupération des données GPS
{
  static bool gps; // Variable de stockage de l'état de la mesure du GPS
  if (mode != 2)   // Si on est pas en mode Eco, on récupère les données GPS
  {
  mesure:
    String trameGGA = "";

    // Boucle de lecture des données GPS (On attends d'avoir la tramme GGA)
    while (trameGGA.indexOf("$GPGGA") == -1)
    {
      // Lecture d'une ligne de données GPS
      String trameGGA = gpsSerial.readStringUntil('\n');

      // Si la ligne est valide, on la parse
      if (trameGGA.startsWith("$GPGGA"))
      {
        return trameGGA;
      }
    }
    // Retour de la trame de sortie
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
  return String(rtc.now().year()).substring(2) + String(rtc.now().month()) + String(rtc.now().day()) + F("_") + String(i) + F(".log"); // Retourne le nom du fichier
}

void prnt(File f) // Fonction d'écriture dans le fichier
{
  climateSensor.takeForcedMeasurement();
  f.print(String(rtc.now().hour()) + F(":") + String(rtc.now().minute()) + F(":") + String(rtc.now().second())); // Ecriture de l'heure
  f.print(F(" ; "));                                                                                             // Ecriture du séparateur
  f.print(getGps());                                                                                             // Ecriture des données GPS
  f.print(F(" ; "));                                                                                             // Ecriture du séparateur
  f.print(getCapteur(LUMIN, analogRead(A0), LUMIN_LOW, LUMIN_HIGH));                                             // Ecriture de la luminosité
  f.print(F(" ; "));                                                                                             // Ecriture du séparateur
  f.print(getCapteur(TEMP_AIR, climateSensor.getTemperatureCelcius(), MIN_TEMP_AIR, MAX_TEMP_AIR));              // Ecriture de la température
  f.print(F(" ; "));                                                                                             // Ecriture du séparateur
  f.print(getCapteur(HYGR, climateSensor.getRelativeHumidity(), HYGR_MINT, HYGR_MAXT));                          // Ecriture de l'humidité
  f.print(F(" ; "));                                                                                             // Ecriture du séparateur
  f.println(getCapteur(PRESSURE, climateSensor.getPressure(), PRESSURE_MIN, PRESSURE_MAX));                      // Ecriture de la pression
  f.close();                                                                                                     // Fermeture du fichier
}

void ecriture() // Fonction de gestion de l'écriture dans le fichier sur la carte SD
{
  static int rev = 0;          // Variable de stockage du numéro de révision du fichier
  static String work = nom(0); // Variable de stockage du nom du fichier

  if (SD.freeClusterCount() < 1) // Si la carte SD est pleine (ou presque) on passe en mode erreur
  {
    mode = erreur_sd;
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

void executeCommand(String command, int argument)
{
  // Utilisez un switch case pour gérer différentes commandes
  if (command == "RESET")
  {
    // Reset des variables en RAM
    int addresses[] = {30, 31, 33, 35, 36, 38, 40, 54, 58};
    int values[] = {0, 0, 0, 0, 0, 0, 0, 2000, 0};
    for (int i = 0; i < 9; i++)
    {
      EEPROM.get(addresses[i], values[i]);
      EEPROM.put(addresses[i], values[i]);
    }

    // Affichage utilisateur
    Serial.println(F("Reset Effectue"));
  }
  else if (command == "LUMIN")
  {
    if (argument == 0 || argument == 1)
    {
      EEPROM.put(0, argument);
      Serial.print(F("Variable LUMIN modifiee : "));
      Serial.println(argument);
    }
    else
    {
      Serial.println(F("Erreur : Valeur incorrecte (Pas d'argument ou argument incorrect)"));
    }
  }
  else if (command == "LUMIN_LOW" || command == "LUMIN_HIGH")
  {
    int address = (command == "LUMIN_LOW") ? 1 : 3;
    if (argument >= 0 && argument <= 1023)
    {
      EEPROM.put(address, argument);
      Serial.print(F("Variable "));
      Serial.print(command);
      Serial.print(F(" modifiee : "));
      Serial.println(argument);
    }
    else
    {
      Serial.println(F("Erreur : Valeur incorrecte (Pas d'argument ou argument incorrect)"));
    }
  }
  else if (command == "TEMP_AIR" || command == "HYGR" || command == "PRESSURE")
  {
    int address = (command == "TEMP_AIR") ? 5 : ((command == "HYGR") ? 10 : 15);
    if (argument == 0 || argument == 1)
    {
      EEPROM.put(address, argument);
      Serial.print(F("Variable "));
      Serial.print(command);
      Serial.print(F(" modifiee : "));
      Serial.println(argument);
    }
    else
    {
      Serial.println(F("Erreur : Valeur incorrecte (Pas d'argument ou argument incorrect)"));
    }
  }
  else if (command == "MIN_TEMP_AIR" || command == "MAX_TEMP_AIR" || command == "HYGR_MINT" || command == "HYGR_MAXT" || command == "PRESSURE_MIN" || command == "PRESSURE_MAX")
  {
    int address = (command == "MIN_TEMP_AIR") ? 6 : ((command == "MAX_TEMP_AIR") ? 8 : ((command == "HYGR_MINT") ? 11 : ((command == "HYGR_MAXT") ? 13 : ((command == "PRESSURE_MIN") ? 16 : 18))));
    int minVal = (command == "MIN_TEMP_AIR" || command == "HYGR_MINT" || command == "PRESSURE_MIN") ? -40 : 300;
    int maxVal = (command == "MAX_TEMP_AIR" || command == "HYGR_MAXT" || command == "PRESSURE_MAX") ? 85 : 1100;
    if (argument >= minVal && argument <= maxVal)
    {
      EEPROM.put(address, argument);
      Serial.print(F("Variable "));
      Serial.print(command);
      Serial.print(F(" modifiee : "));
      Serial.println(argument);
    }
    else
    {
      Serial.println(F("Erreur : Valeur incorrecte (Pas d'argument ou argument incorrect)"));
    }
  }
  else if (command == "LOG_INTERVAL" || command == "FILE_MAX_SIZE" || command == "TIMEOUT")
  {
    int address = (command == "LOG_INTERVAL") ? 20 : ((command == "FILE_MAX_SIZE") ? 24 : 28);
    if (argument > 0)
    {
      EEPROM.put(address, argument);
      Serial.print(F("Variable "));
      Serial.print(command);
      Serial.print(F(" modifiee : "));
      Serial.println(argument);
    }
    else
    {
      Serial.println(F("Erreur : Valeur incorrecte (Argument incorrect)"));
    }
  }
  else
  {
    // Commande inconnue
    Serial.print("Commande inconnue : ");
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
      ecriture(); // On les écrit dans le fichier
      Serial.println("Normal");
      startTime = currentTime; // On réinitialise le temps de démarrage
    }
    break;
  case eco:
    if ((currentTime - startTime) >= 5000)
    {
      ecriture(); // On les écrit dans le fichier
      Serial.println("Eco");
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
          int argument = NAN;
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
      Serial.println("Maintenance");
      while (true)
      {
        gpsSerial.read();
      }
      // climateSensor.takeForcedMeasurement();
      // Serial.print(String(rtc.now().hour()) + F(":") + String(rtc.now().minute()) + F(":") + String(rtc.now().second())); // Ecriture de l'heure
      // Serial.print(F(" ; "));                                                                                             // Ecriture du séparateur
      // Serial.print(getGps());                                                                                             // Ecriture des données GPS
      // Serial.print(F(" ; "));                                                                                             // Ecriture du séparateur
      // Serial.print(getCapteur(LUMIN, analogRead(A0), LUMIN_LOW, LUMIN_HIGH));                                             // Ecriture de la luminosité
      // Serial.print(F(" ; "));                                                                                             // Ecriture du séparateur
      // Serial.print(getCapteur(TEMP_AIR, climateSensor.getTemperatureCelcius(), MIN_TEMP_AIR, MAX_TEMP_AIR));              // Ecriture de la température
      // Serial.print(F(" ; "));                                                                                             // Ecriture du séparateur
      // Serial.print(getCapteur(HYGR, climateSensor.getRelativeHumidity(), HYGR_MINT, HYGR_MAXT));                          // Ecriture de l'humidité
      // Serial.print(F(" ; "));                                                                                             // Ecriture du séparateur
      // Serial.println(getCapteur(PRESSURE, climateSensor.getPressure(), PRESSURE_MIN, PRESSURE_MAX));                      // Ecriture de la pression
      // startTime = currentTime;                                                                                            // On réinitialise le temps de démarrage
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