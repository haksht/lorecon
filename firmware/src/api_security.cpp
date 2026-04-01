/**
 * API Security Implementation
 */

#include "api_security.h"
#include "logger.h"
#include <Preferences.h>
#include <esp_random.h>
#include <WiFi.h>

// Static member initialization
String APISecurity::apiToken = "";
bool APISecurity::initialized = false;
RateLimitBucket APISecurity::rateLimitBuckets[Config::Security::RATE_LIMIT_BUCKETS] = {};

void APISecurity::begin() {
    if (initialized) return;
    
    loadToken();
    
    if (apiToken.isEmpty()) {
        apiToken = generateToken();
        saveToken(apiToken);
        LOG_INFO("Generated new API token");
    } else {
        LOG_INFO("Loaded existing API token");
    }
    
    if (Config::Security::AUTH_ENABLED) {
        LOG_INFO("API authentication: ENABLED");
        LOG_INFO("Token: %s", apiToken.c_str());
        LOG_INFO("Use header: %s: <token>", Config::Security::AUTH_HEADER);
    } else {
        LOG_WARN("API authentication: DISABLED (demo mode)");
    }
    
    initialized = true;
}

String APISecurity::getToken() {
    if (!initialized) begin();
    return apiToken;
}

String APISecurity::regenerateToken() {
    apiToken = generateToken();
    saveToken(apiToken);
    LOG_INFO("API token regenerated: %s", apiToken.c_str());
    return apiToken;
}

bool APISecurity::isAuthenticated(AsyncWebServerRequest* request) {
    // If auth disabled, allow all requests
    if (!Config::Security::AUTH_ENABLED) {
        return true;
    }
    
    if (!initialized) begin();
    
    // Auto-trust private network clients ONLY when connected to an external network (STA mode).
    // In AP mode, anyone who connects to the device's WiFi gets a 192.168.4.x address,
    // so RFC 1918 trust would grant unauthenticated access to dangerous endpoints.
    wifi_mode_t wifiMode = WiFi.getMode();
    bool isStaConnected = (wifiMode == WIFI_STA || wifiMode == WIFI_AP_STA) && WiFi.isConnected();

    if (isStaConnected) {
        IPAddress clientIP = request->client()->remoteIP();
        // Only trust clients on the STA network (not the AP subnet 192.168.4.x)
        bool isAPSubnet = (clientIP[0] == 192 && clientIP[1] == 168 && clientIP[2] == 4);
        if (!isAPSubnet) {
            // RFC 1918 check: client is on user's trusted LAN
            bool isPrivate = (clientIP[0] == 10) ||
                             (clientIP[0] == 172 && clientIP[1] >= 16 && clientIP[1] <= 31) ||
                             (clientIP[0] == 192 && clientIP[1] == 168);
            if (isPrivate) {
                return true;
            }
        }
    }
    
    // Helper: constant-time token comparison.
    // Token is always TOKEN_LENGTH hex chars  -  reject wrong length upfront
    // (length is public knowledge, not secret).
    auto validateToken = [](const String& provided) -> bool {
        if (provided.length() != Config::Security::TOKEN_LENGTH) {
            return false;
        }
        // Constant-time comparison over fixed length
        volatile uint8_t result = 0;
        for (size_t i = 0; i < Config::Security::TOKEN_LENGTH; i++) {
            result |= (provided[i] ^ apiToken[i]);
        }
        return result == 0;
    };

    // Check for token in header
    if (request->hasHeader(Config::Security::AUTH_HEADER)) {
        String providedToken = request->header(Config::Security::AUTH_HEADER);
        providedToken.trim();
        if (validateToken(providedToken)) return true;
    }

    // Check for token in query parameter (fallback for testing)
    if (request->hasParam("token")) {
        String providedToken = request->getParam("token")->value();
        providedToken.trim();
        if (validateToken(providedToken)) return true;
    }

    return false;
}

void APISecurity::sendUnauthorized(AsyncWebServerRequest* request) {
    char response[192];
    snprintf(response, sizeof(response),
        "{\"status\":\"error\",\"error\":\"Unauthorized\","
        "\"message\":\"Protected endpoint requires authentication. "
        "Include header: %s: <token>\"}", Config::Security::AUTH_HEADER);
    request->send(401, "application/json", response);
}

bool APISecurity::isEnabled() {
    return Config::Security::AUTH_ENABLED;
}

void APISecurity::boundReplayParams(uint8_t repeatCount, uint16_t delayMs,
                                     uint8_t& outRepeatCount, uint16_t& outDelayMs) {
    // Apply security bounds
    outRepeatCount = repeatCount;
    if (outRepeatCount > Config::Security::MAX_REPLAY_COUNT) {
        outRepeatCount = Config::Security::MAX_REPLAY_COUNT;
        LOG_WARN("Replay count bounded to %d", outRepeatCount);
    }
    if (outRepeatCount < 1) {
        outRepeatCount = 1;
    }
    
    outDelayMs = delayMs;
    if (outDelayMs > Config::Security::MAX_REPLAY_DELAY_MS) {
        outDelayMs = Config::Security::MAX_REPLAY_DELAY_MS;
        LOG_WARN("Replay delay bounded to %d ms", outDelayMs);
    }
    if (outDelayMs < 100) {
        outDelayMs = 100;  // Minimum 100ms between replays
    }
}

void APISecurity::loadToken() {
    Preferences prefs;
    prefs.begin(Config::Security::NVS_NAMESPACE, true);  // Read-only
    apiToken = prefs.getString(Config::Security::NVS_KEY_TOKEN, "");
    prefs.end();
}

void APISecurity::saveToken(const String& token) {
    Preferences prefs;
    prefs.begin(Config::Security::NVS_NAMESPACE, false);  // Read-write
    prefs.putString(Config::Security::NVS_KEY_TOKEN, token);
    prefs.end();
}

String APISecurity::generateToken() {
    // Generate cryptographically random token
    char token[Config::Security::TOKEN_LENGTH + 1];
    
    for (int i = 0; i < Config::Security::TOKEN_LENGTH; i += 2) {
        uint8_t byte = esp_random() & 0xFF;
        snprintf(&token[i], 3, "%02x", byte);
    }
    token[Config::Security::TOKEN_LENGTH] = '\0';
    
    return String(token);
}

uint32_t APISecurity::hashIP(IPAddress ip) {
    // Simple hash for IP address
    return (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
}

bool APISecurity::checkRateLimit(AsyncWebServerRequest* request) {
    uint32_t now = millis();
    IPAddress clientIP = request->client()->remoteIP();
    uint32_t ipHash = hashIP(clientIP);
    
    // Find existing bucket or oldest bucket to reuse
    int bucketIdx = -1;
    uint32_t oldestTime = UINT32_MAX;
    int oldestIdx = 0;
    
    for (int i = 0; i < Config::Security::RATE_LIMIT_BUCKETS; i++) {
        if (rateLimitBuckets[i].ipHash == ipHash) {
            bucketIdx = i;
            break;
        }
        if (rateLimitBuckets[i].windowStart < oldestTime) {
            oldestTime = rateLimitBuckets[i].windowStart;
            oldestIdx = i;
        }
    }
    
    // Use existing bucket or allocate oldest one
    if (bucketIdx < 0) {
        bucketIdx = oldestIdx;
        rateLimitBuckets[bucketIdx].ipHash = ipHash;
        rateLimitBuckets[bucketIdx].requestCount = 0;
        rateLimitBuckets[bucketIdx].windowStart = now;
    }
    
    RateLimitBucket& bucket = rateLimitBuckets[bucketIdx];
    
    // Check if window has expired
    if (now - bucket.windowStart > Config::Security::RATE_LIMIT_WINDOW_MS) {
        bucket.requestCount = 0;
        bucket.windowStart = now;
    }
    
    // Check rate limit
    if (bucket.requestCount >= Config::Security::RATE_LIMIT_REQUESTS) {
        LOG_WARN("Rate limit exceeded for %d.%d.%d.%d", 
                 clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
        return false;
    }
    
    bucket.requestCount++;
    return true;
}

void APISecurity::sendRateLimited(AsyncWebServerRequest* request) {
    char response[128];
    snprintf(response, sizeof(response),
        "{\"status\":\"error\",\"error\":\"Too Many Requests\","
        "\"message\":\"Rate limit exceeded. Max %d requests per second.\"}",
        Config::Security::RATE_LIMIT_REQUESTS);
    request->send(429, "application/json", response);
}
