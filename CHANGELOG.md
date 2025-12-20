# Changelog

All notable changes to the ESP32 LoRa Sniffer project.

## [2.2.0] - 2024-12-20

### Security
- **API Authentication**: Token-based auth for destructive endpoints (`X-API-Token` header)
- **Device-unique AP passwords**: Format `recon-XXYYZZ` based on MAC address
- **NVS credential storage**: WiFi passwords stored securely, not in JSON files
- **XSS prevention**: All user input escaped in web UI
- **Input validation**: Bounds checking on all API parameters

### Added
- **AP_STA dual WiFi mode**: Device accessible via AP (192.168.4.1) even when connected to hotspot
- **Serial activation**: Commands ignored until Enter pressed (prevents phantom commands from USB noise)
- **Network map visualization**: Interactive threat-colored SVG with device clustering
- **Security scoring**: Multi-factor assessment (RSSI proximity, encryption, router role, traffic patterns)
- **GeoJSON/KML export**: Geographic data export for mapping tools
- **Position tracker tool**: `tools/position_tracker.py` for real-time device triangulation
- **Conference demo tools**: `tools/enhanced_live_visualizer.py` 5-panel dashboard

### Changed
- **Build hardening**: `-Werror` enabled for source files (warnings as errors)
- **Repository pattern**: Device tracking extracted to `DeviceRepository` with Welford's RSSI stats
- **Packet deduplication**: Hash-based filtering in `PacketStore`
- **Replay slots**: Converted to FIFO circular buffer
- **Web UI modular refactor**: Split into `toast.js`, `war-room.js`, `network-map.js`, `app.js`

### Fixed
- Boot logging and PSK banner display
- API token printf format (Arduino String `.c_str()` requirement)
- Documentation accuracy (18 discrepancies fixed)
- Unsigned comparison warning in `wifi_manager.cpp`

### Technical
- 26 frequency configs (Meshtastic, LoRaWAN, Helium, ISM)
- 23 PSK keys including leaked 2023 admin/debug keys
- 50 max targetable devices with LRU eviction
- 100-packet processing queue
- 14+ hours stable in targeting mode (631 packets, 44 devices)

## [2.0.0] - 2024-11-13

### Added
- Complete v2.0 architecture refactor
- Component-based design: `RadioController`, `PacketProcessor`, `LoRaReconTool`, `ReconState`
- `IReconTool` interface pattern for dependency inversion
- Atomic ISR flags for thread-safe packet reception
- Web UI served from LittleFS
- PCAP export with LoRa metadata

### Changed
- Removed all offensive/stress testing features (passive recon only)
- Config namespace organization (`Config::Hardware::*`, `Config::Scanning::*`)
- Dispatch table command handler (replaced if/else chains)

---

For detailed API documentation, see [API_REFERENCE.md](API_REFERENCE.md)  
For architecture details, see [docs/technical/ARCHITECTURE.md](docs/technical/ARCHITECTURE.md)
