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

/**
 * Get the index of a button in the button list by pointer.
 * @param button The button to get the index of.
 * @return The index of the button (possibly 0 if not in the list)
 */
static uint tekGuiGetButtonIndex(const TekGuiButton* button) {
    const ListItem* item = 0;
    uint index = 0;
    // linear search.
    // not much else to say
    foreach(item, (&button_list), {
        const TekGuiButton* check_ptr = (const TekGuiButton*)item->data;
        if (check_ptr == button) {
            break;
        }
        index++;
    });
    return index;
}

/**
 * Check if a position is within a button.
 * @param button The button to check against.
 * @param check_x The x coordinate to check.
 * @param check_y The y coordinate to check.
 * @return 1 if the button contains the coordinate, 0 otherwise.
 */
static int tekGuiCheckButtonHitbox(const TekGuiButton* button, int check_x, int check_y) {
    // very basic maths
    return (button->hitbox_x <= check_x) &&
        (button->hitbox_x + button->hitbox_width > check_x) &&
        (button->hitbox_y <= check_y) &&
        (button->hitbox_y + button->hitbox_height > check_y);
}

/**
 * Mouse button callback for button code. If click occurs within a button, then send callback to the button and only that button.
 * @param button The button being pressed.
 * @param action The action e.g. press or release.
 * @param mods Any modifiers acting on the button.
 */
static void tekGuiButtonMouseButtonCallback(const int button, const int action, const int mods) {
    const ListItem* item = 0;

    // check every button until finding one that is under the mouse.
    // buttons in order of priority/height
    TekGuiButton* clicked_button = 0;
    foreach(item, (&button_list), {
        TekGuiButton* loop_button = (TekGuiButton*)item->data;
        if (tekGuiCheckButtonHitbox(loop_button, mouse_x, mouse_y)) {
            clicked_button = loop_button;
            break;
        }
    });
    if (!clicked_button) return; // no button found = not clicking any button

    // callback if exists
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

/**
 * Mouse movement callback for button code. If mouse moves within region of a button, the topmost button will bbe called back.
 * @param x The x position of the mouse.
 * @param y The y position of the mouse.
 */
static void tekGuiButtonMousePosCallback(double x, double y) {
    // convert to integers
    mouse_x = (uint)x;
    mouse_y = (uint)y;

    // create callback data struct and set data
    const ListItem* item = 0;
    TekGuiButtonCallbackData callback_data = {};
    memset(&callback_data, 0, sizeof(TekGuiButtonCallbackData));

    callback_data.type = TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK;
    callback_data.mouse_x = mouse_x;
    callback_data.mouse_y = mouse_y;

    // check if we have just left a button we were in previously.
    // if so, send a dehover event
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

    // only top button should receive callback, so track if its been done yet
    flag top_button_callbacked = 0;

    // now checking for entering 
    foreach(item, (&button_list), {
        TekGuiButton* button = (TekGuiButton*)item->data;
        const int hitbox_check = tekGuiCheckButtonHitbox(button, mouse_x, mouse_y);

        // callback if touching
        if (!top_button_callbacked && hitbox_check) {
            callback_data.type = TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK;
            if (button->callback) button->callback(button, callback_data);
            top_button_callbacked = 1;
        }

        // remove button from dehover list
        const ListItem* inner_item;
        foreach(inner_item, (&button_dehover_list), {
            const TekGuiButton* dehover_button = (const TekGuiButton*)inner_item->data;
            if (button == dehover_button) return;
        });

        // button entering check
        if (hitbox_check) {
            callback_data.type = TEK_GUI_BUTTON_MOUSE_ENTER_CALLBACK;
            listAddItem(&button_dehover_list, button);
            if (button->callback) button->callback(button, callback_data);
            return;
        }
    });
}

/**
 * Mouse scrolling callback for button code. The topmost button will be called back if the scroll occurs in their bounds.
 * @param x_offset The scroll amount in x-direction
 * @param y_offset The scroll amount in y-direction
 */
static void tekGuiButtonMouseScrollCallback(double x_offset, double y_offset) {
    // create new callback data struct
    const ListItem* item = 0;
    TekGuiButtonCallbackData callback_data = {};
    memset(&callback_data, 0, sizeof(TekGuiButtonCallbackData));

    // write callback data
    callback_data.type = TEK_GUI_BUTTON_MOUSE_SCROLL_CALLBACK;
    callback_data.mouse_x = mouse_x;
    callback_data.mouse_y = mouse_y;
    callback_data.data.mouse_scroll.x_offset = x_offset;
    callback_data.data.mouse_scroll.y_offset = y_offset;

    // loop through buttons and find any which are being touched by mouse right now
    foreach(item, (&button_list), {
        TekGuiButton* button = (TekGuiButton*)item->data;
        if (tekGuiCheckButtonHitbox(button, mouse_x, mouse_y)) {
            if (button->callback) button->callback(button, callback_data);
            return;
        }
    });
}

/**
 * Delete callback for button code. Delete supporting data structures.
 */
static void tekGuiButtonDelete() {
    // delete the lists.
    listDelete(&button_list);
    listDelete(&button_dehover_list);
}

/**
 * Initialisation function for gui button code. Set up callbacks
 */
tek_init tekGuiButtonInit() {
    // delete callback
    exception tek_exception = tekAddDeleteFunc(tekGuiButtonDelete);
    if (tek_exception) return;

    // mouse input callback
    tek_exception = tekAddMouseButtonCallback(tekGuiButtonMouseButtonCallback);
    if (tek_exception) return;

    tek_exception = tekAddMousePosCallback(tekGuiButtonMousePosCallback);
    if (tek_exception) return;

    tek_exception = tekAddMouseScrollCallback(tekGuiButtonMouseScrollCallback);
    if (tek_exception) return;

    // create lists and variables
    listCreate(&button_list);
    listCreate(&button_dehover_list);

    button_init = INITIALISED;
}

/**
 * Create a new button. A button is not rendered to the screen, it is just an area that responds to mouse activity.
 * @param button A pointer to the button. The pointer will be checked at each mouse event.
 * @throws LIST_EXCEPTION if could not add to the list of buttons
 */
exception tekGuiCreateButton(TekGuiButton* button) {
    tekChainThrow(listAddItem(&button_list, button)); // what it says on the tin
    return SUCCESS;
}

/**
 * Set the position on the window of a button in pixels.
 * @param button The button to set the position of.
 * @param x The new x-coordinate of the button.
 * @param y The new y-coordinate of the button.
 */
void tekGuiSetButtonPosition(TekGuiButton* button, int x, int y) {
    // update button position.
    button->hitbox_x = x;
    button->hitbox_y = y;
}

/**
 * Set the size of a button in pixels.
 * @param button The button to set the size of.
 * @param width The new width of the button.
 * @param height The new height of the button.
 */
void tekGuiSetButtonSize(TekGuiButton* button, uint width, uint height) {
    // update it.
    button->hitbox_width = width;
    button->hitbox_height = height;
}

/**
 * Bring a button to the front of the screen, so it is rendered above other buttons
 * @param button The button to bring to the front.
 * @throws LIST_EXCEPTION if could not move button to front of rendering order.
 */
exception tekGuiBringButtonToFront(const TekGuiButton* button) {
    // get index and move to 0
    const uint index = tekGuiGetButtonIndex(button);
    tekChainThrow(listMoveItem(&button_list, index, 0));
    return SUCCESS;
}

/**
 * Delete a button, freeing any allocated memory
 * @param button The button to delete
 */
void tekGuiDeleteButton(const TekGuiButton* button) {
    // complex simplicity (volume 1)
    const uint remove_index = tekGuiGetButtonIndex(button);
    listRemoveItem(&button_list, remove_index, NULL);
}
