#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing Spider Robot v3.1 systemd services..."

cp "$SCRIPT_DIR/spider-brain.service" /etc/systemd/system/
cp "$SCRIPT_DIR/spider-eye.service" /etc/systemd/system/

echo "Reloading systemd daemon..."
systemctl daemon-reload

echo "Enabling services..."
systemctl enable spider-brain.service
systemctl enable spider-eye.service

echo ""
echo "Services installed and enabled."
echo ""
echo "Status:"
systemctl status spider-brain.service --no-pager || true
systemctl status spider-eye.service --no-pager || true

echo ""
echo "To start now: systemctl start spider-brain spider-eye"
echo "View logs: journalctl -u spider-brain -u spider-eye -f"
