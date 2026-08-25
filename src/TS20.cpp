/*!
 * @file TS20.cpp
 *
 * @mainpage Arduino library for the TS20 capacitive touch sensor
 *
 * @section intro_sec Introduction
 *
 * Driver for the AD Semiconductor / TouchSemi TS20, a 20-channel
 * capacitive touch sensor with automatic sensitivity calibration.
 * Sensitivity can be set for every pad at once or tuned per pad at
 * runtime.
 *
 * @section author Author
 *
 * Written by Tod Kurt (\@todbot).
 *
 * @section license License
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include "TS20.h"

/*!
 * Which pad's sensitivity lives in the low nibble of each
 * Sensitivity/PWM register.
 */
static const uint8_t sens_lo[11] = {0, 2, 4, 6, 7, 9, 11, 13, 15, 17, 19};

/*!
 * Which pad's sensitivity lives in the high nibble.  0xFF marks the two
 * registers whose channel sits alone: ch7 at 0x03 and ch20 at 0x0A.
 */
static const uint8_t sens_hi[11] = {1, 3, 5, 0xFF, 8, 10, 12, 14, 16, 18, 0xFF};

/*!
 * Value for an unused high nibble.  Register 0x03's reserved bits want
 * "1111" (datasheet 8.2.1); register 0x0A's empty bits want zero
 * (register map note 2).
 */
static const uint8_t sens_fill[11] = {0, 0, 0, 0xF0, 0, 0, 0, 0, 0, 0, 0x00};

/*!
 * @brief Construct a TS20 with the default configuration.
 *
 * No I2C traffic happens until begin() is called.
 */
TS20::TS20() {
  _wire = &Wire;
  _i2caddr = TS20_ADDR_DEFAULT;
  _begun = false;
  _fineSteps = false;
  _fastMode = true;
  _responseTime = 2;
  _firstTouchTime = 1;
  _highImpedance = false;
  _calCtrl = TS20_DEFAULT_CAL_CTRL;
  _errCtrl = TS20_DEFAULT_ERR_CTRL;
  _resetOnChange = false;
  for (uint8_t i = 0; i < TS20_NUM_PADS; i++) {
    _sens[i] = TS20_DEFAULT_SENSITIVITY;
  }
  updateGtrl1();
  updateGtrl2();
}

/*!
 * @brief Start talking to the TS20 and write the full configuration.
 * @param i2caddr I2C address, TS20_ADDR_GND (default) or TS20_ADDR_VDD.
 * @param theWire TwoWire bus to use, defaults to &Wire.
 * @return true if a device acknowledged at that address.
 */
bool TS20::begin(uint8_t i2caddr, TwoWire *theWire) {
  _i2caddr = i2caddr;
  _wire = theWire;
  _wire->begin();

  // make sure something is actually out there before configuring it
  _wire->beginTransmission(_i2caddr);
  if (_wire->endTransmission() != 0) {
    return false;
  }

  _begun = true;
  reset();
  return true;
}

/*!
 * @brief Soft reset the TS20 and write the full configuration.
 *
 * Asserting SRST may clear the register file, so everything is written
 * again between asserting and releasing it.
 */
void TS20::reset() {
  // Hold the digital block in reset while reconfiguring.  The vendor
  // sequence enters reset with high impedance selected.
  writeRegister(TS20_GTRL2, _gtrl2 | TS20_GTRL2_SRST | TS20_GTRL2_IMP_SEL);

  // All ports to capsense (as opposed to LED driver or tact switch)
  for (uint8_t reg = TS20_PORT_CTRL1; reg <= TS20_PORT_CTRL6; reg++) {
    writeRegister(reg, 0x00);
  }

  uint8_t buf[11];
  packSensitivity(buf);
  writeBlock(TS20_SEN_PWM1, buf, 11);

  writeRegister(TS20_GTRL1, _gtrl1);
  writeRegister(TS20_CAL_HOLD1, 0x00); // calibration on, ch1-ch7
  writeRegister(TS20_CAL_HOLD2, 0x00); // calibration on, ch8-ch14
  writeRegister(TS20_CAL_HOLD3, 0x00); // calibration on, ch15-ch20
  writeRegister(TS20_ERR_CTRL, _errCtrl);
  writeRegister(TS20_CAL_CTRL, _calCtrl);
  writeRegister(TS20_GTRL2, _gtrl2); // release reset
}

/*!
 * @brief Touch state of all pads.
 * @return Bit field where bit 0 is pad 0, up to bit 19 for pad 19.
 */
uint32_t TS20::touched() { return readOutputs() & 0xFFFFFUL; }

/*!
 * @brief Test one pad.
 * @param pad Pad number, 0 to 19.
 * @return true if that pad is being touched, false if not or out of range.
 */
bool TS20::isTouched(uint8_t pad) {
  if (pad >= TS20_NUM_PADS) {
    return false;
  }
  return (touched() >> pad) & 1;
}

/*!
 * @brief Read the chip's noise-detect flag.
 * @return true if the TS20 reports a noisy environment.
 */
bool TS20::noiseDetected() { return (readOutputs() >> 20) & 1; }

/*!
 * @brief Set the sensitivity of every pad.
 * @param value 0 (most sensitive) to 15 (least sensitive).
 */
void TS20::setSensitivity(uint8_t value) {
  for (uint8_t i = 0; i < TS20_NUM_PADS; i++) {
    _sens[i] = value & 0x0F;
  }
  writeSensitivity();
}

/*!
 * @brief Set the sensitivity of a single pad.
 * @param value 0 (most sensitive) to 15 (least sensitive).
 * @param pad Pad number, 0 to 19.  Out of range values are ignored.
 */
void TS20::setSensitivity(uint8_t value, uint8_t pad) {
  if (pad >= TS20_NUM_PADS) {
    return;
  }
  _sens[pad] = value & 0x0F;
  writeSensitivity();
}

/*!
 * @brief Set every pad's sensitivity from an array.
 * @param values Array of TS20_NUM_PADS values, each 0 to 15.
 */
void TS20::setSensitivities(const uint8_t *values) {
  for (uint8_t i = 0; i < TS20_NUM_PADS; i++) {
    _sens[i] = values[i] & 0x0F;
  }
  writeSensitivity();
}

/*!
 * @brief Get the sensitivity currently set for a pad.
 * @param pad Pad number, 0 to 19.
 * @return Sensitivity 0 to 15, or 0 if the pad is out of range.
 */
uint8_t TS20::sensitivity(uint8_t pad) {
  if (pad >= TS20_NUM_PADS) {
    return 0;
  }
  return _sens[pad];
}

/*!
 * @brief Convert a pad's sensitivity setting to a percentage.
 * @param pad Pad number, 0 to 19.
 * @return The percent change in pad capacitance needed to register a
 *         touch (datasheet 8.2.1), or -1.0 if the pad is out of range.
 */
float TS20::sensitivityPercent(uint8_t pad) {
  if (pad >= TS20_NUM_PADS) {
    return -1.0f;
  }
  if (_fineSteps) {
    return _sens[pad] * 0.1f + 0.05f;
  }
  return _sens[pad] * 0.2f + 0.15f;
}

/*!
 * @brief Select the sensitivity step size.
 * @param fineSteps false (default) for normal 0.2% steps, true for
 *        fine 0.1% steps.
 */
void TS20::setFineSteps(bool fineSteps) {
  _fineSteps = fineSteps;
  updateGtrl1();
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Select the sensing rate.
 * @param fastMode true (default) to always sense at the fast rate,
 *        false to alternate fast and slow to save power.
 */
void TS20::setFastMode(bool fastMode) {
  _fastMode = fastMode;
  updateGtrl1();
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Set how long a touch must persist before it is reported.
 * @param responseTime Number of sense periods, 0 to 7.
 */
void TS20::setResponseTime(uint8_t responseTime) {
  _responseTime = responseTime & 0x07;
  updateGtrl1();
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Set the length of the fast-calibration window after a reset.
 * @param firstTouchTime 0 to 3, meaning 13, 25, 50 or 100 * 16 periods.
 */
void TS20::setFirstTouchTime(uint8_t firstTouchTime) {
  _firstTouchTime = firstTouchTime & 0x03;
  updateGtrl1();
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Select the sense input impedance.
 * @param highImpedance true for high impedance (more sensitive, noisier),
 *        false (default) for low.
 */
void TS20::setHighImpedance(bool highImpedance) {
  _highImpedance = highImpedance;
  updateGtrl2();
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Set the raw calibration speed register.
 * @param calCtrl Cal_CTRL register value, see datasheet 8.2.4.
 */
void TS20::setCalCtrl(uint8_t calCtrl) {
  _calCtrl = calCtrl;
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Set the raw noise rejection register.
 * @param errCtrl Err_CTRL register value, see datasheet 8.2.7.
 */
void TS20::setErrCtrl(uint8_t errCtrl) {
  _errCtrl = errCtrl;
  if (_begun) {
    reset();
  }
}

/*!
 * @brief Write an arbitrary set of registers.
 *
 * Escape hatch for registers this driver does not expose.  Note that a
 * later reset() will overwrite anything reset() itself manages.
 *
 * @param configInfo Array of alternating register address and value bytes.
 * @param len Length of configInfo in bytes, must be even.
 * @return true if the data was written, false if len was odd.
 */
bool TS20::reconfigure(const uint8_t *configInfo, uint16_t len) {
  if (len & 1) {
    return false;
  }
  for (uint16_t i = 0; i < len; i += 2) {
    writeRegister(configInfo[i], configInfo[i + 1]);
  }
  return true;
}

/*!
 * @brief Write one 8-bit value to a register.
 * @param reg Register address.
 * @param val Value to write.
 */
void TS20::writeRegister(uint8_t reg, uint8_t val) {
  _wire->beginTransmission(_i2caddr);
  _wire->write(reg);
  _wire->write(val);
  _wire->endTransmission();
}

/*!
 * @brief Read a run of registers.
 * @param start First register address.
 * @param buf Buffer to read into.
 * @param len Number of bytes to read.
 * @return true if all bytes were read.
 */
bool TS20::readBlock(uint8_t start, uint8_t *buf, uint8_t len) {
  _wire->beginTransmission(_i2caddr);
  _wire->write(start);
  if (_wire->endTransmission() != 0) {
    return false;
  }
  if (_wire->requestFrom(_i2caddr, len) != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = _wire->read();
  }
  return true;
}

/*!
 * @brief Write a run of registers starting at an address.
 * @param start First register address.
 * @param data Bytes to write.
 * @param len Number of bytes.
 */
void TS20::writeBlock(uint8_t start, const uint8_t *data, uint8_t len) {
  _wire->beginTransmission(_i2caddr);
  _wire->write(start);
  for (uint8_t i = 0; i < len; i++) {
    _wire->write(data[i]);
  }
  _wire->endTransmission();
}

/*!
 * @brief Read the three Output registers as one bit field.
 * @return Bits 0-19 are pads 0-19, bit 20 is the noise-detect flag.
 */
uint32_t TS20::readOutputs() {
  uint8_t b[3] = {0, 0, 0};
  if (!readBlock(TS20_OUTPUT1, b, 3)) {
    return 0;
  }
  // Output1 bit7 is reserved/don't-care: mask it so it cannot bleed into
  // pad 7.  Output3 bits 6-7 are empty.  Every shift is done in 32 bits
  // because int is only 16 bits wide on AVR.
  return ((uint32_t)(b[0] & 0x7F)) | ((uint32_t)b[1] << 7) |
         ((uint32_t)(b[2] & 0x3F) << 15);
}

/*!
 * @brief Pack the 20 shadow values into the 11 Sensitivity/PWM registers.
 * @param buf Buffer of at least 11 bytes to pack into.
 */
void TS20::packSensitivity(uint8_t *buf) {
  for (uint8_t i = 0; i < 11; i++) {
    uint8_t hi = sens_fill[i];
    if (sens_hi[i] != 0xFF) {
      hi = _sens[sens_hi[i]] << 4;
    }
    buf[i] = hi | _sens[sens_lo[i]];
  }
}

/*!
 * @brief Write the sensitivity registers out to the chip.
 *
 * One block write is enough: measured on a TS20, the chip applies new
 * sensitivity values as soon as they are written, and asserting SRST
 * neither clears the register file nor is needed to latch them.
 * setResetOnChange(true) rewrites everything anyway, for boards that
 * turn out to disagree.
 */
void TS20::writeSensitivity() {
  if (!_begun) {
    return; // begin() will write them
  }
  if (_resetOnChange) {
    reset();
  } else {
    uint8_t buf[11];
    packSensitivity(buf);
    writeBlock(TS20_SEN_PWM1, buf, 11);
  }
}

/*!
 * @brief Rebuild the cached GTRL1 value (datasheet 8.2.2).
 */
void TS20::updateGtrl1() {
  _gtrl1 = ((_fineSteps ? 0 : 1) << 6) | ((_fastMode ? 1 : 0) << 5) |
           ((_firstTouchTime & 0x03) << 3) | (_responseTime & 0x07);
}

/*!
 * @brief Rebuild the cached GTRL2 run value (datasheet 8.2.3).
 */
void TS20::updateGtrl2() {
  _gtrl2 = (_highImpedance ? TS20_GTRL2_IMP_SEL : 0) | TS20_GTRL2_RB_SEL_NORMAL;
}
