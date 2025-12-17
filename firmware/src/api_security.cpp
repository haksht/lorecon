/**
 * API Security Implementation
 */

#include "api_security.h"
#include "logger.h"
#include <Preferences.h>
#include <esp_random.h>

// Static member initialization
String APISecurity::apiToken = "";
bool APISecurity::initialized = false;

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
    
    // Check for token in header
    if (request->hasHeader(Config::Security::AUTH_HEADER)) {
        String providedToken = request->header(Config::Security::AUTH_HEADER);
        providedToken.trim();
        
        // Constant-time comparison to prevent timing attacks
        if (providedToken.length() == apiToken.length()) {
            bool match = true;
            for (size_t i = 0; i < apiToken.length(); i++) {
                match &= (providedToken[i] == apiToken[i]);
            }
            if (match) return true;
        }
    }
    
    // Check for token in query parameter (fallback for testing)
    if (request->hasParam("token")) {
        String providedToken = request->getParam("token")->value();
        providedToken.trim();
        
        if (providedToken.length() == apiToken.length()) {
            bool match = true;
            for (size_t i = 0; i < apiToken.length(); i++) {
                match &= (providedToken[i] == apiToken[i]);
            }
            if (match) return true;
        }
    }
    
    return false;
}

void APISecurity::sendUnauthorized(AsyncWebServerRequest* request) {
    String response = "{\"status\":\"error\",\"error\":\"Unauthorized\",";
    response += "\"message\":\"Protected endpoint requires authentication. ";
    response += "Include header: " + String(Config::Security::AUTH_HEADER) + ": <token>\"}";
    
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
