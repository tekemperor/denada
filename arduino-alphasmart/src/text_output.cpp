#include "text_output.h"

int send_text(TextOutput &output, TextBuffer &buffer) {
    if (!output.begin()) { return 0; }
    int size = buffer.content_size();
    for (int i = 0; i < size; i++) {
        output.send_character(buffer.get_character(i));
    }
    output.end();
    return size;
}

#ifdef ARDUINO
#include <Arduino.h>

// A marker pair so a host-side receiver can tell the transferred document from
// the debug chatter that shares this UART.
#define SERIAL_SEND_HEADER "\n--- DENADA SEND BEGIN ---\n"
#define SERIAL_SEND_FOOTER "\n--- DENADA SEND END ---\n"

bool SerialTextOutput::begin() {
    Serial.print(SERIAL_SEND_HEADER);
    return true;
}

void SerialTextOutput::send_character(char character) {
    Serial.write(character);
}

void SerialTextOutput::end() {
    Serial.print(SERIAL_SEND_FOOTER);
    Serial.flush();
}

#endif // ARDUINO

#if DENADA_HAS_USB_DEVICE
#include <USB.h>
#include <USBHIDKeyboard.h>

static USBHIDKeyboard usb_keyboard;

bool UsbKeyboardTextOutput::begin() {
    usb_keyboard.begin();
    USB.begin();
    return true;
}

void UsbKeyboardTextOutput::send_character(char character) {
    // Paced deliberately: a host's HID stack drops characters if a "keyboard"
    // types faster than any human could.
    usb_keyboard.write(character);
    delay(SEND_KEYSTROKE_DELAY_MILLISECONDS);
}

void UsbKeyboardTextOutput::end() {
    usb_keyboard.releaseAll();
}

#endif // DENADA_HAS_USB_DEVICE
