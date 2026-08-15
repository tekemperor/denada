// Inspiration: AlphaSmart Neo
/*
 * Depends on:
 *    ESP32-USBSoftHost.hpp
 *    SerLCD.h
 *    Wire.h
 *
 * This sketch is the hardware layer and nothing else. Everything that can be
 * reasoned about without a soldering iron -- the gap buffer, word wrap, the
 * cursor, selection, the key map -- lives in src/ and is covered by the host
 * test suite in ../test. What is left here is the part that genuinely needs the
 * board: USB, I2C, flash, and the clock.
 */
#include "src/config.h"
#include "src/editor.h"
#include "src/keyboard_buffer_usb.h"
#include "src/keyboard_usb.h"
#include "src/lcd.h"
#include "src/text_output.h"
#include "src/text_store.h"
#include "src/usb_debug.h"
#include <ESP32-USBSoftHost.hpp> // USH (also starts FreeRTOS)
#include <Wire.h>                // Wire
#include <SerLCD.h>              // SerLCD

// Held keys repeat, because holding backspace to fix a word is not optional in
// a writing device. The HID layer reports "still down", not "pressed again", so
// the repeat has to be generated here.
#define KEY_REPEAT_DELAY_MILLISECONDS 400
#define KEY_REPEAT_INTERVAL_MILLISECONDS 45
// Roomier than the 2048 the old input task used: this one also renders, writes
// to LittleFS, and streams sends.
#define EDITOR_TASK_STACK_BYTES 8192
#define SPLASH_MILLISECONDS 900

SerLCD lcd;
Editor editor;
TextStore store;
KeyboardInputHandler keyboard_handler;
KeyboardInputBuffer keyboard_buffer;
TaskHandle_t editor_task_handle;

static unsigned long last_edit_at = 0;
static bool save_pending = false;
static unsigned int last_status_sequence = 0;
static unsigned long status_shown_at = 0;
static uint8_t repeat_key = KEY_NONE;
static unsigned long repeat_started_at = 0;
static unsigned long repeat_last_at = 0;

static void redraw()
{
    display_editor(&lcd, &editor);
}

// Any modified buffer restarts the idle countdown, so a burst of typing costs
// one flash write rather than one per keystroke.
static void note_possible_edit()
{
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        if (editor.document.buffers[i].is_modified)
        {
            last_edit_at = millis();
            save_pending = true;
            return;
        }
    }
}

static void autosave_if_idle()
{
    if (!save_pending) { return; }
    if (millis() - last_edit_at < AUTOSAVE_IDLE_MILLISECONDS) { return; }

    // Writing to flash disables the instruction cache, and the USB soft host's
    // timer ISR fires often enough that it will be caught mid-write. The
    // library provides TimerPause/TimerResume for exactly this; without them
    // the board panics with "Cache disabled but cached memory region accessed".
    // A save only runs after typing has been idle, so the brief deafness to the
    // keyboard costs nothing.
    USH.TimerPause();
    int written = store.save_modified(editor.document);
    USH.TimerResume();

    save_pending = false;
    if (written > 0) { Serial.printf("[store] saved %d buffer(s)\n", written); }
}

// True when the status line changed state and the screen needs repainting.
static bool update_status_timer()
{
    if (editor.status_sequence != last_status_sequence)
    {
        last_status_sequence = editor.status_sequence;
        status_shown_at = millis();
        return true;
    }
    if (editor.has_status() && millis() - status_shown_at >= STATUS_MESSAGE_MILLISECONDS)
    {
        editor.clear_status();
        return true;
    }
    return false;
}

// Commands that would be wrong to auto-repeat: nobody wants a held F3 to switch
// buffers forty times a second, or a held Ctrl+V to paste until the buffer
// fills.
static bool is_repeatable(EditorCommand command)
{
    switch (command)
    {
    case EditorCommand::SWITCH_BUFFER:
    case EditorCommand::SEND_BUFFER:
    case EditorCommand::COPY:
    case EditorCommand::CUT:
    case EditorCommand::PASTE:
    case EditorCommand::SELECT_ALL:
    case EditorCommand::NONE:
        return false;
    default:
        return true;
    }
}

static bool apply_key_repeat()
{
    uint8_t key = keyboard_handler.held_key();
    if (key == KEY_NONE)
    {
        repeat_key = KEY_NONE;
        return false;
    }
    unsigned long now = millis();
    if (key != repeat_key)
    {
        repeat_key = key;
        repeat_started_at = now;
        repeat_last_at = now;
        return false;
    }
    if (now - repeat_started_at < KEY_REPEAT_DELAY_MILLISECONDS) { return false; }
    if (now - repeat_last_at < KEY_REPEAT_INTERVAL_MILLISECONDS) { return false; }
    repeat_last_at = now;

    EditorAction action = keyboard_handler.translate(keyboard_handler.held_modifiers(), key);
    if (!is_repeatable(action.command)) { return false; }
    editor.apply(action);
    return true;
}

static void handle_send_request()
{
    editor.send_requested = false;
    // On this board the UART is the only transport that physically exists; see
    // the hardware note in text_output.h.
    SerialTextOutput output;
    int sent = send_text(output, *editor.buffer());
    char message[STATUS_MESSAGE_SIZE];
    snprintf(message, sizeof(message), "SENT %d CHARS", sent);
    editor.set_status(message);
}

static bool drain_keyboard()
{
    bool changed = false;
    EditorAction actions[KEYBOARD_MAX_ACTIONS];
    while (!keyboard_buffer.is_empty())
    {
        // Held in a local rather than used straight off the return value: the
        // report has to outlive the parse call.
        struct keyboard_input_report report = keyboard_buffer.read();
        int count = keyboard_handler.parse(report.data, actions, KEYBOARD_MAX_ACTIONS);
        for (int i = 0; i < count; i++)
        {
            editor.apply(actions[i]);
            changed = true;
        }
    }
    return changed;
}

static void editor_task()
{
    for (;;)
    {
        vTaskDelay(1);
        bool changed = drain_keyboard();
        if (apply_key_repeat()) { changed = true; }
        if (editor.send_requested)
        {
            handle_send_request();
            changed = true;
        }
        if (changed) { note_possible_edit(); }
        if (update_status_timer()) { changed = true; }
        if (changed) { redraw(); }
        autosave_if_idle();
    }
}

static void usb_status(uint8_t usbNum, void *dev)
{
#ifdef USB_DEBUG_H
    usb_status_display(usbNum, dev);
#endif
}

static void usb_input(uint8_t usbNum, uint8_t byte_depth, uint8_t *data, uint8_t data_len)
{
    keyboard_buffer.write(data);
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);
    lcd.begin(Wire); // Setup LCD with I2C
    display_config(&lcd);

    // Storage is brought up BEFORE the USB soft host, and the order is load
    // bearing. A blank device has no filesystem, so the first boot formats one,
    // which erases the whole 1.4MB partition. With the soft host's timer
    // already running that erase starves the interrupt watchdog: the board
    // resets with TG1WDT_SYS_RESET, boots, formats again, and loops forever
    // without ever reaching the editor. Doing the flash work while nothing else
    // is running costs nothing and makes first boot survivable.
    lcd.setCursor(0, 2);
    lcd.print("   preparing...");
    if (store.begin())
    {
        int restored = store.load_all(editor.document);
        Serial.printf("[store] restored %d buffer(s) of %d\n", restored, BUFFER_COUNT);
    }
    else
    {
        lcd.setCursor(0, 2);
        lcd.print("  no save storage  ");
    }
    editor.window.attach(editor.document.active());
    editor.window.declare_column_desired();

    usb_pins_config_t usb_config = {PIN_USB_DATA_PLUS, PIN_USB_DATA_MINUS, -1, -1, -1, -1, -1, -1};
    USH.init(usb_config, usb_status, usb_input);

    Serial.printf("[denada] %d buffers of %d bytes, %u bytes heap free\n",
                  BUFFER_COUNT, GAP_BUFFER_DATA_SIZE, (unsigned)ESP.getFreeHeap());

    delay(SPLASH_MILLISECONDS);
    lcd.clear();
    redraw();
    xTaskCreate((TaskFunction_t)editor_task, "editor_task", EDITOR_TASK_STACK_BYTES,
                NULL, 5, &editor_task_handle);
}

void loop()
{
    vTaskDelete(NULL);
}
