# ESP32 Sniffer – Session Handoff

**Branch:** `feature/phone-app-integration`

## What changed this session
- Significant cleanup of legacy docs is staged (`ARCHITECTURE_REFACTOR_NOV11.md`, `ROADMAP.md`, etc. deleted; `README.md`, `API_REFERENCE.md`, `docs/BUILD_GUIDE.md`, and `docs/FEATURES.md` updated).
- Phone/web UI refreshed under `data/webapp/` (new `index.html`, `css/style.css`, `js/app.js`, etc.).
- Firmware web stack (`firmware/src/web_server.cpp/.h`, `api_controller.*`, `recon_service.*`, etc.) updated to support the new UI and better telemetry.
- Patched the vendored `ESPAsyncWebServer` library in `.pio/libdeps/default/ESPAsyncWebServer-esphome/src/AsyncWebSocket.cpp`:
  - `_queueMessage` / `_queueControl` now re-check `_client` to avoid races.
  - `_onData` rewritten to avoid overrunning lwIP buffers: it conditionally copies frames into a temp buffer and null-terminates safely before invoking handlers.
- Firmware compiles successfully (`platformio run` as of SHA `b2118ef...`).

## Current blocking issue
- **Crash persists when the phone connects/disconnects.** Serial log shows `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)` in `AsyncWebSocketClient::_onData` (~line 661) immediately after `WebSocket client #1 disconnected`.
  - EXCVADDR varies, indicating another invalid pointer access inside `_onData` after our temp-buffer patch.
  - Happens right after radio config switches to Meshtastic frequency targeting.

## Next debugging steps
1. Reflash firmware + filesystem to ensure latest patches are on device:
   ```powershell
   C:\Users\tim\.platformio\penv\Scripts\platformio.exe run --target upload
   C:\Users\tim\.platformio\penv\Scripts\platformio.exe run --target uploadfs
   ```
2. Use `pio device monitor` (enable exception decoder: `Ctrl+T`, `Ctrl+E`) while connecting from the phone UI. Capture the stack trace after crash for confirmation.
3. Inspect `AsyncWebSocketClient::_onData` again—particularly branches where `_queueControl` or `_pinfo` interact with `data` during disconnect—to ensure we never call `memcmp`/`strlen` on unmasked or freed data. Consider skipping JSON preview logging until this stabilizes.
4. Audit `firmware/src/web_server.cpp::handleWebSocketMessage` to make sure we’re not queuing broadcasts or manipulating the payload during disconnect.
5. Once no crash occurs, re-test the phone UI command set (status view, targeting, etc.).

## Key files to keep in mind
- `firmware/src/web_server.cpp` – WebSocket events/logging.
- `.pio/libdeps/default/ESPAsyncWebServer-esphome/src/AsyncWebSocket.cpp` – local library patch; beware `pio lib update` overwriting it.
- `data/webapp/js/app.js` – client sending WebSocket frames.

## Misc notes
- SD logging currently disabled due to missing card (harmless warnings on boot).
- Repeated deletions/updates are staged but not committed yet; coordinate before running `git clean` or updating libraries.
