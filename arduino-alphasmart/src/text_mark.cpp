#include "text_mark.h"
TextMarkHandler::TextMarkHandler()
{
    first_mark.prev_mark = nullptr;
    first_mark.next_mark = nullptr;
    first_mark.mark_name = TextMarkName::START_OF_BUFFER;
    first_mark.location = 0;
    first_mark.is_fixed = true;
}

struct TextMark *TextMarkHandler::get_mark_by_name(TextMarkName mark_name)
{
    struct TextMark *current_mark = &first_mark;
    while (current_mark->next_mark != nullptr && current_mark->mark_name != mark_name)
        current_mark = current_mark->next_mark;
    return current_mark;
}

struct TextMark *TextMarkHandler::get_mark_by_location(int location)
{
    struct TextMark *current_mark = &first_mark;
    struct TextMark *last_mark = current_mark;
    while (current_mark->next_mark != nullptr && current_mark->location <= location)
    {
        last_mark = current_mark;
        current_mark = current_mark->next_mark;
    }
    return last_mark;
}

void TextMarkHandler::create_mark(TextMarkName mark_name, int location, bool is_fixed)
{
    struct TextMark *new_mark = (struct TextMark *)malloc(sizeof(struct TextMark));
    struct TextMark *last_mark = get_mark_by_location(location);
    new_mark->mark_name = mark_name;
    new_mark->location = location;
    new_mark->is_fixed = is_fixed;
    new_mark->prev_mark = last_mark;
    new_mark->next_mark = last_mark->next_mark;
    last_mark->next_mark = new_mark;
}

void TextMarkHandler::delete_mark(TextMarkName mark_name)
{
    struct TextMark *current_mark = get_mark_by_name(mark_name);
    if (current_mark != nullptr)
    {
        current_mark->prev_mark->next_mark = current_mark->next_mark->prev_mark;
        current_mark->next_mark->prev_mark = current_mark->prev_mark->next_mark;
        free(current_mark);
    }
}
int TextMarkHandler::get_mark_location(TextMarkName mark_name)
{
    struct TextMark *current_mark = get_mark_by_name(mark_name);
    if (current_mark != nullptr)
        return current_mark->location;
    return -1;
}
bool TextMarkHandler::set_mark_location(TextMarkName mark_name, int location)
{
    struct TextMark *current_mark = get_mark_by_name(mark_name);
    if (current_mark != nullptr)
    {
        current_mark->location = location;
        return (current_mark->location == location);
    }
    return false;
}
int TextMarkHandler::distance_to_mark(TextMarkName mark_name, int location)
{
    return get_mark_location(mark_name) - location;
}