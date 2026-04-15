#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config sqlite3 libsqlite3-dev nginx certbot python3-certbot-nginx

sudo useradd --system --home /opt/hangout --shell /usr/sbin/nologin hangout 2>/dev/null || true
sudo mkdir -p /opt/hangout /var/lib/hangout /var/www/hangout-updates
sudo chown -R hangout:hangout /opt/hangout /var/lib/hangout
