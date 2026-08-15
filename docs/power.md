# Power and battery life

Written 2026-08-15T20:00:23Z. Read this before attempting battery optimization,
because most of the obvious moves are already closed off by the hardware choice
and you will waste a day rediscovering that.

## The target

The wiki requirements ask for **1 month continuous usage** and **1 year
standby/off**, with easily replaceable batteries. The AlphaSmart Neo this device
clones does ~700 hours on 3×AA.

3×AA alkaline is roughly 2400 mAh usable. 720 hours from 2400 mAh is an
**average budget of 3.3 mA**. That number is the whole problem: every decision
below is measured against it.

## Two constraints that are not negotiable in firmware

Both come from `ESP32-USB-Soft-Host`, the library that lets a WROOM read a USB
keyboard at all. Verified by reading the library source, not inferred:

1. **The CPU cannot leave its boot clock speed.** The library bit-bangs USB by
   executing generated runs of `nop` instructions (`usb_host.c`, `makeOpcodes`),
   and it calibrates how many NOPs make a bit period by reading the actual clock
   at init — `usb_host.c:1491` calls `rtc_clk_cpu_freq_get_config()` and then
   tunes `TRANSMIT_TIME_DELAY` against `testDelay6(out_config.freq_mhz)`. Call
   `setCpuFrequencyMhz(80)` afterwards and USB timing breaks. So the single
   largest ESP32 power lever — downclocking — is unavailable.

2. **The CPU cannot light sleep.** `usb_process()` runs from a hardware group
   timer ISR at 1 kHz (`usb_host.h:26`, `TIMER_INTERVAL0_SEC (0.001)`; the ISR
   is `timer_group0_isr` in `ESP32-USBSoftHost.cpp`). Light sleep stops that
   timer, which drops the keyboard. So the second largest lever — sleeping
   between keystrokes — is also unavailable.

These are the same *class* of finding as "the WROOM can never be a USB
keyboard": a property of the part and the approach, not a missing driver. Do not
re-litigate either as a software problem.

## Current census

**All figures below are ESTIMATED** from datasheets and community measurements.
**Nothing here has been measured on the assembled board.** Anyone with a USB
power meter or a bench supply should replace this table with real numbers; until
then treat the total as an order of magnitude, not a measurement.

| Draw | mA (est.) | Fixed by | Firmware can change it? |
|---|---|---|---|
| USB keyboard, bus-powered by us | 50–100 | architecture | no |
| ESP32 active @240 MHz, no sleep | 40–50 | soft USB host | no |
| SerLCD RGB backlight at level 64 ×3 | 20–30 | display choice | **yes** |
| Dev board CP2102 + AMS1117 quiescent | 15–25 | dev board | no |
| Editor redisplay work | (inside the ESP32 row) | — | technically |

Total on the order of **125–205 mA against a 3.3 mA budget** — off by roughly
40–60×, i.e. somewhere near 12–20 hours of continuous use rather than 720.

The important consequence: **no firmware change can reach the requirement.**
Anything presented as "battery optimization" that is not the backlight or a
hardware change is theatre.

## What the firmware actually does

**Two-stage backlight idle.** `config.h` defines `BACKLIGHT_LEVEL` (64 while
writing), `BACKLIGHT_DIM_LEVEL` (12), `BACKLIGHT_DIM_MILLISECONDS` (30 s) and
`BACKLIGHT_OFF_MILLISECONDS` (5 min). `update_backlight()` in the sketch steps
down as the device sits and restores instantly on any key. Two stages rather
than one because a writing device gets stared at while you think — dropping
straight to black after thirty seconds of stillness would be wrong.

The key that wakes the backlight also types. Swallowing the first character of a
sentence to serve a wake-up gesture would be a worse trade than a bright screen.

`display_set_backlight()` skips the I²C write when the level is unchanged, so
calling it from the editor loop costs nothing.

## What was deliberately not done, and why

- **Explicitly disabling WiFi/BT.** The sketch never initialises either, so the
  radio is already off and `esp_wifi_deinit()` / `btStop()` would be no-ops.
  Adding them would look like optimization while doing nothing.
- **Blocking the editor task instead of polling at 1 kHz.** `editor_task` wakes
  every tick and does a few microseconds of work, so this is on the order of
  0.3 % of one row of the table above. Worth doing eventually for cleanliness;
  not worth the risk to key repeat and autosave for the power it saves.
- **Cutting the per-keystroke redisplay cost.** Real and worth fixing on its own
  merits — the window re-derives display-line starts from scratch every frame,
  which is O(paragraph) per keystroke — but the CPU is pinned at 240 MHz and
  cannot sleep, so doing less work with it saves almost no energy. Treat that as
  a responsiveness and headroom job, not a battery job.

## What would actually reach the target

In descending order of effect. All three are hardware decisions:

1. **Drop USB host input for a directly scanned key matrix.** This removes the
   keyboard's bus draw *and* both constraints above at once: with no soft USB
   host there is no NOP calibration and no 1 kHz ISR, so the CPU can downclock
   and light sleep between keypresses with GPIO wake. This is what the Neo does.
   Note the repo already went the other way once — `keyboard_matrix.h` is still
   in the tree, disabled, and commit `eaf7b49 "Remove matrix keypad"` is where
   the current architecture was chosen.
2. **Use a reflective, non-backlit LCD.** This also resolves a tension already
   present in the requirements: the wiki asks for a display "readable in direct
   sunlight", which is exactly what reflective LCDs do well and backlit ones do
   badly. The Neo has no backlight at all, which is part of how it reaches 700
   hours.
3. **Leave the dev board behind.** A bare module plus a regulator chosen for low
   quiescent current removes the CP2102 and the AMS1117.

With those, an average in the 1–2 mA range is plausible (ESP32 light sleep is
~0.8 mA, with short active bursts per keypress), which is inside the budget.

Sending a buffer as a USB keyboard still needs an ESP32-S2/S3 — see the hardware
note in `src/text_output.h`. An S3 would also give native USB with proper sleep
support, so it is worth evaluating as one decision alongside the above rather
than as a separate upgrade.

## How to verify any of this

Put a meter inline with the battery and record average current in four states:
idle with backlight at 64, idle dimmed, idle with backlight off, and sustained
typing. Four numbers replace the entire estimated table above, and they decide
whether the backlight work mattered as much as this document claims.
