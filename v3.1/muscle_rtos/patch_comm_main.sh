#!/bin/bash
# Patch comm_main.c for Spider Muscle Runtime

COMM_MAIN="$HOME/duo-sdk-v1/freertos/cvitek/task/comm/src/riscv64/comm_main.c"

# Backup
cp "$COMM_MAIN" "${COMM_MAIN}.bak"

# Add extern declaration after line 20
sed -i '20 a extern void app_main(void);' "$COMM_MAIN"

# Add app_main() call after main_create_tasks()
sed -i '/main_create_tasks();/a\    printf("[Spider] Muscle Runtime v3.1\\n"); app_main();' "$COMM_MAIN"

echo "Patched comm_main.c"
grep -n "app_main\|Spider" "$COMM_MAIN"
