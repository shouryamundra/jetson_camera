# Jetson system setup for the camera pipeline

`sudo deploy/setup_jetson.sh [--user aruw] [--camera-cpu 4] [--isolate 4-7]` (idempotent).

| Step | What it does | Why | Downside |
|---|---|---|---|
| Persistent journal | `Storage=persistent`, max 200 MB total, 25 MB per file, 14 days, keeps 1 GB disk free | Crashes and reboots leave logs (`journalctl -b -1`) | Up to 200 MB of disk |
| USB autosuspend off | udev rule: Hikrobot (2bdf) devices `power/control=on` | The link never powers down; no resume delay or suspend glitches | A few hundred mW while the camera is idle |
| Real-time + memlock limits | `limits.d`: user may use SCHED_FIFO up to 90 and lock memory | Lets `threads.realtime_priority` and `threads.lock_memory` work | A buggy real-time thread could hog a core (the kernel still caps real-time at 95 % per second) |
| USB IRQ affinity | A boot service routes xHCI interrupts to the camera core | Frame completion is handled on the camera's core, not behind CPU0's work | That core takes all USB interrupt work. On this Jetson one xHCI controller serves every USB port, including the CP210x serial adapter (likely the MCB UART), so Otto's serial interrupts land on the camera core too. If the camera core is ever saturated, all USB devices slow down |
| Core isolation (opt-in) | `isolcpus=managed_irq,domain,<cpus>` on the kernel command line | The scheduler never puts other processes or kernel workers on those cores | Those cores sit idle unless a thread is explicitly pinned there, so you lose general capacity. Needs a reboot; edits boot config (a backup is made) |

## Getting the limits in practice

- **Tailscale SSH sessions bypass PAM**, so `limits.d` does not apply to them. For testing use
  `deploy/run_rt.sh <command>` (raises the shell's limits with sudo, then runs as your user).
- **systemd services** (how Otto runs on the robot) ignore `limits.d` too. Add to the unit:
  `LimitRTPRIO=90` and `LimitMEMLOCK=infinity` (or `AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK`).

## What "real-time priority" means

Linux normally shares each core fairly between all runnable threads, so a thread that just got
a frame may wait a few milliseconds behind something else (Python, logging, a compiler). With
`SCHED_FIFO` the thread runs the instant it becomes runnable and keeps the core until it
blocks again. The grab thread works ~0.3 ms per frame and then sleeps, so it cannot starve the
system. The cost is that a real-time thread with a bug (an endless loop) could monopolise a core.

## Proposed core plan (AGX Orin: 12 cores, clusters 0-3 / 4-7 / 8-11, each with its own L3)

| Cores | Use | Isolated? |
|---|---|---|
| 0-3 | OS, ssh, journald, GStreamer recorder threads, camera watchdog, Otto's non-critical work | no |
| 4 | Camera grab thread (`threads.cpu = 4`) + USB interrupts | yes |
| 5-7 | Otto's latency-critical threads (main loop / inference host thread / UART), pinned by Otto | yes |
| 8-11 | Otto's other threads and Python workers | no |

Cores 4-7 are one cluster, so camera and Otto's hot path share an L3 cache and never compete with
general load. The camera needs ~3-6 % of core 4, so core 4 could also host one light Otto thread.
Revisit when Otto's thread layout is known.

## Not available on this kernel

`nohz_full` (stopping the 250 Hz timer tick on isolated cores) needs `CONFIG_NO_HZ_FULL`, which
the stock L4T kernel does not enable. Isolated cores still take that tick (a few microseconds
every 4 ms).
