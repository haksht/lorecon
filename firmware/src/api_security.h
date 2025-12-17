/**
 * API Security - Token-based Authentication for Protected Endpoints
 * 
 * Provides authentication for sensitive API endpoints:
 * - Token generation and storage in NVS
 * - Request validation via X-API-Token header
 * - Rate limiting for replay operations
 * 
 * Design: Simple token auth suitable for local network operation
 */

#ifndef API_SECURITY_H
#define API_SECURITY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

/**
 * API Security Manager
 * 
 * Handles token generation, storage, and validation.
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

private:
    static String apiToken;
    static bool initialized;
    
    static void loadToken();
    static void saveToken(const String& token);
    static String generateToken();
};

#endif // API_SECURITY_H
