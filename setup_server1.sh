#!/bin/bash
echo "========================================================"
echo " START: AUTOMATISCHES SETUP FÜR SERVER 1 (Zusatzknoten)"
echo "========================================================"

# 1. System aktualisieren & Debian-Pakete installieren
echo "[1/5] Installiere benötigte Debian-Pakete..."
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential libssl-dev libsqlite3-dev libredis++-dev libbcrypt-dev redis-server wget

# 2. Lokalen Redis-Dienst abschalten (Server 1 nutzt den Redis von Server 2)
echo "[2/5] Deaktiviere lokalen Redis-Dienst..."
sudo systemctl stop redis-server
sudo systemctl disable redis-server

# 3. Projektverzeichnis 'chat_project' und 'templates' anlegen
echo "[3/5] Erstelle Projektverzeichnisstruktur..."
mkdir -p ~/chat_project/templates

# 4. In den Ordner wechseln und crow_all.h herunterladen
echo "[4/5] Lade Web-Framework (crow_all.h) herunter..."
cd ~/chat_project
wget https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h

# 5. Selbstsignierte SSL-Zertifikate für HTTPS/WSS generieren
echo "[5/5] Generiere SSL/TLS-Zertifikate..."
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 365 -nodes -subj "/C=DE/ST=State/L=City/O=Enterprise/CN=172.16.130.85"

# 6. System-Bibliotheken auffrischen
sudo ldconfig

echo "========================================================"
echo " Server 1 erfolgreich eingerichtet!"
echo " Ordner: ~/chat_project"
echo "========================================================"
echo " NÄCHSTE SCHRITTE:"
echo " 1. Erstelle ~/chat_project/server.cpp"
echo " 2. Erstelle ~/chat_project/templates/index.html"
echo " 3. Kompiliere mit:"
echo "    g++ -std=c++17 server.cpp -o chat_server -DCROW_ENABLE_SSL -lpthread -lssl -lcrypto -lredis++ -lsqlite3 -lbcrypt"
echo " 4. Starten mit: sudo PORT=443 ./chat_server 2001:db8:130::86"
echo "========================================================"
