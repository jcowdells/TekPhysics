#include "tekgui.h"

#include <stdio.h>

#include "window.h"
#include "../tekgl/manager.h"
#include "../core/vector.h"
#include "../core/yml.h"

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

static flag tek_gui_init = NOT_INITIALISED;
static Vector window_buffer;
static Vector window_data_buffer;
static YmlFile options_yml;

void tekGuiDelete() {
    for (uint i = 0; i < window_buffer.length; i++) {
        TekGuiWindow* window;
        vectorGetItemPtr(&window_buffer, i, &window);
        tekGuiDeleteWindow(window);
    }
    vectorDelete(&window_buffer);
}

tek_init tekGuiInit() {
    exception tek_exception = vectorCreate(1, sizeof(TekGuiWindow), &window_buffer);
    if (tek_exception) return;

    tek_exception = vectorCreate(1, sizeof(TekGuiWindowData), &window_data_buffer);
    if (tek_exception) return;

    tek_exception = ymlReadFile("../tekgui/options.yml", &options_yml);
    if (tek_exception) return;

    tek_exception = tekAddDeleteFunc(tekGuiDelete);
    if (tek_exception) return;

    tek_gui_init = INITIALISED;
}

exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "window_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing window defaults section in 'options.yml'.");
        defaults->x_pos = 0;
        defaults->y_pos = 0;
        defaults->width = 0;
        defaults->height = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "x_pos"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->x_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "y_pos"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->y_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "width"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "height"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->height = (uint)yml_integer;

    double yml_float;
    vec4 yml_colour;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "colour", "r"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[0] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "colour", "g"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[1] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "colour", "b"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[2] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "colour", "a"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[3] = (float)yml_float;

    glm_vec4_copy(yml_colour, defaults->colour);
    return SUCCESS;
}