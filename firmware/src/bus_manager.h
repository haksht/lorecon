#ifndef BUS_MANAGER_H
#define BUS_MANAGER_H

#include <SPI.h>
#include <vector>
#include <memory>
#include "boards/board_config.h"

class BusManager {
public:
    static BusManager& instance();

    SPIClass& ensureBus(const SpiBusConfig& config);
    bool reinitializeBus(const SpiBusConfig& config);

private:
    BusManager() = default;
    BusManager(const BusManager&) = delete;
    BusManager& operator=(const BusManager&) = delete;

    struct BusEntry {
        SpiBusConfig config;
        std::unique_ptr<SPIClass> spi;
        bool begun;
    };

    SPIClass& createBus(const SpiBusConfig& config);

    std::vector<BusEntry> buses;
};

#endif  // BUS_MANAGER_H
