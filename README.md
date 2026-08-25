# TS20 Arduino Library

Arduino library for the AD Semiconductor / TouchSemi **TS20**, a
20-channel capacitive touch sensor with automatic sensitivity
calibration, controlled over I2C.

A CircuitPython version of this driver lives at
[todbot/CircuitPython_TS20](https://github.com/todbot/CircuitPython_TS20)
and exposes the same API.

## Features

- Reads all 20 touch pads in a single I2C transaction
- Sensitivity settable for every pad at once, or **per pad**, at runtime
- Exposes the chip's noise-detect flag
- Response time, sensing rate, step size and sense impedance all
  configurable


## Installation

Install through the Arduino Library Manager by searching for "TS20", or
clone this repo into your `Arduino/libraries` folder.

## Usage

```cpp
#include <TS20.h>
#include <Wire.h>

TS20 ts20;

void setup() {
  Serial.begin(115200);
  delay(100);  // wait 100 msec after power up, as per the datasheet

  // begin() defaults to address 0x6A on the Wire bus. Pass an address
  // and a bus object to use a different one, e.g. &Wire1 on boards
  // that have a second I2C bus:
  // if (!ts20.begin(TS20_ADDR_DEFAULT, &Wire1)) {
  if (!ts20.begin()) {
    Serial.println("No TS20 found, check wiring!");
    while (1) { delay(10); }
  }

  // sensitivity is 0 (most sensitive) to 15 (least sensitive)
  ts20.setSensitivity(5);
}

void loop() {
  uint32_t touches = ts20.touched();   // bit 0 is pad 0

  for (uint8_t pad = 0; pad < TS20_NUM_PADS; pad++) {
    if ((touches >> pad) & 1) {
      Serial.print("pad "); Serial.print(pad); Serial.println(" touched!");
    }
  }
  delay(250);
}
```

Pads are numbered 0-19; the datasheet calls them channels CS1-CS20.

### Sensitivity

Sensitivity is really a touch threshold, so a **lower number means the
pad triggers more easily**. It can be changed at any time:

```cpp
ts20.setSensitivity(3);          // every pad
ts20.setSensitivity(10, 0);      // just pad 0, less sensitive
ts20.setSensitivities(values);   // array of TS20_NUM_PADS values

ts20.sensitivity(0);             // read a pad's setting back
ts20.sensitivityPercent(0);      // as a % change in pad capacitance
```

Changing sensitivity soft-resets the chip, because it may only latch the
new values across a reset, and that reset can clear the rest of the
configuration with them. The reset restarts calibration, which blanks
touches for a moment. If your board picks up new values without it,
`ts20.setResetOnChange(false)` turns the reset off and makes changes a
single block write.

### Other settings

```cpp
ts20.setFineSteps(true);       // 0.1% sensitivity steps instead of 0.2%
ts20.setFastMode(false);       // alternate fast/slow sensing to save power
ts20.setResponseTime(4);       // sense periods a touch must persist, 0-7
ts20.setHighImpedance(true);   // more sensitive, more noise
```

## Examples

- **ts20_simpletest** — prints touched pads to the Serial Monitor
- **ts20_neopixel** — lights an 8x8 NeoPixel grid from the pads
  (needs the Adafruit NeoPixel library)

## Hardware

The TS20 is a 28-pin I2C capacitive touch controller. 
Tie its `ADD` pin to GND for address `0x6A` (the default) or to VDD for `0x7A`.

* For an example board, see https://github.com/todbot/TS20_Test_Board/

## License

MIT, see [LICENSE](LICENSE).
