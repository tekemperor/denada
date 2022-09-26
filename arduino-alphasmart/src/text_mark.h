#ifndef TEXT_MARK_H
#define TEXT_MARK_H

#include <cstdlib>

enum TextMarkName
{
    START_OF_BUFFER
};

struct TextMark
{
    struct TextMark *prev_mark;
    struct TextMark *next_mark;
    enum TextMarkName mark_name;
    int location;
    bool is_fixed;
};

class TextMarkHandler
{
private:
    static struct TextMark first_mark;

public:
    TextMarkHandler();
    struct TextMark *get_mark_by_name(TextMarkName mark_name);
    struct TextMark *get_mark_by_location(int location);
    void create_mark(TextMarkName mark_name, int location, bool is_fixed);
    void delete_mark(TextMarkName mark_name);
    int get_mark_location(TextMarkName mark_name);
    bool set_mark_location(TextMarkName mark_name, int location);
    int distance_to_mark(TextMarkName mark_name, int location);
};

#endif // TEXT_MARK_H
