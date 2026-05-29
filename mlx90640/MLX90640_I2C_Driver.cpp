/**
   @copyright (C) 2017 Melexis N.V.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/
// I2C transport for the Melexis driver, routed through ESPHome's i2c bus.
//
// Previously this used Arduino <Wire.h> directly and relied on ESPHome's old
// Arduino-Wire i2c backend having initialised the bus. ESPHome (2026.x) drives
// I2C through the IDF driver, so the global Wire object is never set up and
// every read NACKs. The MLX90640 component registers as an i2c::I2CDevice; it
// hands us its I2CBus via MLX90640_SetI2CBus() and we use it here. No Wire.
#include "MLX90640_I2C_Driver.h"
#include "esphome/components/i2c/i2c.h"

namespace {
esphome::i2c::I2CBus *mlx90640_i2c_bus = nullptr;
}  // namespace

void MLX90640_SetI2CBus(esphome::i2c::I2CBus *bus) { mlx90640_i2c_bus = bus; }

void MLX90640_I2CInit(void) {}

// Read nMemAddressRead 16-bit words starting at startAddress. Returns 0 on
// success, -1 on error.
int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data) {
  if (mlx90640_i2c_bus == nullptr)
    return -1;

  // Read in small chunks, re-issuing the address each time (matches the proven
  // original driver). A single large combined transaction — e.g. the 832-word
  // EEPROM dump or 834-word frame — stalls the IDF i2c driver long enough to
  // trip the task watchdog. Each chunk is its own short write(no-stop)+read.
  static const uint16_t MAX_WORDS_PER_READ = 16;  // 32 bytes per i2c transaction
  uint16_t words_done = 0;
  while (words_done < nMemAddressRead) {
    uint16_t chunk = nMemAddressRead - words_done;
    if (chunk > MAX_WORDS_PER_READ)
      chunk = MAX_WORDS_PER_READ;

    const uint16_t addr = startAddress + words_done;
    const uint8_t addr_buf[2] = {(uint8_t) (addr >> 8), (uint8_t) (addr & 0xFF)};

    // Atomic write-address-then-read with a repeated start. NOTE: ESPHome's
    // legacy write(stop=false)+read() does NOT work here — those wrappers
    // ignore `stop` and each issues its own STOP, so the MLX loses its address
    // pointer between the two and returns garbage. write_readv() is the one
    // primitive that keeps the connection open across the restart.
    uint8_t raw[MAX_WORDS_PER_READ * 2];
    if (mlx90640_i2c_bus->write_readv(slaveAddr, addr_buf, 2, raw, (size_t) chunk * 2) != esphome::i2c::ERROR_OK)
      return -1;

    // Bytes arrive MSB,LSB per word; assemble into host-order uint16.
    for (uint16_t i = 0; i < chunk; i++) {
      data[words_done + i] = ((uint16_t) raw[2 * i] << 8) | raw[2 * i + 1];
    }
    words_done += chunk;
  }
  return 0;
}

// I2C clock is configured on the ESPHome i2c bus (YAML `frequency:`); the
// Melexis API still calls this, so keep it as a no-op.
void MLX90640_I2CFreqSet(int freq) { (void) freq; }

// Write one 16-bit word to a 16-bit address, then read it back to verify.
int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
  if (mlx90640_i2c_bus == nullptr)
    return -1;

  const uint8_t buf[4] = {(uint8_t) (writeAddress >> 8), (uint8_t) (writeAddress & 0xFF), (uint8_t) (data >> 8),
                          (uint8_t) (data & 0xFF)};
  if (mlx90640_i2c_bus->write(slaveAddr, buf, 4, true) != esphome::i2c::ERROR_OK)
    return -1;

  uint16_t dataCheck = 0;
  if (MLX90640_I2CRead(slaveAddr, writeAddress, 1, &dataCheck) != 0)
    return -1;
  if (dataCheck != data)
    return -2;

  return 0;
}

// General I2C reset - sends 0x06 to the general-call address (0x00).
int MLX90640_I2CGeneralReset(void) {
  if (mlx90640_i2c_bus == nullptr)
    return -1;

  const uint8_t reset_cmd = 0x06;
  if (mlx90640_i2c_bus->write(0x00, &reset_cmd, 1, true) != esphome::i2c::ERROR_OK)
    return -1;
  return 0;
}
