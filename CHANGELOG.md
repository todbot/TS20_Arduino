# Changelog

All notable changes to this library are documented in this file.

The format is based on [Keep a Changelog][kac], and this project adheres
to [Semantic Versioning][semver].

## [Unreleased]

## [0.6.0] - 2026-08-29

### Removed

- `Adafruit NeoPixel` is no longer listed in `depends`. It was only ever
  needed to build the `ts20_neopixel` example, never the library itself,
  so installing TS20 no longer pulls it in. Existing installs keep the
  library they already have; a fresh install that wants to compile
  `ts20_neopixel` should install "Adafruit NeoPixel" from the Library
  Manager.

## [0.5.0] - 2026-08-25

First release as a standalone library. For the previous two years the
driver lived as a sub-module of other projects.

### Added

- `TS20` class driving all 20 capacitive touch pads over I2C.
- `touched()` returns every pad as one 20-bit field, read in a single
  I2C transaction; `isTouched(pad)` tests one pad.
- Sensitivity is settable at runtime for every pad at once
  (`setSensitivity(value)`), for a single pad
  (`setSensitivity(value, pad)`), or from an array
  (`setSensitivities(values)`). 0 is most sensitive, 15 is least.
- `sensitivity(pad)` and `sensitivityPercent(pad)` read settings back
  without touching the bus, from a shadow copy of the registers.
- `noiseDetected()` exposes the chip's noise-detect flag.
- Configuration helpers for sensitivity step size, sensing rate,
  response time, first-touch window, sense impedance, and the raw
  `Cal_CTRL` / `Err_CTRL` registers.
- `reconfigure()` escape hatch for registers the driver does not expose.
- Examples: `ts20_simpletest` and `ts20_neopixel`.

### Fixed

Relative to the pre-release sub-module code:

- Touch reads masked bit 7 of `Output1`, which is reserved. When it read
  as 1 it was OR-ed into bit 7 and reported a phantom touch on pad 7.
- `getTouches()` shifted `int` values on 8-bit cores, where `int` is 16
  bits: pad 19 was silently dropped and pad 15 sign-extended into 17
  simultaneous touches. All shifts are now done in `uint32_t`.
- The upper nibble of Sensitivity register `0x0A` was written with a
  duplicate of pad 19's value. Those bits are empty and the datasheet
  register map requires zero, so pad 19 behaves differently than before.
- `Cal_HOLD2` was written twice and `Cal_HOLD3` never, leaving channel
  calibration for pads 14-19 unconfigured.
- `begin()` now returns `false` when no device acknowledges on the bus,
  instead of always reporting success.
- Register defines for `0x37`-`0x39` named `Rd_CH4`/`5`/`6` rather than
  repeating `Rd_CH1`/`2`/`3`.

### Notes

- Changing sensitivity is a single block write. Measured on hardware,
  the chip applies new values immediately, and asserting SRST neither
  clears the register file nor is needed to latch them.
  `setResetOnChange(true)` restores the full-reconfigure behaviour for
  boards that turn out to disagree; note that it soft-resets the chip,
  which restarts calibration and absorbs any touch already in progress
  into the new baseline.
- A sensitivity read-back helper was evaluated and dropped. On hardware
  the `Sensitivity_RD` register returned a constant regardless of the
  configured threshold, so it could not report what its name implied.

[kac]: https://keepachangelog.com/en/1.1.0/
[semver]: https://semver.org/spec/v2.0.0.html
[Unreleased]: https://github.com/todbot/TS20_Arduino/compare/0.6.0...HEAD
[0.6.0]: https://github.com/todbot/TS20_Arduino/releases/tag/0.6.0
[0.5.0]: https://github.com/todbot/TS20_Arduino/releases/tag/0.5.0
