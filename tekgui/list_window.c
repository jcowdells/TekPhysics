#include "list_window.h"

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
    tekChainThrow(tekCreateText(text, window->text_size, tekGuiGetDefaultFont(), tek_text_ptr));
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

static exception tekGuiDrawListWindow(const TekGuiWindow* window) {
    const TekGuiListWindow* list_window = (TekGuiListWindow*)window->data;
    const ListItem* item;
    const float x = (float)(list_window->window.x_pos + (int)(list_window->text_size / 2));
    float y = (float)list_window->window.y_pos;
    foreach(item, list_window->text_list, {
        const char* text = item->data;
        TekText* tek_text = 0;
        tekChainThrow(tekGuiListWindowGetLookup(window, text, &tek_text));
        tekChainThrow(tekDrawColouredText(tek_text, x, y, (vec4){0.0f, 0.0f, 0.0f, 1.0f}));

        y += (float)list_window->text_size * 1.25f;
    });
    return SUCCESS;
}

exception tekGuiCreateListWindow(TekGuiListWindow* window, List* text_list) {
    struct TekGuiListWindowDefaults list_window_defaults = {};
    tekChainThrow(tekGuiGetListWindowDefaults(&list_window_defaults));

    tekChainThrow(tekGuiCreateWindow(&window->window));
    window->text_list = text_list;
    window->text_size = list_window_defaults.text_size;
    window->num_visible = list_window_defaults.text_size;
    glm_vec4_copy(list_window_defaults.text_colour, window->text_colour);
    window->window.data = window;
    window->window.draw_callback = tekGuiDrawListWindow;

    tekChainThrow(hashtableCreate(&window->text_lookup, 8));

    return SUCCESS;
}

void tekGuiDeleteListWindow(TekGuiListWindow* window) {
    tekGuiDeleteWindow(&window->window);
    tekGuiListWindowDeleteLookup(window);
}