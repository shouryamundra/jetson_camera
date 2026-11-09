#!/bin/bash
# System setup for low-latency capture on the Jetson. Idempotent: safe to run again.
# Usage: sudo deploy/setup_jetson.sh [--user aruw] [--camera-cpu 4] [--isolate 4-7]
#   --isolate CPUS   also add isolcpus=CPUS to the kernel command line (takes effect on reboot;
#                    the old extlinux.conf is backed up). Omit to leave the boot config alone.
# See deploy/README.md for what each step does and its trade-offs.
set -euo pipefail

USER_NAME=aruw
CAMERA_CPU=4
ISOLATE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --user) USER_NAME=$2; shift 2 ;;
    --camera-cpu) CAMERA_CPU=$2; shift 2 ;;
    --isolate) ISOLATE=$2; shift 2 ;;
    *) echo "unknown option $1"; exit 2 ;;
  esac
done
[ "$(id -u)" -eq 0 ] || { echo "run as root (sudo)"; exit 1; }
say() { echo "== $*"; }

say "1. Persistent journal, capped at 200 MB / 14 days, 1 GB kept free, synced every 10 s"
mkdir -p /etc/systemd/journald.conf.d /var/log/journal
cat > /etc/systemd/journald.conf.d/50-jetson-camera.conf <<'EOF'
[Journal]
Storage=persistent
SystemMaxUse=200M
SystemKeepFree=1G
SystemMaxFileSize=25M
MaxRetentionSec=14day
MaxFileSec=1day
# Write to disk every 10 s (default 5 min) so a hard freeze keeps the last logs.
SyncIntervalSec=10s
EOF
systemd-tmpfiles --create --prefix /var/log/journal
systemctl restart systemd-journald
journalctl --disk-usage

say "2. USB autosuspend off for Hikrobot cameras (udev rule + applied now)"
cat > /etc/udev/rules.d/81-hikrobot-usb-power.rules <<'EOF'
# Never autosuspend Hikrobot (2bdf) cameras: no link power-down, no resume latency.
ACTION=="add", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="2bdf", TEST=="power/control", ATTR{power/control}="on"
EOF
udevadm control --reload
for d in /sys/bus/usb/devices/*; do
  if [ "$(cat "$d/idVendor" 2>/dev/null)" = "2bdf" ]; then
    echo on > "$d/power/control"
    echo "   $d power/control = $(cat "$d/power/control")"
  fi
done

say "3. Real-time priority and memory locking allowed for user $USER_NAME (new logins)"
cat > /etc/security/limits.d/90-jetson-camera.conf <<EOF
# Lets the camera pipeline use SCHED_FIFO (capped at 90) and mlockall().
$USER_NAME - rtprio 90
$USER_NAME - memlock unlimited
EOF
echo "   (log out/in or open a new ssh session; systemd services need LimitRTPRIO/LimitMEMLOCK instead)"

say "4. USB controller interrupts -> CPU $CAMERA_CPU, re-applied at every boot"
cat > /usr/local/sbin/jetson-camera-irq.sh <<EOF
#!/bin/bash
# Route xHCI (USB3 host) interrupts to the camera core so frame-completion handling never
# waits behind other work on CPU0 and stays cache-local to the grab thread.
for irq in \$(awk -F: '/xhci/ {gsub(/ /, "", \$1); print \$1}' /proc/interrupts); do
  echo $CAMERA_CPU > /proc/irq/\$irq/smp_affinity_list && echo "irq \$irq -> cpu \$(cat /proc/irq/\$irq/smp_affinity_list)"
done
EOF
chmod 755 /usr/local/sbin/jetson-camera-irq.sh
cat > /etc/systemd/system/jetson-camera-irq.service <<'EOF'
[Unit]
Description=Route USB (xHCI) interrupts to the camera CPU core
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/jetson-camera-irq.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable --now jetson-camera-irq.service
systemctl is-active irqbalance >/dev/null 2>&1 && echo "   WARNING: irqbalance is running and may move the IRQ back"

if [ -n "$ISOLATE" ]; then
  say "5. Kernel isolcpus=$ISOLATE (reboot required)"
  conf=/boot/extlinux/extlinux.conf
  if grep -q "isolcpus=" "$conf"; then
    echo "   isolcpus already present: $(grep -o 'isolcpus=[^ ]*' "$conf" | head -1); not changing it"
  else
    cp "$conf" "$conf.bak.$(date +%Y%m%d-%H%M%S)"
    # Only the first APPEND line (the default 'primary' entry).
    sed -i "0,/^\(\s*APPEND .*\)$/s//\1 isolcpus=managed_irq,domain,$ISOLATE/" "$conf"
    grep -n "APPEND" "$conf" | head -1
    echo "   backup saved; reboot to apply"
  fi
else
  say "5. Core isolation skipped (pass --isolate CPUS to enable)"
fi
say "done"
