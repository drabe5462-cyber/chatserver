#!/bin/bash
echo "========================================================"
echo " START: AUTOMATISCHES SETUP FÜR SERVER 2 (Zentrale + Redis)"
echo "========================================================"

# 1. System aktualisieren & Debian-Pakete installieren
echo "[1/6] Installiere benötigte Debian-Pakete..."
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential libssl-dev libsqlite3-dev libredis++-dev libbcrypt-dev redis-server ufw wget

# 2. Redis für das Netzwerk (IPv4 & IPv6) öffnen und Passwort setzen
echo "[2/6] Konfiguriere zentralen Redis-Server..."
REDIS_CONF="/etc/redis/redis.conf"
sudo sed -i 's/^bind .*/bind 0.0.0.0 ::/' $REDIS_CONF
sudo sed -i 's/^protected-mode yes/protected-mode no/' $REDIS_CONF

# Passwort am Ende der Konfiguration anhängen
if ! grep -q "requirepass MeinSicheresNetzwerkPasswort" "$REDIS_CONF"; then
    echo "requirepass MeinSicheresNetzwerkPasswort" | sudo tee -a $REDIS_CONF
fi

# Redis neu starten, um Konfiguration zu laden
sudo systemctl restart redis-server

# 3. Firewall (UFW) einrichten: Erlaube Server 1 den Zugriff auf Redis (Port 6379)
echo "[3/6] Konfiguriere Firewall für Server 1..."
sudo ufw allow from 172.16.130.85 to any port 6379 proto tcp
sudo ufw allow from 2001:db8:130::85 to any port 6379 proto tcp
sudo ufw --force enable

# 4. Projektverzeichnis 'chat_project' und 'templates' anlegen
echo "[4/6] Erstelle Projektverzeichnisstruktur..."
mkdir -p ~/chat_project/templates

# 5. In den Ordner wechseln und crow_all.h herunterladen
echo "[5/6] Lade Web-Framework (crow_all.h) herunter..."
cd ~/chat_project
wget https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h

# 6. Selbstsignierte SSL-Zertifikate für HTTPS/WSS generieren
echo "[6/6] Generiere SSL/TLS-Zertifikate..."
openssl req -x509 -newkey rsa:4096 -keyout _.rfhlab.de_private_key.key_neustart05032026.key -out rfhlab.de_ssl_certificate.cer -sha256 -days 365 -nodes -subj "/C=DE/ST=State/L=City/O=Enterprise/CN=172.16.130.86"

# 7. System-Bibliotheken auffrischen
sudo ldconfig

echo "========================================================"
echo " Server 2 (Zentrale) erfolgreich eingerichtet!"
echo " Ordner: ~/chat_project"
echo "========================================================"
echo " NÄCHSTE SCHRITTE:"
echo " 1. Erstelle ~/chat_project/server.cpp"
echo " 2. Erstelle ~/chat_project/templates/index.html"
echo " 3. Kompiliere mit:"
echo "    g++ -std=c++17 server.cpp -o chat_server -DCROW_ENABLE_SSL -lpthread -lssl -lcrypto -lredis++ -lsqlite3 -lbcrypt"
echo " 4. Starten mit: sudo PORT=443 ./chat_server [::1]"
echo "========================================================"
