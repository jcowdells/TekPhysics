#include "button.h"

#include "../tekgl/manager.h"
#include <GLFW/glfw3.h>
#include "../core/list.h"

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

static uint mouse_x = 0;
static uint mouse_y = 0;

static flag button_init = NOT_INITIALISED;

static List button_list = {0, 0};

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

static void tekGuiButtonMouseButtonCallback(int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    const ListItem* item = 0;
    const TekGuiButton* clicked_button = 0;
    foreach(item, (&button_list), {
        const TekGuiButton* button = (const TekGuiButton*)item->data;
        if (tekGuiCheckButtonHitbox(button, mouse_x, mouse_y)) {
            clicked_button = button;
            break;
        }
    });
    if (!clicked_button) return;

    printf("Clicked button @ %u %u (=%p)\n", mouse_x, mouse_y, clicked_button);
}

static void tekGuiButtonMousePosCallback(double x, double y) {
    mouse_x = (uint)x;
    mouse_y = (uint)y;
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

    button_init = INITIALISED;
}

exception tekGuiCreateButton(TekGuiButton* button, uint x, uint y, uint width, uint height) {
    button->hitbox_x = x;
    button->hitbox_y = y;
    button->hitbox_width = width;
    button->hitbox_height = height;

    tekChainThrow(listAddItem(&button_list, button));

    return SUCCESS;
}

exception tekGuiDeleteButton(const TekGuiButton* button) {
    const uint remove_index = tekGuiGetButtonIndex(button);
    tekChainThrow(listRemoveItem(&button_list, remove_index, NULL));

    return SUCCESS;
}
