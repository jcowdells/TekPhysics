#include "list_window.h"

#include <GLFW/glfw3.h>

#include "tekgui.h"

static exception tekGuiListWindowRemoveLookup(TekGuiListWindow* window, const char* key) {
    TekText* tek_text;
    tekChainThrow(hashtableGet(&window->text_lookup, key, &tek_text));
    if (tek_text) {
        tekDeleteText(tek_text);
        free(tek_text);
    }
    tekChainThrow(hashtableRemove(&window->text_lookup, key));
    return SUCCESS;
}

static exception tekGuiListCleanLookup(TekGuiListWindow* window) {
    char** keys;
    tekChainThrow(hashtableGetKeys(&window->text_lookup, &keys));

    // if there is a key in the lookup that is no longer in the list, we should remove it as it will not be rendered.
    for (uint i = 0; i < window->text_lookup.num_items; i++) {
        const char* key = keys[i];
        const ListItem* item;
        flag exists = 0;
        foreach(item, window->text_list, {
            if (!strcmp(key, item->data)) {
                exists = 1;
                break;
            }
        });
        if (!exists) tekChainThrow(tekGuiListWindowRemoveLookup(window, key));
    }

    free(keys);
    return SUCCESS;
}

static exception tekGuiListWindowAddLookup(TekGuiListWindow* window, const char* text, TekText** tek_text) {
    TekText* tek_text_ptr = (TekText*)malloc(sizeof(TekText));
    if (!tek_text_ptr)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text mesh.");
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText(text, window->text_size, font, tek_text_ptr));
    tekChainThrow(hashtableSet(&window->text_lookup, text, tek_text_ptr));
    if (tek_text)
        *tek_text = tek_text_ptr;
    return SUCCESS;
}

static exception tekGuiListWindowGetLookup(TekGuiListWindow* window, const char* text, TekText** tek_text) {
    const flag has_text = hashtableHasKey(&window->text_lookup, text);
    if (has_text) {
        tekChainThrow(hashtableGet(&window->text_lookup, text, tek_text));
    } else {
        tekChainThrow(tekGuiListWindowAddLookup(window, text, tek_text));
        tekChainThrow(tekGuiListCleanLookup(window));
    }
    return SUCCESS;
}

static exception tekGuiListWindowDeleteLookup(TekGuiListWindow* window) {
    char** keys;
    tekChainThrow(hashtableGetKeys(&window->text_lookup, &keys));

    for (uint i = 0; i < window->text_lookup.num_items; i++) {
        const char* key = keys[i];
        tekChainThrow(tekGuiListWindowRemoveLookup(window, key));
    }

    free(keys);
    return SUCCESS;
}

static float tekGuiGetColourBrightness(const vec4 colour) {
    return colour[0] * 0.299f + colour[1] * 0.587f + colour[2] * 0.114f;
}

static void tekGuiModifyColourBrightness(vec4 original_colour, vec4 final_colour, const float delta) {
    const float brightness = tekGuiGetColourBrightness(original_colour);
    if (brightness < 0.5f) {
        glm_vec4_copy((vec4){
                original_colour[0] + delta,
                original_colour[1] + delta,
                original_colour[2] + delta,
                original_colour[3],
            },
            final_colour
        );
    } else {
        glm_vec4_copy((vec4){
                original_colour[0] - delta,
                original_colour[1] - delta,
                original_colour[2] - delta,
                original_colour[3],
            },
            final_colour
        );
    }
}

static exception tekGuiDrawListWindow(TekGuiWindow* window) {
    TekGuiListWindow* list_window = (TekGuiListWindow*)window->data;
    tekGuiSetButtonPosition(&list_window->button, list_window->window.x_pos, list_window->window.y_pos);
    tekGuiSetButtonSize(&list_window->button, list_window->window.width, list_window->window.height);

    const ListItem* item;
    const float x = (float)(list_window->window.x_pos + (int)(list_window->text_size / 2));
    float y = (float)list_window->window.y_pos;
    uint index = 0;
    foreach(item, list_window->text_list, {

        if (index < list_window->draw_index) {
            index++;
            item = item->next;
            continue;
        }
        if (index >= list_window->draw_index + list_window->num_visible) break;

        const char* text = item->data;
        TekText* tek_text = 0;
        tekChainThrow(tekGuiListWindowGetLookup(window, text, &tek_text));

        vec4 text_colour;
        if (index == list_window->select_index) {
            tekGuiModifyColourBrightness(list_window->text_colour, text_colour, 0.4f);
        } else if (index == list_window->hover_index) {
            tekGuiModifyColourBrightness(list_window->text_colour, text_colour, 0.2f);
        } else {
            glm_vec4_copy(list_window->text_colour, text_colour);
        }

        tekChainThrow(tekDrawColouredText(tek_text, x, y, text_colour));

        y += (float)list_window->text_size * 1.25f;
        index++;
    });
    return SUCCESS;
}

static exception tekGuiSelectListWindow(TekGuiWindow* window) {
    TekGuiListWindow* list_window = (TekGuiListWindow*)window->data;
    tekChainThrow(tekGuiBringButtonToFront(&list_window->button));
    return SUCCESS;
}

static int tekGuiListWindowGetIndex(TekGuiListWindow* window, int mouse_y) {
    const int
        min_y = window->window.y_pos,
        max_y = min_y + window->window.height;

    const float depth = (float)(mouse_y - min_y) / (float)(max_y - min_y);
    const int screen_index = (int)(depth * (float)window->num_visible);

    // if the index would suggest a selection off the screen, disallow it.
    if (screen_index < 0 || screen_index >= window->num_visible)
        return -1;

    return screen_index + window->draw_index;
}

static void tekGuiListWindowButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    TekGuiListWindow* window = (TekGuiListWindow*)button->data;
    if (!window->window.visible)
        return;
    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK:
        window->hover_index = tekGuiListWindowGetIndex(window, (int)callback_data.mouse_y);
        break;
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT || callback_data.data.mouse_button.action != GLFW_RELEASE)
            break;
        window->select_index = tekGuiListWindowGetIndex(window, (int)callback_data.mouse_y);
        if (window->callback)
            window->callback(window);
        break;
    case TEK_GUI_BUTTON_MOUSE_SCROLL_CALLBACK:
        if (callback_data.data.mouse_scroll.y_offset > 0.0) {
            if (window->draw_index > 0)
                window->draw_index--;
        } else if (callback_data.data.mouse_scroll.y_offset < 0.0) {
            if (window->draw_index < (int)window->text_list->length - (int)window->num_visible)
                window->draw_index++;
        }
        window->hover_index = tekGuiListWindowGetIndex(window, (int)callback_data.mouse_y);
        break;
    default:
        break;
    }
}

exception tekGuiCreateListWindow(TekGuiListWindow* window, List* text_list) {
    struct TekGuiListWindowDefaults list_window_defaults = {};
    tekChainThrow(tekGuiGetListWindowDefaults(&list_window_defaults));

    tekChainThrow(tekGuiCreateWindow(&window->window));
    window->text_list = text_list;
    window->text_size = list_window_defaults.text_size;
    window->num_visible = list_window_defaults.num_visible;
    glm_vec4_copy(list_window_defaults.text_colour, window->text_colour);
    window->window.data = window;
    window->window.draw_callback = tekGuiDrawListWindow;
    window->window.select_callback = tekGuiSelectListWindow;

    printf("num: %u, size: %u, final: %u\n", window->num_visible, window->text_size, window->num_visible * window->text_size * 5 / 4);
    tekGuiSetWindowSize(&window->window, window->window.width, window->num_visible * window->text_size * 5 / 4);

    tekChainThrow(hashtableCreate(&window->text_lookup, 8));
    tekChainThrow(tekGuiCreateButton(&window->button));
    tekGuiSetButtonPosition(&window->button, window->window.x_pos, window->window.y_pos);
    tekGuiSetButtonSize(&window->button, window->window.width, window->window.height);
    window->button.callback = tekGuiListWindowButtonCallback;
    window->button.data = window;

    return SUCCESS;
}

void tekGuiDeleteListWindow(TekGuiListWindow* window) {
    tekGuiDeleteWindow(&window->window);
    tekGuiListWindowDeleteLookup(window);
}
