#include "bus_manager.h"
#include "driver/spi_common.h"
#include <Arduino.h>

namespace {
spi_host_device_t toHost(SpiHostId id) {
    return id == SpiHostId::HostFSPI ? SPI3_HOST : SPI2_HOST;
}
}

BusManager& BusManager::instance() {
    static BusManager manager;
    return manager;
}

SPIClass& BusManager::ensureBus(const SpiBusConfig& config) {
    for (auto& entry : buses) {
        if (entry.config == config) {
            if (!entry.begun) {
                entry.spi->begin(config.sck, config.miso, config.mosi, -1);
                entry.begun = true;
            }
            return *entry.spi;
        }
    }
    return createBus(config);
}

SPIClass& BusManager::createBus(const SpiBusConfig& config) {
    std::unique_ptr<SPIClass> spi(new SPIClass(toHost(config.host)));
    SPIClass& ref = *spi;
    spi->begin(config.sck, config.miso, config.mosi, -1);
    buses.push_back(BusEntry{config, std::move(spi), true});
    return ref;
}

bool BusManager::reinitializeBus(const SpiBusConfig& config) {
    Serial.printf("[BUS] Reinitializing SPI bus (SCK:%d MISO:%d MOSI:%d)\n",
                  config.sck, config.miso, config.mosi);
    
    for (auto& entry : buses) {
        if (entry.config == config && entry.begun) {
            Serial.println("[BUS] Ending existing bus...");
            entry.spi->end();
            delay(50);
            
            Serial.println("[BUS] Restarting bus...");
            entry.spi->begin(config.sck, config.miso, config.mosi, -1);
            entry.begun = true;
            Serial.println("[BUS] SPI bus reinitialized successfully");
            return true;
        }
    }
    
    Serial.println("[BUS] Bus not found for reinitialization");
    return false;
}
