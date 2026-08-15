#ifndef TEXT_STORE_H
#define TEXT_STORE_H
#include "text_document.h"

// Buffer persistence: text survives power loss.
//
// This is the behaviour that separates an AlphaSmart from a terminal. You close
// it mid-sentence, you open it a week later, the sentence is still there. It is
// also the last un-struck caveat in the project README.
//
// Buffers are written to LittleFS on the ESP32's own flash, one file per
// buffer, as the GapBufferRaw record verbatim. Writes happen after typing goes
// idle rather than on every keystroke: flash has a finite erase budget, and a
// write per character would burn it while making typing stutter.
class TextStore {
public:
    TextStore();

    bool begin();
    bool is_ready();

    // Returns how many buffers were restored. A file that fails validation is
    // skipped and its buffer left empty rather than shown as garbage.
    int load_all(TextDocument&);
    bool save_buffer(TextDocument&, int);
    // Writes only the buffers marked modified. Returns how many were written.
    int save_modified(TextDocument&);

private:
    bool ready;
};

#endif // TEXT_STORE_H
