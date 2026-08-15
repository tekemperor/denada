#ifndef CONFIG_H
#define CONFIG_H

// GPIO Pins
// Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
#define PIN_I2C_SDA 18
#define PIN_I2C_SCL 19
// usb_pins_config_t {DP_P0, DM_P0, DP_P1, DM_P1, DP_P2, DM_P2, DP_P3, DM_P3}
// Good pairs (16,17),(22,23),(18,19),(13,15) -- (-1,-1) to disable
#define PIN_USB_DATA_PLUS 22
#define PIN_USB_DATA_MINUS 23

// Storage (in bytes)
// GAP_BUFFER_RAW_SIZE is the whole on-flash record for one buffer, header
// included, so the writable text is GAP_BUFFER_DATA_SIZE. BUFFER_COUNT of them
// live in RAM at once, which is what makes buffer switching instant. At 8K x 8
// that is 64K of the ESP32's ~320K, measured against a baseline build that left
// 292K free. Both are safe to raise; capacity is a uint16_t, so keep
// GAP_BUFFER_RAW_SIZE at or below 65535.
#define GAP_BUFFER_RAW_SIZE 8192
#define BUFFER_COUNT 8
#define CLIPBOARD_SIZE 1024
#define BUFFER_NAME_SIZE 32
#define KEYBOARD_BUFFER_SIZE 256

// Persistence
// Buffers are written to flash once typing has been idle this long, so a power
// loss costs at most this much work, and steady typing does not thrash flash.
#define AUTOSAVE_IDLE_MILLISECONDS 2000
#define STORAGE_PATH_FORMAT "/buffer%d.txt"

// Display
#define FIRST_COLUMN_NUMBER 1
#define FIRST_LINE_NUMBER 1
#define TAB_SIZE 4
#define WINDOW_HEIGHT 4
#define WINDOW_WIDTH 20

// Power
//
// The backlight is the only significant current draw this firmware controls.
// Everything else is fixed by the hardware choice: the USB keyboard is bus
// powered by us, the ESP32 cannot leave 240MHz because the soft USB host
// calibrates its NOP bit-timing against the boot clock, and it cannot light
// sleep because that host runs from a 1kHz timer ISR. See docs/power.md for
// the full census and what would actually reach the wiki's battery target.
//
// ESTIMATED, not measured on this board: the SerLCD RGB backlight is on the
// order of 20-30mA at level 64 on all three channels.
//
// Two stages rather than one, because a writing device is stared at while you
// think, not only while you type. Thirty seconds of stillness is a pause; five
// minutes is you having walked away.
#define BACKLIGHT_LEVEL 64             // 0-255 per channel, while writing
#define BACKLIGHT_DIM_LEVEL 12         // still readable indoors
#define BACKLIGHT_DIM_MILLISECONDS 30000
#define BACKLIGHT_OFF_MILLISECONDS 300000
// How long a transient notice ("buffer full", "sent") holds the screen.
#define STATUS_MESSAGE_MILLISECONDS 1200

// Send-buffer output
// Delay between simulated keystrokes when replaying a buffer to a host.
#define SEND_KEYSTROKE_DELAY_MILLISECONDS 8

// Special Characters
#define CHAR_EOL '\n'
#define CHAR_NUL '\0'
#define CHAR_SPC ' '
#define CHAR_TAB '\t'
#define WHITESPACE " \t\n"
// debug
#define CHAR_EOL_DISPLAY '='
#define CHAR_NUL_DISPLAY '.'
#define CHAR_SPC_DISPLAY '_'
#define CHAR_TAB_DISPLAY '~'

// USB
// USB KeyBoard - ESP32-Wroom
//#define PROFILE_NAME "Default Wroom"
//#define DEBUG_ALL

// Set to 1 to trace window/render internals over the serial port. This used to
// be unconditional, which printed the whole screen on every keystroke.
#define DEBUG_WINDOW_TRACE 0

#endif // CONFIG_H
