/*!
 * @file TS20.h
 *
 * Arduino library for the AD Semiconductor / TouchSemi TS20
 * 20-channel capacitive touch sensor.
 *
 * Pads are numbered 0-19 in this library; the datasheet calls them
 * channels CS1-CS20.  Sensitivity is 0-15, where 0 is the *most*
 * sensitive and 15 the least (it is really a touch threshold).
 *
 * Written by Tod Kurt (@todbot).
 *
 * Ported from the CircuitPython TS20 library:
 *   https://github.com/todbot/CircuitPython_TS20
 * Configuration information from:
 *   https://github.com/yni2yni/TS20
 *
 * MIT license, all text above must be included in any redistribution.
 */

#ifndef TS20_H
#define TS20_H

#include "Arduino.h"
#include <Wire.h>

#define TS20_ADDR_GND 0x6A ///< I2C address with the ADD pin tied to GND
#define TS20_ADDR_VDD 0x7A ///< I2C address with the ADD pin tied to VDD
#define TS20_ADDR_DEFAULT TS20_ADDR_GND ///< default I2C address

#define TS20_NUM_PADS 20 ///< number of touch pads on the chip

// Registers.  Sensitivity/PWM1..11 are contiguous from 0x00 to 0x0A.
#define TS20_SEN_PWM1 0x00     ///< Sensitivity/PWM1, ch2 & ch1
#define TS20_GTRL1 0x0B        ///< General Control 1
#define TS20_GTRL2 0x0C        ///< General Control 2
#define TS20_CAL_CTRL 0x0D     ///< Calibration speed control
#define TS20_PORT_CTRL1 0x0E   ///< Port Control 1, ch1-ch4
#define TS20_PORT_CTRL6 0x13   ///< Port Control 6, ch20
#define TS20_CAL_HOLD1 0x14    ///< Channel calibration control, ch1-ch7
#define TS20_CAL_HOLD2 0x15    ///< Channel calibration control, ch8-ch14
#define TS20_CAL_HOLD3 0x16    ///< Channel calibration control, ch15-ch20
#define TS20_ERR_CTRL 0x17     ///< Noise environment overcome control
#define TS20_OUTPUT1 0x20      ///< Touch output, ch1-ch7
#define TS20_OUTPUT2 0x21      ///< Touch output, ch8-ch15
#define TS20_OUTPUT3 0x22      ///< Touch output, ch16-ch20 plus noise flag
#define TS20_REF_WR_H 0x23     ///< Reference count write, high byte
#define TS20_REF_WR_L 0x24     ///< Reference count write, low byte
#define TS20_REF_WR_CH1 0x25   ///< Reference count write channel select 1
#define TS20_REF_WR_CH2 0x26   ///< Reference count write channel select 2
#define TS20_REF_WR_CH3 0x27   ///< Reference count write channel select 3
#define TS20_SEN_RD_CTRL 0x28  ///< Sensitivity read channel select
#define TS20_SEN_RD 0x29       ///< Sensitivity read data
#define TS20_RD_CH1 0x30       ///< Sense/ref count channel flags 1
#define TS20_RD_CH2 0x31       ///< Sense/ref count channel flags 2
#define TS20_RD_CH3 0x32       ///< Sense/ref count channel flags 3
#define TS20_SEN_H 0x33        ///< Sense count, high byte
#define TS20_SEN_L 0x34        ///< Sense count, low byte
#define TS20_REF_H 0x35        ///< Reference count, high byte
#define TS20_REF_L 0x36        ///< Reference count, low byte
#define TS20_RD_CH4 0x37       ///< Sense/ref count channel flags, recheck 1
#define TS20_RD_CH5 0x38       ///< Sense/ref count channel flags, recheck 2
#define TS20_RD_CH6 0x39       ///< Sense/ref count channel flags, recheck 3

// GTRL2 bit fields (datasheet 8.2.3)
#define TS20_GTRL2_IMP_SEL 0x10       ///< 1 = high sense impedance
#define TS20_GTRL2_SRST 0x08          ///< 1 = hold digital block in reset
#define TS20_GTRL2_RB_SEL_NORMAL 0x02 ///< internal clock speed, 10 = normal

/*! Calibration speed, hand-tuned; see datasheet 8.2.4 */
#define TS20_DEFAULT_CAL_CTRL 0xAF
/*! Noise rejection, 0.7% level and 3 counts; see datasheet 8.2.7 */
#define TS20_DEFAULT_ERR_CTRL 0x0F
/*! Least sensitive, a safe starting point for most pad geometries */
#define TS20_DEFAULT_SENSITIVITY 15

/*!
 * @brief Driver for the TS20 capacitive touch sensor.
 */
class TS20 {
public:
  TS20();

  bool begin(uint8_t i2caddr = TS20_ADDR_DEFAULT, TwoWire *theWire = &Wire);
  void reset();

  uint32_t touched();
  bool isTouched(uint8_t pad);
  bool noiseDetected();

  /*!
   * @brief Deprecated alias for touched(), kept for older sketches.
   * @return Bit field of touched pads, bit 0 is pad 0.
   */
  uint32_t getTouches() { return touched(); }

  void setSensitivity(uint8_t value);
  void setSensitivity(uint8_t value, uint8_t pad);
  void setSensitivities(const uint8_t *values);
  uint8_t sensitivity(uint8_t pad);
  float sensitivityPercent(uint8_t pad);
  float readSensitivityPercent(uint8_t pad);

  void setFineSteps(bool fineSteps);
  void setFastMode(bool fastMode);
  void setResponseTime(uint8_t responseTime);
  void setFirstTouchTime(uint8_t firstTouchTime);
  void setHighImpedance(bool highImpedance);
  void setCalCtrl(uint8_t calCtrl);
  void setErrCtrl(uint8_t errCtrl);

  /*!
   * @brief Control whether sensitivity changes trigger a full reset.
   *
   * The chip may only latch sensitivity writes across a soft reset, and
   * that reset may clear the rest of the register file with them, so by
   * default every change rewrites the whole configuration.  Turning this
   * off makes changes a single block write, which avoids the brief
   * recalibration blank but may not take effect on all boards.
   *
   * @param resetOnChange true (default) to reconfigure on every change.
   */
  void setResetOnChange(bool resetOnChange) { _resetOnChange = resetOnChange; }

  bool reconfigure(const uint8_t *configInfo, uint16_t len);

private:
  void writeRegister(uint8_t reg, uint8_t val);
  bool readBlock(uint8_t start, uint8_t *buf, uint8_t len);
  void writeBlock(uint8_t start, const uint8_t *data, uint8_t len);
  uint32_t readOutputs();
  void packSensitivity(uint8_t *buf);
  void writeSensitivity();
  void updateGtrl1();
  void updateGtrl2();

  TwoWire *_wire;   ///< I2C bus the chip is on
  uint8_t _i2caddr; ///< I2C address of the chip
  bool _begun;      ///< true once begin() has run

  uint8_t _sens[TS20_NUM_PADS]; ///< shadow copy of per-pad sensitivities
  uint8_t _gtrl1;               ///< cached General Control 1 value
  uint8_t _gtrl2;               ///< cached General Control 2 run value
  uint8_t _calCtrl;             ///< cached Cal_CTRL value
  uint8_t _errCtrl;             ///< cached Err_CTRL value

  bool _fineSteps;         ///< GTRL1 SSC, false = normal 0.2% steps
  bool _fastMode;          ///< GTRL1 MS, true = always fast sensing
  uint8_t _responseTime;   ///< GTRL1 RTC, sense periods before reporting
  uint8_t _firstTouchTime; ///< GTRL1 FTC, fast-calibration window
  bool _highImpedance;     ///< GTRL2 IMP_SEL
  bool _resetOnChange;     ///< reconfigure on every sensitivity change
};

#endif // TS20_H
