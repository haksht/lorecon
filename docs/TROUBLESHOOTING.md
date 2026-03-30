# Troubleshooting Guide

---

## Serial console activation

Serial commands are disabled by default to prevent USB electrical noise from triggering phantom
commands during unattended operation.

**To activate:** Press Enter once -- you will see "Press Enter again within 1.5s to activate..."
Press Enter a second time within 1.5 seconds. The console auto-deactivates after 5 minutes of
inactivity.

**Heltec V4:** No serial monitor available. The V4 native USB interferes with the radio at the
hardware level; serial is permanently disabled. Use the web UI for all monitoring and commands.

---

## Pre-flight checklist

- [ ] Antenna connected to the LoRa module
- [ ] Antenna matches your region (915 MHz for US, 868 MHz for EU)
- [ ] OLED shows scanning status, or web UI is accessible
- [ ] Test device powered on and configured for the same frequency region

---

## Diagnostic steps

### Step 1: Confirm the test device is transmitting

Using the Meshtastic app: Radio Configuration -> LoRa. Note the Region, Modem Preset, and
Frequency. Send a test message and watch for the "Sent" confirmation.

Using the CLI:
```bash
meshtastic --info        # shows region, modem preset, frequency
meshtastic --sendtext "Test"
```

### Step 2: Confirm the radio is receiving (Heltec V3 / T3-S3 / T-Beam)

```
1. Open serial monitor: pio device monitor --port COM3 --baud 115200
2. Wait for "[RECON] Started"
3. Hold a Meshtastic device within 1 foot of the sniffer
4. Send a message
5. Look for any [PACKET] lines
```

Expected:
```
[SCAN] Meshtastic_LF_906 (906.875 MHz, SF11, BW250, SW:0x2B)
[PACKET] Meshtastic, 14 bytes, -45.0 dBm, 8.2 dB SNR
```

On the **Heltec V4**, check the web UI Packets tab instead.

### Step 3: Lock to a known frequency

If you know what frequency the test device is using, lock the sniffer to it:

1. Press `m` on the serial console (or use the Frequency tab in the web UI)
2. Press `f` for frequency targeting
3. Select the matching config (e.g., `Meshtastic_LF_906` for 906.875 MHz)
4. Send a message from the test device -- it should appear immediately

---

## Common issues

### No antenna connected

**Symptom:** No packets ever, even with devices touching.
**Fix:** Connect a 915 MHz antenna to the LoRa module.

### Wrong region

**Symptom:** Test device on EU frequencies (433/868 MHz), sniffer scanning US (902-928 MHz).
**Fix:**
```
Meshtastic app -> Radio Config -> Region -> US
# or
meshtastic --set lora.region US
```

### Test device not transmitting

**Symptom:** Device appears on, no TX activity.
**Check:** Battery charged? Stuck in boot loop? Error on screen?

### Replayed packet not recaptured

This is expected. The SX1262 is half-duplex -- it cannot receive while transmitting. Your own
replay is already gone by the time the radio returns to RX mode. To verify a replay worked, use
a second Meshtastic device in range, or watch for "TX complete" in serial output.

### Queue overflow / dropped packets

**Symptom:** Serial shows `[QUEUE] Full - dropping packet!` or web UI toast warns of drops.

This happens in high-traffic environments (50+ devices, burst transmissions, slow SD card writes).

**Solutions:**
- Disconnect the browser/phone during critical capture -- WebSocket broadcasts consume CPU
- Use a Class 10 or UHS-I SD card (faster writes reduce blocking time)
- Lock to a single frequency (`f` in serial or Frequency tab) to eliminate scan overhead

**Drop rate interpretation:**

| Drop rate | Assessment |
|-----------|------------|
| < 2% | Normal -- occasional bursts |
| 2-5% | Acceptable in busy environments |
| 5-10% | Investigate -- try the mitigations above |
| > 10% | Problematic |

Monitor via: `/api/status` -> `droppedPackets`, or watch for `[QUEUE]` in serial output.

### Hardware sensitivity variance

**Symptom:** Two identical boards at the same location show significantly different packet counts.

SX1262 chips have +/-2-3 dB manufacturing tolerance in receive sensitivity. A 3 dB difference
means one radio can hear signals the other misses. This is normal and not firmware-fixable.
Test both units overnight and label the more sensitive one as your field unit.

### Frequency mismatch

**Symptom:** Sniffer scanning frequency A, test device on frequency B.
**Fix:** Lock both to a known config.

On the Meshtastic device: app -> Channels -> Primary -> set Frequency Slot 20 (default = 906.875 MHz).
On the sniffer: press `f` -> select `Meshtastic_LF_906`.

---

## Signal health check

**Radio working (minimum):** Any packet received from a nearby device, even 14-byte routing packets.

**Ready for field use:** Packets appearing consistently, devices visible in Devices tab with node IDs.

**Full capability:** PSK decryption succeeding, decrypted text visible in Packets tab.

---

## Serial commands

Full command reference is in [developers/BUILDING.md](developers/BUILDING.md#serial-monitor).

Quick reference for troubleshooting:

| Key | Action |
|-----|--------|
| `m` | Device menu |
| `f` | Frequency targeting |
| `r` | Resume reconnaissance |
| `s` | Show summary |
| `i` | Reset reason and health info |
| `t` | Show API token |
| `b` | Reboot |

Activate the console first: double-Enter within 1.5 seconds.

---

## OLED display

**Blank display or "No OLED detected":**

Check serial for `[DISPLAY] Address 0x3C: FOUND!` vs `not found (error 2)`.

- Error 2 = no device at that I2C address -- check physical connection
- Heltec boards power the OLED via a GPIO-controlled rail; serial will show `[DISPLAY] Vext ON` if it toggled correctly
- Some boards ship without the OLED soldered; the firmware continues without display
- Heltec V1/V2 (non-S3) boards are not supported

---

## SD card

SD logging is automatic on T3-S3 and T-Beam Supreme. Insert a FAT32-formatted MicroSD before
booting -- firmware creates `/logs/` on the first received packet.

**Card not detected:** Check serial for `[SD] SD card detected` vs `No SD card detected`.

- Must be FAT32 (not exFAT or NTFS)
- Use 4 GB or smaller cards on T3-S3; T-Beam supports up to 32 GB
- Push until it clicks; try a different card if a generic one fails

**Heltec V3 and V4** have no SD slot -- all data is RAM-only. Export before rebooting.

---

## API authentication

**401 Unauthorized:** The web UI handles auth automatically. For direct API access (curl, scripts):

1. Get the token -- shown in serial output at boot, or in the web UI Settings tab (V4)
2. Add it to requests:
```bash
curl -X POST http://192.168.4.1/api/devices/clear      -H "X-API-Token: YOUR_TOKEN_HERE"
```

The token persists across reboots. If you lose it, reboot and check serial or Settings.

Read-only endpoints (devices list, status) require no token. Destructive and configuration
endpoints (clear, replay, WiFi config, firmware upload) require the token.

---

## Quick reception test

Before field deployment:

1. Lock to a known config: `f` -> `Meshtastic_LF_906`
2. Hold a Meshtastic device within 1 foot
3. Send a message
4. Watch for any `[PACKET]` line in serial (or Packets tab on V4)

Packet received: radio working, deploy.
Nothing: check antenna, region setting, confirm test device is transmitting.
