#include "button.h"

#include <stdio.h>

#include "../tekgl/manager.h"
#include <GLFW/glfw3.h>
#include "../core/list.h"
#include <string.h>

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

static uint mouse_x = 0;
static uint mouse_y = 0;

static flag button_init = NOT_INITIALISED;

static List button_list = {0, 0};
static List button_dehover_list = {0, 0};

static uint tekGuiGetButtonIndex(const TekGuiButton* button) {
    const ListItem* item = 0;
    uint index = 0;
    foreach(item, (&button_list), {
        const TekGuiButton* check_ptr = (const TekGuiButton*)item->data;
        if (check_ptr == button) {
            break;
        }
        index++;
    });
    return index;
}

static int tekGuiCheckButtonHitbox(const TekGuiButton* button, uint check_x, uint check_y) {
    return (button->hitbox_x <= check_x) &&
        (button->hitbox_x + button->hitbox_width > check_x) &&
        (button->hitbox_y <= check_y) &&
        (button->hitbox_y + button->hitbox_height > check_y);
}

static void tekGuiButtonMouseButtonCallback(const int button, const int action, const int mods) {
    const ListItem* item = 0;
    TekGuiButton* clicked_button = 0;
    foreach(item, (&button_list), {
        TekGuiButton* loop_button = (TekGuiButton*)item->data;
        if (tekGuiCheckButtonHitbox(loop_button, mouse_x, mouse_y)) {
            clicked_button = loop_button;
            break;
        }
    });
    if (!clicked_button) return;

    if (clicked_button->callback) {
        TekGuiButtonCallbackData callback_data = {};
        memset(&callback_data, 0, sizeof(TekGuiButtonCallbackData));
        callback_data.type = TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK;
        callback_data.data.mouse_button.button = button;
        callback_data.data.mouse_button.action = action;
        callback_data.data.mouse_button.mods   = mods;
        callback_data.mouse_x = mouse_x;
        callback_data.mouse_y = mouse_y;
        clicked_button->callback(clicked_button, callback_data);
    }
}

static void tekGuiButtonMousePosCallback(double x, double y) {
    mouse_x = (uint)x;
    mouse_y = (uint)y;

    const ListItem* item = 0;
    TekGuiButtonCallbackData callback_data = {};
    memset(&callback_data, 0, sizeof(TekGuiButtonCallbackData));

    callback_data.type = TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK;
    callback_data.mouse_x = mouse_x;
    callback_data.mouse_y = mouse_y;

    uint index = 0;
    item = button_dehover_list.data;
    while (item) {
        TekGuiButton* button = (TekGuiButton*)item->data;
        item = item->next;
        if (tekGuiCheckButtonHitbox(button, mouse_x, mouse_y)) {
            button = NULL;
            index++;
            continue;
        }
        if (button->callback) button->callback(button, callback_data);
        listRemoveItem(&button_dehover_list, index, NULL);
    }

    flag top_button_callbacked = 0;

    foreach(item, (&button_list), {
        TekGuiButton* button = (TekGuiButton*)item->data;
        const int hitbox_check = tekGuiCheckButtonHitbox(button, mouse_x, mouse_y);

        if (!top_button_callbacked && hitbox_check) {
            callback_data.type = TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK;
            if (button->callback) button->callback(button, callback_data);
            top_button_callbacked = 1;
        }

        const ListItem* inner_item;
        foreach(inner_item, (&button_dehover_list), {
            const TekGuiButton* dehover_button = (const TekGuiButton*)inner_item->data;
            if (button == dehover_button) return;
        });

        if (hitbox_check) {
            callback_data.type = TEK_GUI_BUTTON_MOUSE_ENTER_CALLBACK;
            listAddItem(&button_dehover_list, button);
            if (button->callback) button->callback(button, callback_data);
            return;
        }
    });
}

static void tekGuiButtonDelete() {
    listDelete(&button_list);
}

tek_init tekGuiButtonInit() {
    printf("TekButton init\n");
    exception tek_exception = tekAddDeleteFunc(tekGuiButtonDelete);
    if (tek_exception) return;

    tek_exception = tekAddMouseButtonCallback(tekGuiButtonMouseButtonCallback);
    if (tek_exception) return;

    tek_exception = tekAddMousePosCallback(tekGuiButtonMousePosCallback);
    if (tek_exception) return;

    listCreate(&button_list);
    listCreate(&button_dehover_list);

    button_init = INITIALISED;
}

exception tekGuiCreateButton(TekGuiButton* button) {
    tekChainThrow(listAddItem(&button_list, button));
    return SUCCESS;
}

void tekGuiSetButtonPosition(TekGuiButton* button, uint x, uint y) {
    button->hitbox_x = x;
    button->hitbox_y = y;
}

void tekGuiSetButtonSize(TekGuiButton* button, uint width, uint height) {
    button->hitbox_width = width;
    button->hitbox_height = height;
}

exception tekGuiBringButtonToFront(const TekGuiButton* button) {
    const uint index = tekGuiGetButtonIndex(button);
    tekChainThrow(listMoveItem(&button_list, index, 0));
    return SUCCESS;
}

void tekGuiDeleteButton(const TekGuiButton* button) {
    const uint remove_index = tekGuiGetButtonIndex(button);
    listRemoveItem(&button_list, remove_index, NULL);
}
