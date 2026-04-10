// Compile packet_store.cpp into the native test binary.
//
// Define LOGGER_H before including the source so that when
// packet_store.cpp does #include "../logger.h" the include guard
// fires immediately and the real logger.h (which needs Stream/Serial)
// is skipped. The stub logger.h was already processed via the search path.
#include "logger.h"   // stub — defines Logger, NullLogger, getInstance()
#define LOGGER_H      // block any subsequent #include "../logger.h"
#include "repositories/packet_store.cpp"
