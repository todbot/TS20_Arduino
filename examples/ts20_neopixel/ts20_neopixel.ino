// ts20_neopixel.ino -- Show TS20 touches on an 8x8 NeoPixel grid
//
// Test setup:
// - TS20 test board, with zero-ohm resistors and no caps on the pad
//   lines (low impedance)
// - wired to Raspberry Pi Pico pins GP4 & GP5 (I2C0, the Wire default
//   on the arduino-pico core)
// - 8x8 NeoPixel grid wired to Pico pin GP22
//
// Lights the LED mapped to each touched pad, and uses the last LED of
// the grid to show the chip's noise-detect flag.

#include <Adafruit_NeoPixel.h>
#include <TS20.h>
#include <Wire.h>

const int LED_PIN = 22;
const int NUM_LEDS = 64;
const int LED_BRIGHTNESS = 10;

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
TS20 ts20;

// maps a serpentine 8x8 LED grid to how the pads are laid out.
// index is pad number, value is LED number
const int pad_to_led[TS20_NUM_PADS] = {
    0, 15, 16, 31, 32, 1,  14, 17, 30, 33,
    2, 13, 18, 29, 34, 3,  12, 19, 28, 35,
};
const int NOISE_LED = 63; // last LED shows the "is noisy" flag

void setup() {
  Serial.begin(115200);
  Wire.setClock(400000); // run I2C at 400 kHz

  leds.begin();
  leds.setBrightness(LED_BRIGHTNESS);
  leds.clear();
  leds.show();

  delay(100);            // wait 100 msec after power up, as per datasheet
  if (!ts20.begin()) {   // initialize the TS20
    Serial.println("No TS20 found, check wiring!");
    while (1) {
      delay(10);
    }
  }
  ts20.setSensitivity(5); // 0 = most sensitive, 15 = least

  // pads that read too hot or too cold can be trimmed individually:
  // ts20.setSensitivity(8, 3);
}

void loop() {
  leds.clear();

  uint32_t touches = ts20.touched();
  Serial.print("touches: ");
  for (uint8_t pad = 0; pad < TS20_NUM_PADS; pad++) {
    bool touched = (touches >> pad) & 1;
    Serial.print(touched ? '1' : '0');
    if (touched) {
      leds.setPixelColor(pad_to_led[pad], 0xff00ff);
    }
  }

  if (ts20.noiseDetected()) {
    Serial.print(" NOISY");
    leds.setPixelColor(NOISE_LED, 0xff0000);
  }
  Serial.println();

  leds.show();
  delay(50);
}
