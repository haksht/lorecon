# Contributing to ESP32 LoRa Sniffer

Thank you for your interest in contributing! This document provides guidelines and standards to ensure consistent, high-quality code.

## Quick Start

1. **Hardware Required**: Heltec WiFi LoRa 32 V3 or V4 (SX1262 + OLED), LilyGO T3-S3, or T-Beam Supreme
2. **Build System**: PlatformIO (not Arduino IDE)
3. **Testing**: Native Unity tests in `test/native/` (run by CI), plus PSK boot tests and real hardware validation

## Code Style

### C++ Firmware (`firmware/src/`)

#### Naming Conventions
- **Classes**: `PascalCase` (e.g., `PacketProcessor`, `RadioController`)
- **Methods/Functions**: `camelCase` (e.g., `processQueue()`, `handlePacket()`)
- **Constants**: `UPPER_SNAKE_CASE` in `Config::` namespaces
- **Member Variables**: No prefix, use `this->` only when shadowing

#### Architecture Patterns
- **Interface Segregation**: Use `IReconTool` interface to break circular dependencies
- **Repository Pattern**: Storage layer in `repositories/` directory
- **Atomic ISR Flags**: Use `std::atomic<bool>` for interrupt communication
- **No Exceptions**: Return `bool` for success/failure
- **Logging**: Use `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()` from `logger.h`

#### Memory Safety
- Use `strncpy` with explicit null termination
- Use fixed-size arrays (no `std::vector`) — the codebase avoids heap fragmentation
- When using Arduino `String` with printf, always call `.c_str()`

#### Config Constants
All magic numbers belong in `config.h` namespaces:
```cpp
namespace Config {
    namespace Hardware {
        constexpr uint8_t LORA_NSS = 7;  // Board-specific, see config.h
    }
    namespace Scanning {
        constexpr uint32_t DWELL_TIME_MS = 12000;
    }
}
```

### JavaScript Web App (`data/webapp/js/`)

#### Debug Pattern
Use the shared DEBUG configuration pattern:
```javascript
const DEBUG = {
    enabled: false,
    log: (...args) => DEBUG.enabled && console.log('[Tag]', ...args),
    warn: (...args) => console.warn('[Tag]', ...args),
    error: (...args) => console.error('[Tag]', ...args)
};
```

#### XSS Prevention
**Always** use `escapeHtml()` for user-controlled data:
```javascript
// ✅ Good
html += `<td>${escapeHtml(data.nodeId)}</td>`;

// ❌ Bad - XSS vulnerability
html += `<td>${data.nodeId}</td>`;
```

#### CSS Class Migration
Prefer CSS classes over inline styles:
```javascript
// ✅ Good
'<div class="emoji-large text-muted">📊</div>'

// ❌ Avoid
'<div style="font-size: 3rem; margin-bottom: 1rem;">📊</div>'
```

#### Shared Utilities
Use existing utilities instead of duplicating:
- `TableRenderer` - Data table generation
- `CardRenderer` - Metric/status cards
- `ModalRenderer` - Modal dialogs
- `renderPlaceholder()` - Empty states
- `renderErrorState()` - Error displays
- `handleError()` - Standardized error handling

### CSS (`data/webapp/css/style.css`)

- Use CSS variables from `:root`
- Follow existing section organization
- Add utility classes for repeated patterns

## API Responses

Use `JsonUtils` helpers for consistent JSON responses:
```cpp
// Success
request->send(200, "application/json", JsonUtils::success());

// Success with data
request->send(200, "application/json", JsonUtils::successWithData(jsonDoc));

// Error
request->send(400, "application/json", JsonUtils::error("Invalid request"));
```

## File Size Guidelines

| Component | Target | Max |
|-----------|--------|-----|
| C++ modules | ~200 lines | 300 lines |
| JS tab handlers | ~100 lines | 150 lines |
| Total webapp JS | ~3000 lines | 4000 lines |

If a file exceeds limits, extract new components.

## Commit Messages

Format: `type: brief description`

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`

Examples:
- `feat: add PSK key rotation support`
- `fix: XSS in packet table`
- `docs: update API reference for v2.3`

## Testing

1. **Native Unit Tests**: `pio test -e native` runs Unity tests in `test/native/` (also runs in CI on every push)
2. **PSK Boot Tests**: `PSKTests::runAll()` runs automatically on device boot
3. **Serial Monitor**: Watch for errors during operation
4. **Web UI Testing**: Check all tabs, actions, error states
5. **Python Tools**: Use `lorecon dev monitor --host <ip> --tui` for live visualizer testing (or `--demo` without hardware)

## Pull Request Checklist

- [ ] Code compiles without warnings (`-Werror` is enabled)
- [ ] Tested on real hardware (Heltec V3/V4, T3-S3, or T-Beam Supreme)
- [ ] No new inline styles in JavaScript
- [ ] All user data escaped with `escapeHtml()`
- [ ] Console.log calls use DEBUG system
- [ ] API endpoints documented in `docs/developers/API.md`
- [ ] No hardcoded magic numbers (use config)

## Questions?

Check these docs first:
- [ARCHITECTURE.md](docs/developers/ARCHITECTURE.md) - Design patterns
- [API.md](docs/developers/API.md) - HTTP/WebSocket endpoints
- [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) - Common issues

---

*This is a security research tool for educational purposes. Use responsibly.*
