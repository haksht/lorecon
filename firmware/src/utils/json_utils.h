/**
 * JSON Response Utilities
 * 
 * Standardized JSON response building to reduce code duplication
 * across API endpoints and WebSocket handlers.
 */

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>

namespace JsonUtils {

/**
 * Create a success response with optional message
 * 
 * @param message Optional success message
 * @return JSON string: {"status":"success","message":"..."}
 */
inline String success(const char* message = nullptr) {
    JsonDocument doc;
    doc["status"] = "success";
    if (message) {
        doc["message"] = message;
    }
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Create a success response with String message
 */
inline String success(const String& message) {
    return success(message.c_str());
}

/**
 * Create an error response
 * 
 * @param error Error message
 * @return JSON string: {"status":"error","error":"..."}
 */
inline String error(const char* error) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["error"] = error;
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Create an error response with String message
 */
inline String error(const String& error) {
    return JsonUtils::error(error.c_str());
}

/**
 * Create a success response with data object
 * Caller provides a lambda to populate the data
 * 
 * Example:
 *   return JsonUtils::successWithData([](JsonDocument& doc) {
 *       doc["devices"] = 5;
 *       doc["packets"] = 100;
 *   });
 */
template<typename F>
inline String successWithData(F populateData) {
    JsonDocument doc;
    doc["status"] = "success";
    populateData(doc);
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Create a JsonDocument pre-seeded with "status":"success".
 * Use when the caller needs to add more fields before serializing.
 */
inline JsonDocument successDoc() {
    JsonDocument doc;
    doc["status"] = "success";
    return doc;
}

/**
 * Create and serialize an error response with "status":"error".
 * Convenience for lock-failure and similar early-return paths.
 */
inline String errorResponse(const char* message) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = message;
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Serialize a JsonDocument to String (convenience wrapper)
 */
inline String serialize(JsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Create standard health check response
 */
inline String healthOk(const char* service = "LoRecon") {
    JsonDocument doc;
    doc["status"] = "ok";
    doc["service"] = service;
    String response;
    serializeJson(doc, response);
    return response;
}

} // namespace JsonUtils

#endif // JSON_UTILS_H
