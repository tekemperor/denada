#include "text_store.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

TextStore::TextStore() {
    ready = false;
}

bool TextStore::begin() {
    // Format on first boot: a brand new device has no filesystem, and failing
    // to mount would otherwise mean silently never saving anything.
    ready = LittleFS.begin(true);
    if (!ready) {
        Serial.println("[store] LittleFS mount failed; buffers will not persist");
    }
    return ready;
}

bool TextStore::is_ready() {
    return ready;
}

static void storage_path(int index, char *path, int path_size) {
    snprintf(path, path_size, STORAGE_PATH_FORMAT, index);
}

int TextStore::load_all(TextDocument &document) {
    if (!ready) { return 0; }
    int restored = 0;
    char path[32];
    for (int i = 0; i < BUFFER_COUNT; i++) {
        storage_path(i, path, sizeof(path));
        if (!LittleFS.exists(path)) { continue; }
        File file = LittleFS.open(path, FILE_READ);
        if (!file) { continue; }
        TextBuffer &buffer = document.buffers[i];
        bool read_succeeded = false;
        if (file.size() == (size_t)TextBuffer::raw_size()) {
            size_t read = file.read(buffer.raw_storage_for_load(), TextBuffer::raw_size());
            read_succeeded = (read == (size_t)TextBuffer::raw_size());
        }
        file.close();
        if (buffer.finish_raw_load(read_succeeded)) {
            restored++;
        } else {
            Serial.printf("[store] %s rejected; buffer %d left empty\n", path, i);
        }
    }
    return restored;
}

bool TextStore::save_buffer(TextDocument &document, int index) {
    if (!ready) { return false; }
    if (index < 0 || index >= BUFFER_COUNT) { return false; }
    char path[32];
    storage_path(index, path, sizeof(path));
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[store] could not open %s for write\n", path);
        return false;
    }
    TextBuffer &buffer = document.buffers[index];
    size_t written = file.write(buffer.raw_bytes(), TextBuffer::raw_size());
    file.close();
    if (written != (size_t)TextBuffer::raw_size()) {
        Serial.printf("[store] short write to %s (%u of %d)\n", path,
                      (unsigned)written, TextBuffer::raw_size());
        return false;
    }
    buffer.is_modified = false;
    return true;
}

int TextStore::save_modified(TextDocument &document) {
    if (!ready) { return 0; }
    int written = 0;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (!document.buffers[i].is_modified) { continue; }
        if (save_buffer(document, i)) { written++; }
    }
    return written;
}

#else // !ARDUINO

// The host test build has no flash to persist to. The record format itself is
// exercised directly through TextBuffer::load_raw().
TextStore::TextStore() { ready = false; }
bool TextStore::begin() { return false; }
bool TextStore::is_ready() { return false; }
int TextStore::load_all(TextDocument &document) { return 0; }
bool TextStore::save_buffer(TextDocument &document, int index) { return false; }
int TextStore::save_modified(TextDocument &document) { return 0; }

#endif // ARDUINO
