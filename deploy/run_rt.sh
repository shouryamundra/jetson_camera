#!/bin/bash
# Run a command with real-time and memory-lock limits raised, for interactive testing.
# Needed because Tailscale SSH sessions bypass PAM, so /etc/security/limits.d does not apply.
# Production services set LimitRTPRIO=90 and LimitMEMLOCK=infinity in their systemd unit instead.
# Usage: deploy/run_rt.sh ./build/capture_bench --seconds 30
set -e
sudo prlimit --pid $$ --rtprio=90:90 --memlock=unlimited:unlimited
exec "$@"
