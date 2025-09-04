#include "tekgui.h"
#include "window.h"

#include <string.h>
#include <cglm/vec2.h>

exception tekGuiCreateWindow(TekGuiWindow* window) {
    struct TekGuiWindowDefaults defaults = {};
    tekChainThrow(tekGuiGetWindowDefaults(&defaults));

    window->type = WINDOW_TYPE_EMPTY;
    window->data = NULL;
    window->x_pos = defaults.x_pos;
    window->y_pos = defaults.y_pos;
    window->width = defaults.width;
    window->height = defaults.height;
    glm_vec4_copy(defaults.colour, window->background_colour);
}

void tekGuiDeleteWindow(TekGuiWindow* window) {

}

void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data) {
    glm_vec2_copy((vec2){(float)window->x_pos, (float)window->y_pos}, window_data->position);
    glm_vec2_copy((vec2){(float)window->width, (float)window->height}, window_data->size);
    memcpy(window_data->colour, window->background_colour, sizeof(vec4));
}