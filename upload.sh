#!/bin/bash

# Vérifie que la carte Arduino est connectée
if ! ls /dev/ttyACM* &> /dev/null; then
  echo "Aucune carte Arduino connectée."
  exit 1
fi

# Obtient le nom du fichier compilé
file_name=$(basename "$1")

# Convertit le fichier compilé en fichier `.hex`
avr-objcopy -O ihex "$1" "$1.hex"

# Téléverse le fichier `.hex` sur la carte Arduino
avrdude -p atmega328p -c arduino -P /dev/ttyACM0 -b 115200 -U flash:w:"$1.hex":i -v

# Supprime le fichier `.hex`
rm "$1.hex"

# Affiche un message de réussite
echo "Téléversement réussi."

