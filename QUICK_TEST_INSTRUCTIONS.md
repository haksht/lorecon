# Quick Test: Capture Text Messages

## The Problem
When targeting a specific device, we only see routing packets (35 bytes) and miss the actual text messages (45-60 bytes) because they're transmitted during the brief moment the ESP32 is busy processing.

## Solution: Passive Mesh Monitoring

Instead of targeting your Heltec, let the ESP32 monitor the entire mesh passively.

## Test Procedure

1. **Start in Reconnaissance Mode** (not targeting mode)
   - Reset ESP32
   - Let it scan all frequencies
   - DO NOT press 't' to target

2. **Send Multiple Messages**
   - From Heltec, send: "TEST1"
   - Wait 5 seconds
   - Send: "TEST2"
   - Wait 5 seconds  
   - Send: "TEST3"

3. **What to Look For**
   - Packets **45-70 bytes** (text message size)
   - From ANY node (not just Heltec)
   - Decrypting with key #1 "AQ=="
   - Starting with **0x08 0x01** or **0x0A** (Field 1 = Data message)

## Why This Works Better

- **Mesh rebroadcasting**: Other nodes will forward your message, giving multiple capture opportunities
- **No processing bottleneck**: ESP32 processes packets as they come in queue
- **Catch routed copies**: See messages being forwarded through mesh

## Expected Output

When a text message is captured, you should see:
```
[PSK] First byte: 0x08 or 0x0A (Field 1)
[PSK] Type: DATA MESSAGE
📧 TEXT MESSAGE: "TEST1"
```

## Current Status

Your setup is working correctly:
- ✅ Decryption working (key #1 "AQ==")
- ✅ Capturing packets from Heltec
- ✅ Packet queue buffering active
- ❌ Missing text messages in capture window

The text messages ARE being transmitted successfully (you see them on LilyGo), we just need to catch them at the right moment.
