# DeNada
Free  and Open Source AlphaSmart Neo alternative.

There's a wiki
https://github.com/libresmart/denada/wiki

Would you like to be a project maintainer?
Open an issue requesting maintainer privileges

## Current Hardware Requirements (subject to change based on feedback)
* ESP32
* SparkFun SerLCD - 20x4 (use qwiic for I2C, or modify the code accordingly)
* USB Keyboard
* (optional) USB connector port (to avoid damaging your keyboard's usb cable)

## Wiring
* I2C SDA: 18
* I2C SCL: 19
* USB DATA+: 22
* USB DATA-: 23
* there are 4 more wires you'll have to connect, but that's for power so just
  find something that has the right voltage

## Build and flash

With `arduino-cli` (no GUI needed, and this is what the project is developed
against):

```
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17
arduino-cli lib install "SparkFun SerLCD Arduino Library"
arduino-cli lib install "ESP32-USB-Soft-Host"

arduino-cli compile --fqbn esp32:esp32:esp32 arduino-alphasmart
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 arduino-alphasmart
arduino-cli monitor -p /dev/cu.usbserial-0001 --config baudrate=115200
```

Your serial port will differ. If the upload cannot connect, hold the board's
BOOT button until "Connecting..." appears, then let go.

With the Arduino IDE, open `arduino-alphasmart/arduino-alphasmart.ino`, add the
board URL above under Preferences, install the `esp32` board package and the two
libraries, select "ESP32 Dev Module", and Upload.

## Tests

The editor core -- gap buffer, word wrap, cursor, selection, buffers, the key
map, the save format -- is plain C++ with no Arduino dependencies, so it builds
and runs on your development machine in about a second:

```
cd test && make
```

Nothing in `src/` should need a flash cycle to verify. Only USB, I2C, flash, and
the clock genuinely need the board, and those live in the `.ino`.

## Keys

| Key | Action |
| --- | --- |
| Arrow keys | Move by character, or by display line |
| Home / End | Start / end of the display line |
| Ctrl + Home / End | Start / end of the buffer |
| Page Up / Page Down | Move by one screen |
| Shift + any movement | Extend the selection |
| Backspace / Delete | Delete before / at the cursor |
| Ctrl + A | Select all |
| Ctrl + C / X / V | Copy / cut / paste |
| F1 - F8 | Switch to buffer 1 - 8 |
| F9 | Send the current buffer to the host |

Text wraps at word boundaries, the screen follows the cursor, and a word longer
than the screen is broken at the edge. Held keys repeat.

Because a 20x4 character LCD cannot highlight a range, an active selection is
reported by size on the bottom row rather than shown in place.

## Storage

Eight buffers of 8184 bytes each live in RAM at once, which is what makes
switching between them instant. They are written to the ESP32's own flash
(LittleFS, one file per buffer) once typing has been idle for two seconds, so
text survives a power cut but steady typing does not thrash the flash.

Buffer size and count are `GAP_BUFFER_RAW_SIZE` and `BUFFER_COUNT` in
`src/config.h`. The current build uses 26% of program storage and 28% of RAM,
so there is room to raise them; `capacity` is a `uint16_t`, so keep
`GAP_BUFFER_RAW_SIZE` at or below 65535.

## Caveats
* ~~Only printable characters and backspace are recognized.~~
* ~~The LCD screen does not scroll.~~
* ~~Cursor navigation not yet supported.~~
* ~~Does not save/load/export buffers~~
* Sending a buffer (F9) writes it out the serial port, not as a USB keyboard.
  **The classic ESP32-WROOM cannot present as a USB keyboard at all** -- it has
  no USB device peripheral. The socket on the dev board is a CP2102 UART bridge,
  and `ESP32-USBSoftHost` bit-bangs *host* mode, which is the opposite
  direction. Keyboard output needs a chip with native USB OTG, meaning an
  ESP32-S2 or S3. The transport is abstracted in `src/text_output.h` and the
  HID implementation is there, compiled only for those targets and **not
  verified on hardware.**
* No UTF-8; the buffer is bytes and the LCD is a character device.
