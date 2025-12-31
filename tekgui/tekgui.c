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

/**
 * Get a colour from the options.yml file.
 * @param option_name The name of the option where the colour is stored.
 * @param colour_name The name of the colour.
 * @param colour The colour that is contained there.
 * @throws YML_EXCEPTION if either of the keys does not exist or does not point to a colour.
 */
static exception tekGuiGetOptionsColour(const char* option_name, const char* colour_name, vec4 colour) {
    // this code demonstrates how boilerplatey my yml code is lmao
    // just assume that it has an r g b and a component
    // and just jam all that into a vec4
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

/**
 * Load the default values for a window into a struct. Has default values if nothing specified in options.yml
 * @param defaults A pointer to an empty TekGuiWindowDefaults struct.
 * @throws YML_EXCEPTION if the yml is invalid.
 */
static exception tekGuiLoadWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    YmlData* yml_data;

    // if no values provided, give benefit of doubt and provide some defaults
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

    // load values from options yml
    long yml_integer;

    // starting x position
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    // starting y position
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    // width
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    // height
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->height = (uint)yml_integer;

    // height of title (width?)
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "title_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->title_width = (uint)yml_integer;

    // width of border
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    // title text
    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "title"));
    tekChainThrow(ymlDataToString(yml_data, &defaults->title));

    // colours
    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "border_colour", defaults->border_colour));
    tekChainThrow(tekGuiGetOptionsColour("window_defaults", "title_colour", defaults->title_colour));

    return SUCCESS;
}

/**
 * Load the default values from options.yml into a struct. Has default values if nothing specified in options.yml
 * @param defaults A pointer to a TekGuiListWindowDefaults struct.
 * @throws YML_EXCEPTION if the yml file is invalid.
 */
static exception tekGuiLoadListWindowDefaults(struct TekGuiListWindowDefaults* defaults) {
    YmlData* yml_data;

    // if no defaults found, give benefit of doubt and provide some default values
    const exception tek_exception = ymlGet(&options_yml, &yml_data, "list_window_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing list window defaults section in 'options.yml'.");
        defaults->text_size = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->text_colour);
        return SUCCESS;
    }

    // otherwise load defaults
    long yml_integer;

    // text height
    tekChainThrow(ymlGet(&options_yml, &yml_data, "list_window_defaults", "text_size"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_size = (uint)yml_integer;

    // maximum number of text entries to display.
    tekChainThrow(ymlGet(&options_yml, &yml_data, "list_window_defaults", "num_visible"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->num_visible = (uint)yml_integer;

    // text colour
    tekChainThrow(tekGuiGetOptionsColour("list_window_defaults", "text_colour", defaults->text_colour));
}

/**
 * Load the default values for a text button from the options.yml file. Has some default values if none found.
 * @param defaults A pointer to a TekGuiTextButtonDefaults struct that is filled with the data.
 * @throws YML_EXCEPTION if the yml file is invalid.
 */
static exception tekGuiLoadTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults) {
    YmlData* yml_data;

    // attemp to get yml data, if it does not exist then give benefit of doubt and provide some defaults
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

    // otherwise read defaults
    long yml_integer;

    // x position
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    // y position
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    // width
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    // height of button
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->height = (uint)yml_integer;

    // width of border
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    // height of text
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_button_defaults", "text_height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_height = (uint)yml_integer;

    // colours
    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "selected_colour", defaults->selected_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_button_defaults", "border_colour", defaults->border_colour));

    return SUCCESS;
}

/**
 * Load the text input default values from the options.yml file. Has some default values
 * @param defaults The outputted default values for text input.
 * @throws YML_EXCEPTION if there are missing defaults.
 */
static exception tekGuiLoadTextInputDefaults(struct TekGuiTextInputDefaults* defaults) {
    YmlData* yml_data;

    // if there is no section at all, we can use some default values, give benefit of doubt
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

    // otherwise we will use the values specified by the user
    // for each item we need to go through the arduous process of using my yml file thing
    // need to get the yml data, convert to the correct type and then put into the defaults
    long yml_integer;

    // starting x pos
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "x_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->x_pos = (uint)yml_integer;

    // starting y pos
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "y_pos"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->y_pos = (uint)yml_integer;

    // width
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->width = (uint)yml_integer;

    // height of text
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "text_height"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->text_height = (uint)yml_integer;

    // border width
    tekChainThrow(ymlGet(&options_yml, &yml_data, "text_input_defaults", "border_width"));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    defaults->border_width = (uint)yml_integer;

    // background colour
    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "background_colour", defaults->background_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "border_colour", defaults->border_colour));
    tekChainThrow(tekGuiGetOptionsColour("text_input_defaults", "text_colour", defaults->text_colour));

    return SUCCESS;
}

/**
 * Get the default values for a window gui element.
 * @param defaults The outputted default values.
 * @throws FAILURE if TekGUI is not initialised.
 */
exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    // copy over the values.
    memcpy(defaults, &window_defaults, sizeof(struct TekGuiWindowDefaults));
    return SUCCESS;
}

/**
 * Get the default values for a list window.
 * @param defaults The outputted default values.
 * @throws FAILURE if tek gui not initialised.
 */
exception tekGuiGetListWindowDefaults(struct TekGuiListWindowDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    // copy over values
    memcpy(defaults, &list_window_defaults, sizeof(struct TekGuiListWindowDefaults));
    return SUCCESS;
}

/**
 * Get default values for tek gui button element.
 * @param defaults The outputted default values.
 * @throws FAILURE if tek gui is not initialised.
 */
exception tekGuiGetTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    // copy over the values.
    memcpy(defaults, &text_button_defaults, sizeof(struct TekGuiTextButtonDefaults));
    return SUCCESS;
}

/**
 * Get default values for text input gui elements.
 * @param defaults The outputted default values.
 * @throws FAILURE if tek gui is not initialised.
 */
exception tekGuiGetTextInputDefaults(struct TekGuiTextInputDefaults* defaults) {
    if (!tek_gui_init)
        tekThrow(FAILURE, "TekGUI is not initialised.")

    // copy over values that have been loaded already.
    memcpy(defaults, &text_input_defaults, sizeof(struct TekGuiTextInputDefaults));
    return SUCCESS;
}

/**
 * Delete callback for tek gui code.
 */
void tekGuiDelete() {
    // delete options yml
    ymlDelete(&options_yml);
    tek_gui_init = DE_INITIALISED;
    tek_gui_gl_init = DE_INITIALISED;
}

/**
 * Callback for when opengl has loaded for tek gui code. 
 * @throws FREETYPE_EXCEPTION if could not load default font.
 */
exception tekGuiGLLoad() {
    // default font is hard coded... lol
    tekChainThrow(tekCreateBitmapFont("../res/urwgothic.ttf", 0, 64, &default_font));
    tek_gui_gl_init = INITIALISED;
    return SUCCESS;
}

/**
 * Initialisation function for tek gui base / loader code.
 */
tek_init tekGuiInit() {
    // reading options yml
    exception tek_exception = ymlReadFile("../tekgui/options.yml", &options_yml);
    if (tek_exception) return;

    // setup callbacks
    tek_exception = tekAddDeleteFunc(tekGuiDelete);
    if (tek_exception) return;

    tek_exception = tekAddGLLoadFunc(tekGuiGLLoad);
    if (tek_exception) return;

    // load defaults for different areas
    // also display if init failed, likely that people could mess with these config files.
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

    // if worked without errors, then we are initialised
    tek_gui_init = INITIALISED;
}

/**
 * Get the default font for the program.
 * @param font The outputted default font.
 * @throws OPENGL_EXCEPTION if opengl not initialised.
 */
exception tekGuiGetDefaultFont(TekBitmapFont** font) {
    if (tek_gui_gl_init != INITIALISED)
        tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    // literally what it says on the tin
    *font = &default_font;
    return SUCCESS;
}
