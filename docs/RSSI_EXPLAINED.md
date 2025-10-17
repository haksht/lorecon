# Why is RSSI Negative? - Radio Signal Strength Explained

## Quick Answer

**RSSI is measured in dBm (decibels relative to 1 milliwatt), and it's negative because the received signal power is less than 1 milliwatt.**

---

## Understanding dBm

### What is dBm?

**dBm = decibels relative to 1 milliwatt (mW)**

It's a logarithmic scale for measuring power:

```
dBm = 10 × log₁₀(Power_mW / 1 mW)
```

### Examples:

```
Power               dBm Value
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
1000 mW (1 W)       +30 dBm   (very strong - transmitter)
100 mW              +20 dBm   (strong transmitter)
1 mW                0 dBm     (reference point)
0.1 mW (100 µW)     -10 dBm   (weak but usable)
0.001 mW (1 µW)     -30 dBm   (very weak)
0.000001 mW (1 nW)  -60 dBm   (extremely weak)
0.000000001 mW      -90 dBm   (barely detectable)
```

### Why Logarithmic?

Radio signals span an enormous range of power levels:

```
Transmitted:  100,000,000 nanowatts  (100 mW, +20 dBm)
Received:            1 nanowatt       (1 nW, -60 dBm)
                     ─────────────
Ratio:        100,000,000:1           (80 dB loss)
```

Using linear scale would be impractical:
- ❌ "Signal strength: 0.000000001 mW"
- ✅ "Signal strength: -60 dBm"

---

## RSSI in Your LoRa Sniffer

### Typical RSSI Values

From your code comments and real-world usage:

```cpp
// Power class estimation
if (rssi > -70) return 2;  // High power (>100mW)
if (rssi > -90) return 1;  // Medium power (10-100mW)
return 0;                  // Low power (<10mW)
```

### Real-World Examples:

```
RSSI        Distance    Scenario
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
-30 dBm     < 1 m       Device right next to you
-50 dBm     1-10 m      Same room, line of sight
-70 dBm     10-100 m    Neighboring building
-90 dBm     100-500 m   Across town (with LoRa)
-110 dBm    500-2000 m  Long range (rural)
-120 dBm    2-10 km     Maximum LoRa range
-130 dBm    > 10 km     Beyond typical reception
```

### Your Device Classifications:

```cpp
// From protocol_analyzer.cpp
if (rssi > -50) return "Meshtastic Base/Solar";      // Very close
if (rssi > -80) return "Meshtastic Mobile";          // Nearby
if (rssi > -110) return "Meshtastic Handheld";       // Distant
return "Meshtastic Low-Power";                       // Very distant
```

---

## Why Negative Values?

### Physical Explanation:

When you transmit at **+20 dBm** (100 mW) and receive at **-80 dBm** (0.00001 mW):

```
Signal Loss = Transmit Power - Received Power
            = (+20 dBm) - (-80 dBm)
            = 100 dB path loss

This loss comes from:
- Distance (inverse square law)
- Obstacles (walls, trees, buildings)
- Atmospheric absorption
- Antenna inefficiency
```

### Mathematical Explanation:

```
Received Power = 0.00001 mW = 0.00000001 W = 10 nanowatts

dBm = 10 × log₁₀(0.00001 mW / 1 mW)
    = 10 × log₁₀(0.00001)
    = 10 × (-5)
    = -50 dBm
```

**Since the received power is less than 1 mW (the reference), the log is negative!**

---

## RSSI in Your Code

### Reading RSSI from SX1262:

```cpp
float rssi = radio.getRSSI();  // Returns negative value
```

### SX1262 RSSI Range:

```
Minimum: -148 dBm  (sensitivity limit - barely detectable)
Typical: -120 to -40 dBm  (normal operation)
Maximum: -10 dBm   (very strong signal, risk of overload)
```

### Why Skip RSSI for Small Packets?

From your code:

```cpp
// Get signal metrics ONLY for large packets (optimization!)
float rssi = 0, snr = 0;
if (length >= 40) {  // Skip for beacons (saves ~5ms)
    rssi = radio.getRSSI();
    snr = radio.getSNR();
}
```

**Reason**: Reading RSSI requires SPI transaction to radio registers (~2ms overhead)
- For beacons and keep-alives, signal strength isn't critical
- For data packets (≥40 bytes), RSSI helps classify device type

---

## SNR vs RSSI

### SNR (Signal-to-Noise Ratio)

```
SNR = Signal Power - Noise Floor
```

**Examples:**

```
RSSI = -90 dBm    Noise = -100 dBm    SNR = +10 dB  ✓ Good
RSSI = -90 dBm    Noise = -95 dBm     SNR = -5 dB   ✗ Poor
```

### Why SNR Matters More Than RSSI:

```
Scenario A:  RSSI = -50 dBm, SNR = -2 dB  → Can't decode (noisy environment)
Scenario B:  RSSI = -110 dBm, SNR = +8 dB → Decodes perfectly (clean signal)
```

**Your code considers both:**

```cpp
logPacket(data, length, rssi, snr, protocol);
```

### LoRa Sensitivity:

The SX1262 can decode signals with:
- **RSSI as low as -148 dBm**
- **SNR as low as -20 dB** (with SF12)

This is why LoRa has incredible range!

---

## Practical Implications for Your Sniffer

### 1. Device Discovery

```cpp
// High power device (close or strong transmitter)
if (rssi > -70) {
    // Probably base station, solar-powered, or very close
}

// Low power device (distant or weak transmitter)
if (rssi < -110) {
    // Probably handheld at edge of range
}
```

### 2. Targeting Strategy

Better RSSI = Better chance of successful decryption:

```cpp
void LoRaReconTool::trackTargetableDevice(..., float rssi, ...) {
    // Prioritize devices with strong signal
    if (rssi > target.bestRSSI) {
        target.bestRSSI = rssi;  // Update to strongest reception
    }
}
```

### 3. Range Estimation

Using the **Friis transmission equation**:

```
Path Loss (dB) = 20 × log₁₀(distance) + 20 × log₁₀(frequency) + 32.44

For 915 MHz LoRa:
- 100 m  → ~90 dB loss  → RSSI ≈ -70 dBm (if TX = +20 dBm)
- 1 km   → ~110 dB loss → RSSI ≈ -90 dBm
- 10 km  → ~130 dB loss → RSSI ≈ -110 dBm
```

---

## Common Misconceptions

### ❌ "More negative = stronger signal"
**✓ Correct**: Less negative = stronger  
- -50 dBm is **stronger** than -90 dBm
- -50 dBm = 0.00001 mW
- -90 dBm = 0.000000001 mW (1000× weaker!)

### ❌ "RSSI is a percentage"
**✓ Correct**: RSSI is absolute power in dBm  
- Not relative to anything except 1 mW reference

### ❌ "RSSI -90 dBm is bad"
**✓ Correct**: -90 dBm is actually quite good for LoRa!  
- LoRa designed for weak signals
- -90 dBm can decode perfectly with good SNR

### ❌ "0 dBm is zero power"
**✓ Correct**: 0 dBm = 1 mW (not zero!)  
- Negative values are less than 1 mW
- Positive values are more than 1 mW

---

## Debugging Tips

### Good Signal Quality:

```
[RECON] Packet: Meshtastic, 80 bytes, -65.0 dBm, 9.5 dB SNR
                                        ^^^^^^^^  ^^^^^^^^^
                                        Strong    Excellent
```

✓ RSSI > -80 dBm (strong)  
✓ SNR > 5 dB (clean signal)  
✓ Likely to decode successfully

### Marginal Signal:

```
[RECON] Packet: Meshtastic, 80 bytes, -105.0 dBm, -2.3 dB SNR
                                        ^^^^^^^^^^  ^^^^^^^^^^
                                        Weak        Noisy
```

⚠️ RSSI < -100 dBm (weak)  
⚠️ SNR < 0 dB (noisy)  
⚠️ May fail to decode, corruption likely

### No Signal:

```
[RECON] Waiting... (no packets)
```

Possible reasons:
- No active devices in range
- Wrong frequency configured
- RSSI would be below -148 dBm (sensitivity limit)

---

## Quick Reference

### RSSI Scale:

```
-30 dBm  ████████████████████████████  Extremely Strong (touching)
-50 dBm  ████████████████████          Very Strong (same room)
-70 dBm  ████████████                  Strong (nearby)
-90 dBm  ████████                      Good (distant)
-110 dBm ████                          Weak (far away)
-130 dBm █                             Very Weak (barely detectable)
-148 dBm                               Sensitivity Limit (SX1262)
```

### SNR Scale:

```
+15 dB   ████████████████████████████  Excellent (crystal clear)
+10 dB   ████████████████████          Very Good (clean)
+5 dB    ████████████                  Good (usable)
0 dB     ████████                      Fair (signal = noise)
-5 dB    ████                          Poor (noisy)
-10 dB   ██                            Very Poor (marginal)
-20 dB   █                             Minimum (SF12 only)
```

---

## Math Cheat Sheet

### dBm to Watts:

```
Power (W) = 10^(dBm / 10) / 1000
```

Examples:
```
+30 dBm = 10^(30/10) / 1000 = 1 W
  0 dBm = 10^(0/10) / 1000  = 0.001 W = 1 mW
-30 dBm = 10^(-30/10) / 1000 = 0.000001 W = 1 µW
-60 dBm = 10^(-60/10) / 1000 = 0.000000001 W = 1 nW
```

### Watts to dBm:

```
dBm = 10 × log₁₀(Power_mW)
```

Examples:
```
1 W     = 10 × log₁₀(1000) = +30 dBm
1 mW    = 10 × log₁₀(1)    = 0 dBm
1 µW    = 10 × log₁₀(0.001) = -30 dBm
1 nW    = 10 × log₁₀(0.000001) = -60 dBm
```

### dB Addition Rules:

```
+3 dB  = 2× power     (double)
+10 dB = 10× power    (one order of magnitude)
-3 dB  = 0.5× power   (half)
-10 dB = 0.1× power   (one tenth)
```

---

## Summary

**RSSI is negative because:**

1. **It's measured in dBm** (decibels relative to 1 milliwatt)
2. **Received signals are tiny** (< 1 mW, often nanowatts or picowatts)
3. **Logarithmic scale** makes huge ranges manageable
4. **log₁₀(value < 1) = negative number**

**Remember:**
- Less negative = stronger signal (-50 dBm > -90 dBm)
- RSSI and SNR together determine decode success
- LoRa excels at weak signals (-120 dBm still usable!)

**In your sniffer:**
- -50 to -70 dBm: Close devices (same building)
- -70 to -90 dBm: Medium distance (neighborhood)
- -90 to -110 dBm: Long distance (across town)
- < -110 dBm: Very long distance (edge of range)

---

*This explanation prepared for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-15*
