/**
 * Minimal Arduino.h stub for native (host-side) unit tests.
 *
 * Provides just enough of the Arduino API so that firmware headers
 * (config.h, data_structures.h, protocol_analyzer.h, etc.) compile
 * on a desktop compiler without the real Arduino SDK.
 *
 * Only types and functions actually referenced by testable code are
 * stubbed here. Add more as needed when new test targets are added.
 */

#ifndef ARDUINO_H_STUB
#define ARDUINO_H_STUB

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdarg>

// --- Arduino type aliases ---
using byte = uint8_t;

// --- Minimal String class ---
class String {
public:
    String() : buf_(nullptr), len_(0) {}
    String(const char* s) : buf_(nullptr), len_(0) {
        if (s) {
            len_ = strlen(s);
            buf_ = new char[len_ + 1];
            memcpy(buf_, s, len_ + 1);
        }
    }
    // Numeric constructors (used by FormatUtils)
    String(int val) : buf_(nullptr), len_(0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d", val);
        len_ = strlen(tmp);
        buf_ = new char[len_ + 1];
        memcpy(buf_, tmp, len_ + 1);
    }
    String(unsigned int val) : buf_(nullptr), len_(0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%u", val);
        len_ = strlen(tmp);
        buf_ = new char[len_ + 1];
        memcpy(buf_, tmp, len_ + 1);
    }
    String(unsigned long val) : buf_(nullptr), len_(0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%lu", val);
        len_ = strlen(tmp);
        buf_ = new char[len_ + 1];
        memcpy(buf_, tmp, len_ + 1);
    }
    String(const String& o) : buf_(nullptr), len_(0) {
        if (o.buf_) {
            len_ = o.len_;
            buf_ = new char[len_ + 1];
            memcpy(buf_, o.buf_, len_ + 1);
        }
    }
    ~String() { delete[] buf_; }

    String& operator=(const String& o) {
        if (this != &o) {
            delete[] buf_;
            buf_ = nullptr;
            len_ = 0;
            if (o.buf_) {
                len_ = o.len_;
                buf_ = new char[len_ + 1];
                memcpy(buf_, o.buf_, len_ + 1);
            }
        }
        return *this;
    }

    const char* c_str() const { return buf_ ? buf_ : ""; }
    unsigned int length() const { return len_; }

    String& operator+=(const char* s) {
        if (!s) return *this;
        size_t slen = strlen(s);
        char* nb = new char[len_ + slen + 1];
        if (buf_) memcpy(nb, buf_, len_);
        memcpy(nb + len_, s, slen + 1);
        delete[] buf_;
        buf_ = nb;
        len_ += slen;
        return *this;
    }

    String operator+(const char* s) const {
        String result(*this);
        result += s;
        return result;
    }

    String operator+(const String& o) const {
        String result(*this);
        result += o.c_str();
        return result;
    }

    char operator[](unsigned int i) const { return buf_ ? buf_[i] : 0; }

private:
    char* buf_;
    size_t len_;
};

// --- Time stub ---
inline unsigned long millis() { return 0; }

#endif // ARDUINO_H_STUB
