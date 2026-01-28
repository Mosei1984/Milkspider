#!/bin/bash
# Spider Robot v3.1 - Deployment Script
# Copies binaries and Python scripts to Milk-V Duo

set -e

# Configuration
DUO_HOST="${DUO_HOST:-192.168.42.1}"
DUO_USER="${DUO_USER:-root}"
DEPLOY_DIR="/usr/local/spider"

echo "=== Spider v3.1 Deployment ==="
echo "Target: ${DUO_USER}@${DUO_HOST}"
echo ""

# Check SSH connectivity
echo "[1/5] Testing SSH connection..."
if ! ssh -o ConnectTimeout=5 ${DUO_USER}@${DUO_HOST} "echo 'Connected'" 2>/dev/null; then
    echo "ERROR: Cannot connect to ${DUO_HOST}"
    echo "Ensure Milk-V Duo is connected via USB (RNDIS) or network."
    exit 1
fi

# Create remote directories
echo "[2/5] Creating directories..."
ssh ${DUO_USER}@${DUO_HOST} "mkdir -p ${DEPLOY_DIR}/bin ${DEPLOY_DIR}/python ${DEPLOY_DIR}/motions ${DEPLOY_DIR}/config"

# Deploy binaries
echo "[3/5] Deploying binaries..."
BINARIES=(
    "build/brain_linux/src/brain_daemon"
    "build/brain_linux/eye_service/eye_service"
)
for bin in "${BINARIES[@]}"; do
    if [ -f "$bin" ]; then
        scp "$bin" ${DUO_USER}@${DUO_HOST}:${DEPLOY_DIR}/bin/
        echo "  ✓ $(basename $bin)"
    else
        echo "  ⚠ $bin not found (not built yet?)"
    fi
done

# Deploy Python scripts
echo "[4/5] Deploying Python scripts..."
scp -r python/*.py ${DUO_USER}@${DUO_HOST}:${DEPLOY_DIR}/python/
echo "  ✓ Python scripts"

# Deploy motions and config
echo "[5/5] Deploying configuration..."
if [ -d "motions" ]; then
    scp -r motions/* ${DUO_USER}@${DUO_HOST}:${DEPLOY_DIR}/motions/ 2>/dev/null || true
fi
if [ -d "config" ]; then
    scp -r config/* ${DUO_USER}@${DUO_HOST}:${DEPLOY_DIR}/config/ 2>/dev/null || true
fi

# Create systemd services
echo ""
echo "[Optional] Creating systemd services..."
ssh ${DUO_USER}@${DUO_HOST} "cat > /etc/init.d/spider-brain << 'EOF'
#!/bin/sh
### BEGIN INIT INFO
# Provides:          spider-brain
# Required-Start:    \$local_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
### END INIT INFO

DAEMON=${DEPLOY_DIR}/bin/brain_daemon
PIDFILE=/var/run/spider-brain.pid

case \"\$1\" in
    start)
        echo \"Starting Spider Brain...\"
        start-stop-daemon -S -b -m -p \$PIDFILE -x \$DAEMON
        ;;
    stop)
        echo \"Stopping Spider Brain...\"
        start-stop-daemon -K -p \$PIDFILE
        rm -f \$PIDFILE
        ;;
    restart)
        \$0 stop
        \$0 start
        ;;
    *)
        echo \"Usage: \$0 {start|stop|restart}\"
        exit 1
esac
exit 0
EOF
chmod +x /etc/init.d/spider-brain"

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "To test hardware:"
echo "  ssh ${DUO_USER}@${DUO_HOST}"
echo "  python3 ${DEPLOY_DIR}/python/spider_test.py"
echo "  python3 ${DEPLOY_DIR}/python/pca9685_test.py"
echo "  python3 ${DEPLOY_DIR}/python/vl53l0x_test.py"
echo ""
echo "To start Brain Daemon:"
echo "  ${DEPLOY_DIR}/bin/brain_daemon"
echo ""
