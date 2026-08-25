// ts20_simpletest.ino -- Simple test of the TS20 capacitive touch sensor
//
// Prints out which of the 20 touch pads are being touched.
// Open the Serial Monitor at 115200 baud to see the output.
//
// Pads are numbered 0-19 (the datasheet calls them channels CS1-CS20).
// Sensitivity is really a touch threshold, so a lower number means the
// pad triggers more easily.

#include <TS20.h>
#include <Wire.h>

TS20 ts20;

void setup() {
  Serial.begin(115200);
  delay(100); // wait 100 msec after power up, as per the datasheet

  // use begin(TS20_ADDR_VDD) if the ADD pin is tied to VDD
  if (!ts20.begin()) {
    Serial.println("No TS20 found, check wiring!");
    while (1) {
      delay(10);
    }
  }

  // sensitivity is 0 (most sensitive) to 15 (least sensitive)
  ts20.setSensitivity(5);

  // and any single pad can be re-tuned at any time:
  // ts20.setSensitivity(10, 0);   // just pad 0, less sensitive

  Serial.print("pad 0 needs a ");
  Serial.print(ts20.sensitivityPercent(0));
  Serial.println("% capacitance change");
}

void loop() {
  uint32_t touches = ts20.touched(); // bit 0 is pad 0

  if (touches) {
    Serial.print("touched:");
    for (uint8_t pad = 0; pad < TS20_NUM_PADS; pad++) {
      if ((touches >> pad) & 1) {
        Serial.print(' ');
        Serial.print(pad);
      }
    }
    Serial.println();
  }

  delay(250); // small delay to keep from spamming the serial monitor
}
