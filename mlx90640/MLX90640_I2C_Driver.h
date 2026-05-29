/**
 * @copyright (C) 2017 Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _MLX90640_I2C_Driver_H_
#define _MLX90640_I2C_Driver_H_

#include <stdint.h>
#include "MLX90640_API.h"

// The MLX90640 component (an i2c::I2CDevice) hands the Melexis driver its
// ESPHome I2C bus via this setter; all transfers below go through it instead
// of Arduino Wire. Must be called before any other MLX90640_I2C* call.
namespace esphome {
namespace i2c {
class I2CBus;
}  // namespace i2c
}  // namespace esphome
extern void MLX90640_SetI2CBus(esphome::i2c::I2CBus *bus);

extern void MLX90640_I2CInit(void);
extern int MLX90640_I2CGeneralReset(void);
extern int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data);
extern int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
extern void MLX90640_I2CFreqSet(int freq);
#endif
