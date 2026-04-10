// Compile device_repository.cpp into the native test binary.
// Include logger.h via the stub path first so the real logger.h
// (which references Arduino Stream/Serial) is blocked by the guard.
#include "logger.h"
#include "repositories/device_repository.cpp"
