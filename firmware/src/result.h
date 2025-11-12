/**
 * Result<T> - Rust-inspired error handling for C++
 * 
 * Provides type-safe error handling without exceptions.
 * Forces caller to check for errors explicitly.
 * 
 * Usage:
 *   Result<float> getRSSI() {
 *       if (radio.isAvailable()) {
 *           return Result<float>::Ok(radio.readRSSI());
 *       } else {
 *           return Result<float>::Err("Radio not responding");
 *       }
 *   }
 * 
 *   auto result = getRSSI();
 *   if (result.isOk()) {
 *       float rssi = result.unwrap();
 *       Serial.printf("RSSI: %.1f dBm\n", rssi);
 *   } else {
 *       Serial.printf("Error: %s\n", result.error());
 *   }
 */

#ifndef RESULT_H
#define RESULT_H

#include <Arduino.h>

/**
 * Result<T> - Contains either a value (Ok) or an error message (Err)
 */
template<typename T>
class Result {
public:
    // Construct success result with value
    static Result Ok(T value) {
        Result r;
        r.success = true;
        r.value = value;
        return r;
    }
    
    // Construct error result with message
    static Result Err(const char* errorMsg) {
        Result r;
        r.success = false;
        r.errorMsg = errorMsg;
        return r;
    }
    
    // Check if result is Ok
    bool isOk() const { return success; }
    
    // Check if result is Err
    bool isErr() const { return !success; }
    
    // Get value (only call if isOk())
    T unwrap() const {
        if (!success) {
            // In production, log error and return default
            Serial.printf("FATAL: unwrap() called on Err result: %s\n", errorMsg);
            return T{};
        }
        return value;
    }
    
    // Get value or default if error
    T unwrapOr(T defaultValue) const {
        return success ? value : defaultValue;
    }
    
    // Get error message (only call if isErr())
    const char* error() const {
        return errorMsg;
    }
    
private:
    Result() : success(false), errorMsg(""), value(T{}) {}
    
    bool success;
    const char* errorMsg;
    T value;
};

/**
 * Result<void> - Specialization for operations that don't return a value
 * 
 * Usage:
 *   Result<void> initialize() {
 *       if (setupFailed) {
 *           return Result<void>::Err("Setup failed");
 *       }
 *       return Result<void>::Ok();
 *   }
 */
template<>
class Result<void> {
public:
    static Result Ok() {
        Result r;
        r.success = true;
        return r;
    }
    
    static Result Err(const char* errorMsg) {
        Result r;
        r.success = false;
        r.errorMsg = errorMsg;
        return r;
    }
    
    bool isOk() const { return success; }
    bool isErr() const { return !success; }
    
    const char* error() const { return errorMsg; }
    
    // No unwrap() for void
    
private:
    Result() : success(false), errorMsg("") {}
    
    bool success;
    const char* errorMsg;
};

/**
 * Convenience macros for early return on error
 */
#define TRY(expr) \
    do { \
        auto __result = (expr); \
        if (__result.isErr()) { \
            return Result<decltype(__result)::value_type>::Err(__result.error()); \
        } \
    } while(0)

#define TRY_VOID(expr) \
    do { \
        auto __result = (expr); \
        if (__result.isErr()) { \
            return Result<void>::Err(__result.error()); \
        } \
    } while(0)

#endif // RESULT_H
