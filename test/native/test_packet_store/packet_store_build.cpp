// Compile packet_store.cpp into the native test binary.
// Include logger.h via the stub path first so the real logger.h
// (which references Arduino Stream/Serial) is blocked by the guard.
#include "logger.h"
#include "repositories/packet_store.cpp"
