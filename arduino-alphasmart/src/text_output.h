#ifndef TEXT_OUTPUT_H
#define TEXT_OUTPUT_H
#include "text_buffer.h"

// Sending a buffer to a host computer.
//
// HARDWARE NOTE, and it is the important one in this file: the classic
// ESP32-WROOM cannot act as a USB keyboard. It has no USB device peripheral at
// all. The USB socket on the dev board is a CP2102 UART bridge, and
// ESP32-USBSoftHost bit-bangs *host* mode on two GPIOs, which is the opposite
// direction. Presenting as a keyboard needs native USB OTG, which arrived with
// the ESP32-S2 and S3.
//
// So the transport is abstracted rather than assumed. SerialTextOutput works on
// the board that exists today and is what the send key is wired to;
// UsbKeyboardTextOutput is the real thing and compiles only for a chip that can
// support it. Which one you get is a property of the silicon, not a setting.
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define DENADA_HAS_USB_DEVICE 1
#else
#define DENADA_HAS_USB_DEVICE 0
#endif

class TextOutput {
public:
    virtual ~TextOutput() {}
    // False aborts the send; the caller reports it rather than half-sending.
    virtual bool begin() { return true; }
    virtual void send_character(char) = 0;
    virtual void end() {}
    virtual const char* name() { return "output"; }
};

// Walks a buffer through a transport. Pure logic, so the send path is testable
// without any of the transports being present.
int send_text(TextOutput&, TextBuffer&);

#ifdef ARDUINO

// Streams the buffer out the UART. On the current hardware this is the only
// direction that physically works, and it is what `denada-receive` on the host
// side reads.
class SerialTextOutput : public TextOutput {
public:
    bool begin() override;
    void send_character(char) override;
    void end() override;
    const char* name() override { return "serial"; }
};

#endif // ARDUINO

#if DENADA_HAS_USB_DEVICE

// Types the buffer into whatever has focus on the host, as a real keyboard.
// Unverified: no ESP32-S2/S3 was connected when this was written.
class UsbKeyboardTextOutput : public TextOutput {
public:
    bool begin() override;
    void send_character(char) override;
    void end() override;
    const char* name() override { return "usb-keyboard"; }
};

#endif // DENADA_HAS_USB_DEVICE

#endif // TEXT_OUTPUT_H
