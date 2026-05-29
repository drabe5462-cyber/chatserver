#!/bin/bash
echo "========================================================"
echo " START: SETUP FÜR SERVER 1 MIT ECHTEM SSL-ZERTIFIKAT"
echo "========================================================"

# 1. System aktualisieren & Pakete installieren (inklusive unzip)
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential libssl-dev libsqlite3-dev libredis++-dev libbcrypt-dev redis-server wget unzip

# 2. Lokalen Redis-Dienst abschalten
sudo systemctl stop redis-server
sudo systemctl disable redis-server

# 3. Projektverzeichnis anlegen
mkdir -p ~/chat_project/templates

# 4. crow_all.h herunterladen
cd ~/chat_project
wget https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h

# 5. EIGENE SSL-ZERTIFIKATE EINBINDEN
echo "[SSL] Verarbeite rfhlab.de Zertifikate..."

# Privaten Schlüssel kopieren und für Crow vorbereiten
if [ -f ~/_.rfhlab.de_private_key.key_neustart05032026.key ]; then
    cp ~/_.rfhlab.de_private_key.key_neustart05032026.key ~/chat_project/key.pem
else
    echo "WARNUNG: Private Key nicht in ~/ gefunden!"
fi

# ZIP-Archiv entpacken und Certificate Chain (Bundle) erstellen
if [ -f ~/_.rfhlab.de_ssl_certificate_INTERMEDIATE.zip ]; then
    mkdir -p /tmp/ssl_unpack
    unzip -q ~/_.rfhlab.de_ssl_certificate_INTERMEDIATE.zip -d /tmp/ssl_unpack
    
    # Automatische Zusammenführung von Domain-Zertifikat und Intermediate/Bundle
    # Falls dein Zip andere Dateinamen hat, hier kurz anpassen:
    cat /tmp/ssl_unpack/*.crt > ~/chat_project/cert.pem
    rm -rf /tmp/ssl_unpack
else
    echo "WARNUNG: SSL-Zip-Archiv nicht in ~/ gefunden!"
fi

sudo ldconfig
echo "========================================================"
echo " Server 1 erfolgreich eingerichtet!"
echo "========================================================"
