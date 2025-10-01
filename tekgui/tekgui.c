#include "tekgui.h"

#include <stdio.h>
#include <string.h>

#include "../tekgl/manager.h"
#include "../core/yml.h"

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

static flag tek_gui_init = NOT_INITIALISED;
static flag tek_gui_gl_init = NOT_INITIALISED;
static YmlFile options_yml;

static struct TekGuiWindowDefaults window_defaults;
static struct TekGuiListWindowDefaults list_window_defaults;
static struct TekGuiTextButtonDefaults text_button_defaults;
static struct TekGuiTextInputDefaults text_input_defaults;
static TekBitmapFont default_font;

static exception tekGuiGetOptionsColour(const char* option_name, const char* colour_name, vec4 colour) {
    YmlData* yml_data;
    double yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, option_name, colour_name, "r"));
    tekChainThrow(ymlDataToFloat(yml_data, &yml_float));
    colour[0] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, option_name, colour_name, "g"));
    tekChainThrow(ymlDataToFloat(yml_data, &yml_float));
    colour[1] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, option_name, colour_name, "b"));
    tekChainThrow(ymlDataToFloat(yml_data, &yml_float));
    colour[2] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, option_name, colour_name, "a"));
    tekChainThrow(ymlDataToFloat(yml_data, &yml_float));
    colour[3] = (float)yml_float;

    return SUCCESS;
}

static exception tekGuiLoadWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "window_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing window defaults section in 'options.yml'.");
        defaults->x_pos = 0;
        defaults->y_pos = 0;
        defaults->width = 0;
        defaults->height = 0;
        defaults->title_width = 0;
        defaults->border_width = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->background_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->border_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->title_colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->height = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "title_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->title_width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "title"));
    tekChainThrow(ymlDataToString(yml_data, &defaults->title));

    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "border_colour", defaults->border_colour));
    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "title_colour", defaults->title_colour));

    return SUCCESS;
}

static exception tekGuiLoadListWindowDefaults(struct TekGuiListWindowDefaults* defaults) {
    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "list_window_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing list window defaults section in 'options.yml'.");
        defaults->text_size = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->text_colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "list_window_defaults", "text_size"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_size = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "list_window_defaults", "num_visible"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->num_visible = (uint)yml_integer;

    tekChainThrow(tekGuiGetOptionsColour("list_window_defaults", "text_colour", defaults->text_colour));
}

static exception tekGuiLoadTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults) {
    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "text_button_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing text button defaults section in 'options.yml'.");
        defaults->x_pos = 0;
        defaults->y_pos = 0;
        defaults->width = 0;
        defaults->height = 0;
        defaults->border_width = 0;
        defaults->text_height = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->background_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->selected_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->border_colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->height = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "text_height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_height = (uint)yml_integer;

    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "selected_colour", defaults->selected_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "border_colour", defaults->border_colour));

    return SUCCESS;
}

static exception tekGuiLoadTextInputDefaults(struct TekGuiTextInputDefaults* defaults) {
    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "text_input_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing text input defaults section in 'options.yml'.");
        defaults->x_pos = 0;
        defaults->y_pos = 0;
        defaults->width = 0;
        defaults->text_height = 0;
        defaults->border_width = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->background_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->border_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->text_colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "text_height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_height = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "border_colour", defaults->border_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "text_colour", defaults->text_colour));

    return SUCCESS;
}

exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    memcpy(defaults, &window_defaults, sizeof(struct TekGuiWindowDefaults));
    return SUCCESS;
}

exception tekGuiGetListWindowDefaults(struct TekGuiListWindowDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    memcpy(defaults, &list_window_defaults, sizeof(struct TekGuiListWindowDefaults));
    return SUCCESS;
}

exception tekGuiGetTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    memcpy(defaults, &text_button_defaults, sizeof(struct TekGuiTextButtonDefaults));
    return SUCCESS;
}

exception tekGuiGetTextInputDefaults(struct TekGuiTextInputDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    memcpy(defaults, &text_input_defaults, sizeof(struct TekGuiTextInputDefaults));
    return SUCCESS;
}

void tekGuiDelete() {
    tek_gui_init = DE_INITIALISED;
    tek_gui_gl_init = DE_INITIALISED;
}

exception tekGuiGLLoad() {
    tekChainThrow(tekCreateBitmapFont("../res/urwgothic.ttf", 0, 64, &default_font));
    tek_gui_gl_init = INITIALISED;
    return SUCCESS;
}

tek_init tekGuiInit() {
    exception tek_exception = ymlReadFile("../tekgui/options.yml", &options_yml);
    if (tek_exception) return;

    tek_exception = tekAddDeleteFunc(tekGuiDelete);
    if (tek_exception) return;

    tek_exception = tekAddGLLoadFunc(tekGuiGLLoad);
    if (tek_exception) return;

    tek_exception = tekGuiLoadWindowDefaults(&window_defaults);
    if (tek_exception) {
        tekGuiLog("Error during TekGui Init: %d\n", tek_exception);
        tekPrintException();
        return;
    }

    tek_exception = tekGuiLoadListWindowDefaults(&list_window_defaults);
    if (tek_exception) {
        tekGuiLog("Error during TekGui Init: %d\n", tek_exception);
        tekPrintException();
        return;
    }

    tek_exception = tekGuiLoadTextButtonDefaults(&text_button_defaults);
    if (tek_exception) {
        tekGuiLog("Error during TekGui Init: %d\n", tek_exception);
        tekPrintException();
        return;
    }

    tek_exception = tekGuiLoadTextInputDefaults(&text_input_defaults);
    if (tek_exception) {
        tekGuiLog("Error during TekGui Init: %d\n", tek_exception);
        tekPrintException();
        return;
    }

    tek_gui_init = INITIALISED;
}

exception tekGuiGetDefaultFont(TekBitmapFont** font) {
    if (tek_gui_gl_init != INITIALISED)
        tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    *font = &default_font;
    return SUCCESS;
}
