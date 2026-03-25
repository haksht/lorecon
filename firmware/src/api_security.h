/**
 * API Security - Token-based Authentication for Protected Endpoints
 * 
 * Provides authentication for sensitive API endpoints:
 * - Token generation and storage in NVS
 * - Request validation via X-API-Token header
 * - Rate limiting for replay operations
 * - Per-IP rate limiting for DoS protection
 * 
 * Design: Simple token auth suitable for local network operation
 */

#ifndef API_SECURITY_H
#define API_SECURITY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

/**
 * Rate limiter bucket for tracking requests per IP
 */
struct RateLimitBucket {
    uint32_t ipHash;
    uint16_t requestCount;
    uint32_t windowStart;
};

/**
 * API Security Manager
 * 
 * Handles token generation, storage, validation, and rate limiting.
 * Token is generated on first boot and stored in NVS.
 */
class APISecurity {
public:
    /**
     * Initialize security system
     * Loads or generates API token from NVS
     */
    static void begin();
    
    /**
     * Get current API token
     * @return Token string (32 hex characters)
     */
    static String getToken();
    
    /**
     * Regenerate API token
     * Creates new random token and stores in NVS
     * @return New token string
     */
    static String regenerateToken();
    
    /**
     * Validate request authentication
     * Checks X-API-Token header against stored token
     * @param request HTTP request to validate
     * @return true if authenticated or auth disabled
     */
    static bool isAuthenticated(AsyncWebServerRequest* request);
    
    /**
     * Send 401 Unauthorized response
     * @param request HTTP request to respond to
     */
    static void sendUnauthorized(AsyncWebServerRequest* request);
    
    /**
     * Check if authentication is enabled
     * @return true if Config::Security::AUTH_ENABLED
     */
    static bool isEnabled();
    
    /**
     * Validate replay parameters within safe bounds
     * @param repeatCount Requested repeat count
     * @param delayMs Requested delay between repeats
     * @param outRepeatCount Bounded repeat count
     * @param outDelayMs Bounded delay
     */
    static void boundReplayParams(uint8_t repeatCount, uint16_t delayMs,
                                   uint8_t& outRepeatCount, uint16_t& outDelayMs);
    
    /**
     * Check if request is rate limited
     * Uses sliding window per IP address
     * @param request HTTP request to check
     * @return true if within rate limit, false if exceeded
     */
    static bool checkRateLimit(AsyncWebServerRequest* request);
    
    /**
     * Send 429 Too Many Requests response
     * @param request HTTP request to respond to
     */
    static void sendRateLimited(AsyncWebServerRequest* request);

private:
    static String apiToken;
    static bool initialized;
    static RateLimitBucket rateLimitBuckets[Config::Security::RATE_LIMIT_BUCKETS];
    
    static void loadToken();
    static void saveToken(const String& token);
    static String generateToken();
    static uint32_t hashIP(IPAddress ip);
};

/**
 * Macro: enforce rate limiting + authentication in a handler.
 * Returns 429 or 401 as appropriate and exits the calling function.
 */
#define REQUIRE_AUTH(request) \
    do { \
        if (!APISecurity::checkRateLimit(request)) { \
            APISecurity::sendRateLimited(request); \
            return; \
        } \
        if (!APISecurity::isAuthenticated(request)) { \
            APISecurity::sendUnauthorized(request); \
            return; \
        } \
    } while (0)

#endif // API_SECURITY_H
