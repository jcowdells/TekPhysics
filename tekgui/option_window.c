#include "option_window.h"

#include "../core/yml.h"
#include "tekgui.h"
#include <glad/glad.h>

#define TEK_UNKNOWN_INPUT -1
#define TEK_LABEL          0
#define TEK_STRING_INPUT   1
#define TEK_NUMBER_INPUT   2
#define TEK_BOOLEAN_INPUT  3
#define TEK_VEC3_INPUT     4
#define TEK_VEC4_INPUT     5

/// Struct containing data about the window rendering.
struct TekGuiOptionsWindowDefaults {
    const char* title;
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    uint text_height;
    uint input_width;
};

/// Struct containing data about an input option in the window.
struct TekGuiOptionsWindowOption {
    char* name;
    char* label;
    flag type;
};

/**
 * Load a single unsigned integer given a key and a yml file.
 * @param[in] yml_file The yml file to load the uint from.
 * @param[in] key The string key that specifies the uint.
 * @param[out] uint_ptr A pointer to where the uint should be written to.
 * @throws YML_EXCEPTION if the key does not exist.
 */
static exception tekGuiLoadOptionsUint(YmlFile* yml_file, const char* key, uint* uint_ptr) {
    // empty variables to write into.
    YmlData* yml_data;
    long yml_integer;

    // collect data from yml files
    tekChainThrow(ymlGet(yml_file, &yml_data, key));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));

    // cast long to uint and write to output
    *uint_ptr = (uint)yml_integer;

    return SUCCESS;
}

/**
 * Load a string from a yml file given a key.
 * @param[in] yml_file The yml file to read from
 * @param[in] key The key from which to get the string.
 * @param[out] string A pointer to a char pointer which will be set to a pointer to a new buffer containing the string.
 * @throws YML_EXCEPTION if the yml key does not exist.
 * @throws MEMORY_EXCEPTION if the string buffer could not be allocated.
 * @note This function allocates memory which needs to be freed.
 */
static exception tekGuiLoadOptionsString(YmlFile* yml_file, const char* key, char** string) {
    // empty variable to write into
    YmlData* yml_data;

    // collect data from yml file
    tekChainThrow(ymlGet(yml_file, &yml_data, key));
    tekChainThrow(ymlDataToString(yml_data, string));

    return SUCCESS;
}

/**
 * Get the type of user input from a string.
 * @param[in] option_type The string to get the option type from, of the form "$tek_..._input"
 * @return The input type, which will be -1 A.K.A TEK_UNKNOWN_INPUT if the string was invalid.
 */
static flag tekGuiGetOptionInputType(const char* option_type) {
    if (!strcmp(option_type, "$tek_label"))
        return TEK_LABEL;

    if (!strcmp(option_type, "$tek_string_input"))
        return TEK_STRING_INPUT;

    if (!strcmp(option_type, "$tek_number_input"))
        return TEK_NUMBER_INPUT;

    if (!strcmp(option_type, "$tek_boolean_input"))
        return TEK_BOOLEAN_INPUT;

    if (!strcmp(option_type, "$tek_vec3_input"))
        return TEK_VEC3_INPUT;

    if (!strcmp(option_type, "$tek_vec4_input"))
        return TEK_VEC4_INPUT;

    return TEK_UNKNOWN_INPUT;
}

/**
 * Load a single option from a yml file given the name of the input.
 * @param[in] yml_file The yml file to read from.
 * @param[in] key The key that the option is stored under.
 * @param[out] option A pointer to a struct which will be filled with the data of this option.
 * @throws YML_EXCEPTION if the key does not exist.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @note This function will allocate memory that needs to be freed.
 */
static exception tekGuiLoadOption(YmlFile* yml_file, char* key, struct TekGuiOptionsWindowOption* option) {
    // empty variable to store yml stuff
    YmlData* yml_data;

    // allows user to access this option later using the name it was created with
    option->name = key;

    // get the label as a string 
    tekChainThrow(ymlGet(yml_file, &yml_data, "options", key, "label"));
    tekChainThrow(ymlDataToString(yml_data, &option->label));

    // get the type as a string
    char* type_string;
    tekChainThrow(ymlGet(yml_file, &yml_data, "options", key, "type"));
    tekChainThrow(ymlDataToString(yml_data, &type_string));

    // convert string to flag and free the string as it's no longer needed.
    option->type = tekGuiGetOptionInputType(type_string);
    free(type_string);

    // unknown input should not be accepted
    if (option->type == TEK_UNKNOWN_INPUT)
        tekThrow(YML_EXCEPTION, "Unknown input type for option window.");

    return SUCCESS;
}

/**
 * Load all options as described in a yml file.
 * @param[in] yml_file The yml file to read from.
 * @param[in] keys An array of keys that specify the options to load
 * @param[in] num_keys The number of keys in the keys array
 * @param[out] options A pointer to a pointer that will be set to a new buffer containing the loaded options.
 * @throws MEMORY_EXCEPTION if malloc() fails
 * @throws YML_EXCEPTION if any of the keys do not exist in the yml file.
 */
static exception tekGuiLoadOptions(YmlFile* yml_file, char** keys, uint num_keys, struct TekGuiOptionsWindowOption** options) {
    // mallocate sum mem
    *options = (struct TekGuiOptionsWindowOption*)malloc(num_keys * sizeof(struct TekGuiOptionsWindowOption));
    if (!(*options))
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for options buffer.");

    // for each key, load the option and add it to the array.
    for (uint i = 0; i < num_keys; i++) {
        tekChainThrowThen(tekGuiLoadOption(yml_file, keys[i], *options + i), {
            // on failure, we need to free the buffer.
            free(*options);
            *options = 0;
        });
    }
    return SUCCESS;
}

/**
 * Load all data for an options window including window metadata from a yml file.
 * @param[in] The yml file to load the data from.
 * @param[out] defaults The window defaults struct containing generic window metadata.
 * @param[out] options An array of options data containing each option.
 * @param[out] len_options A pointer to where the length of the new options array can be written.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws YML_EXCEPTION if the yml file is malformed.
 * @note The options array will be allocated by the function and needs to be freed.
 */
static exception tekGuiLoadOptionsYml(YmlFile* yml_file, struct TekGuiOptionsWindowDefaults* defaults, struct TekGuiOptionsWindowOption** options, uint* len_options) {
    *len_options = 0;

    // load window defaults by name
    tekChainThrow(tekGuiLoadOptionsString(yml_file, "title", &defaults->title));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "x_pos", &defaults->x_pos));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "y_pos", &defaults->y_pos));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "width", &defaults->width));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "height", &defaults->height));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "text_height", &defaults->text_height));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "input_width", &defaults->input_width));

    // load in the array of keys
    char** keys;
    uint num_keys = 0;
    tekChainThrow(ymlGetKeys(yml_file, &keys, &num_keys, "options"));

    // load options and update length if no errors.
    tekChainThrow(tekGuiLoadOptions(yml_file, keys, num_keys, options));
    *len_options = num_keys;

    return SUCCESS;
}

/**
 * Create a label for the window and add it to the list of displayed items.
 * @param[in] label The text that the label should display.
 * @param[in] text_height The height of the text for this label.
 * @param[in/out] option_display The vector for which to add the label display to.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 */
static exception tekGuiCreateLabelOption(const char* label, const uint text_height, Vector* option_display) {
    // empty option struct
    TekGuiOption option = {};

    // fill with label data
    option.type = TEK_LABEL;
    option.height = text_height * 5 / 4;
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText(label, text_height, font, &option.display.label));

    // add to option vector.
    tekChainThrow(vectorAddItem(option_display, &option));

    return SUCCESS;
}

static void tekGuiOptionInputCallback(struct TekGuiTextInput* text_input, const char* text, uint len_text) {
    printf("Recieved text callback: %s\n", text);
}

/**
 * Create a single input display and add it to the list of displayed items.
 * @param[in] name The name of the input, will be used to access it
 * @param[in] type The type of the input, something like TEK_..._INPUT
 * @param[in/out] option_display The vector to add the display item to.
 * @throws OPENGL_EXCEPTION if something graphical fails.
 */
static exception tekGuiCreateSingleInput(const char* name, const flag type, const uint input_width, Vector* option_display) {
    // emtpy option struct
    TekGuiOption empty_option = {};
    tekChainThrow(vectorAddItem(option_display, &empty_option));

    // get pointer to struct
    TekGuiOption* option;
    tekChainThrow(vectorGetItemPtr(option_display, option_display->length - 1,  &option));

    // fill with input data
    option->type = type;
    tekChainThrow(tekGuiCreateTextInput(&option->display.input.text_input));
    option->height = (uint)option->display.input.text_input.button.hitbox_height;
    tekChainThrow(tekGuiSetTextInputSize(&option->display.input.text_input, input_width, option->height));

    // set up callbacks
    option->display.input.text_input.data = option;
    option->display.input.text_input.callback = tekGuiOptionInputCallback;
    option->display.input.name = name;
    option->display.input.index = 0; // index only relevant for multi-item inputs

    return SUCCESS;
}

/**
 * Create multiple input displays which are all linked to the same name, used for vector inputs
 * @param[in] name The name of the input, used to access the inputted data.
 * @param[in] type The input type, something like TEK_..._INPUT
 * @param[in] num_inputs The number of inputs to add.
 * @param[in/out] option_display The vector to add all the display items to.
 * @throws OPENGL_EXCEPTION if any of the single inputs failed to be created.
 */
static exception tekGuiCreateMultiInput(const char* name, const flag type, const uint input_width, const uint num_inputs, Vector* option_display) {
    // iterate for each input needed
    for (uint i = 0; i < num_inputs; i++) {
        // each has the same name, as pointing to the same option
        tekChainThrow(tekGuiCreateSingleInput(name, type, input_width, option_display));

        // once created, update the display index to whatever
        TekGuiOption* option;
        tekChainThrow(vectorGetItemPtr(option_display, option_display->length - 1, &option));
        option->display.input.index = i;
    }

    return SUCCESS;
}

static exception tekGuiCreateOption(const char* name, const char* label, const flag type, const uint text_height, const uint input_width, Vector* option_display) {
    tekChainThrow(tekGuiCreateLabelOption(label, text_height, option_display));

    switch (type) {
    case TEK_LABEL:
        return SUCCESS;
    case TEK_STRING_INPUT:
    case TEK_NUMBER_INPUT:
    case TEK_BOOLEAN_INPUT:
        tekChainThrow(tekGuiCreateSingleInput(name, type, input_width, option_display));
        break;
    case TEK_VEC3_INPUT:
        tekChainThrow(tekGuiCreateMultiInput(name, type, input_width, 3, option_display));
        break;
    case TEK_VEC4_INPUT:
        tekChainThrow(tekGuiCreateMultiInput(name, type, input_width, 4, option_display));
        break;
    default:
        tekThrow(YML_EXCEPTION, "Invalid input type for option.");
    }

    return SUCCESS;
}

static exception tekGuiDrawOptionLabel(TekGuiOption* option, int x_pos, int y_pos) {
    float x = (float)x_pos + (float)option->height * 0.4f;
    float y = (float)y_pos + (float)option->height * 0.1f;
    glDepthFunc(GL_ALWAYS);
    tekChainThrow(tekDrawText(&option->display.label, x_pos, y_pos));
    printf("Drawing @ %f %f\n", x,y );
    return SUCCESS;
}

static exception tekGuiDrawOptionInput(TekGuiOption* option, int x_pos, int y_pos) {
    tekChainThrow(tekGuiSetTextInputPosition(&option->display.input.text_input, x_pos + option->height, y_pos));
    tekChainThrow(tekGuiDrawTextInput(&option->display.input.text_input));
    return SUCCESS;
}

static exception tekGuiDrawOption(TekGuiOption* option, int x_pos, int y_pos) {
    switch (option->type) {
    case TEK_LABEL:
        tekGuiDrawOptionLabel(option, x_pos, y_pos);
        break;
    case TEK_STRING_INPUT:
    case TEK_NUMBER_INPUT:
    case TEK_BOOLEAN_INPUT:
    case TEK_VEC3_INPUT:
    case TEK_VEC4_INPUT:
        tekGuiDrawOptionInput(option, x_pos, y_pos);
        break;
    default:
        tekThrow(FAILURE, "Cannot draw option of unknown type.");
    }

    return SUCCESS;
}

static exception tekGuiOptionWindowDrawCallback(TekGuiWindow* window_ptr) {
    TekGuiOptionWindow* window = (TekGuiOptionWindow*)window_ptr->data;

    uint x_pos = (uint)window_ptr->x_pos;
    uint y_pos = (uint)window_ptr->y_pos;
    for (uint i = 0; i < window->option_display.length; i++) {
        TekGuiOption* option;
        tekChainThrow(vectorGetItemPtr(&window->option_display, i, &option));
        tekChainThrow(tekGuiDrawOption(option, x_pos, y_pos));
        y_pos += option->height;
    }

    return SUCCESS;
}

static exception tekGuiCreateBaseWindow(TekGuiOptionWindow* window, struct TekGuiOptionsWindowDefaults* defaults) {
    tekChainThrow(tekGuiCreateWindow(&window->window));
    tekGuiSetWindowPosition(&window->window, defaults->x_pos, defaults->y_pos);
    tekGuiSetWindowSize(&window->window, defaults->width, defaults->height);
    tekChainThrow(tekGuiSetWindowTitle(&window->window, defaults->title));
    window->window.draw_callback = tekGuiOptionWindowDrawCallback;
    window->window.data = window;
    return SUCCESS;
}

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window) {
    tekChainThrow(vectorCreate(4, sizeof(TekGuiOption), &window->option_display));

    YmlFile yml_file = {};
    tekChainThrow(ymlReadFile(options_yml, &yml_file));

    struct TekGuiOptionsWindowDefaults defaults = {};
    struct TekGuiOptionsWindowOption* options;
    uint len_options = 0;
    tekChainThrowThen(tekGuiLoadOptionsYml(&yml_file, &defaults, &options, &len_options), {
        ymlDelete(&yml_file);
    });

    for (uint i = 0; i < len_options; i++) {
        tekChainThrowThen(tekGuiCreateOption(options[i].name, options[i].label, options[i].type, defaults.text_height, defaults.input_width, &window->option_display), {
            ymlDelete(&yml_file);
        });
    }

    tekChainThrowThen(tekGuiCreateBaseWindow(window, &defaults), {
        ymlDelete(&yml_file);
    });

    ymlDelete(&yml_file);
    return SUCCESS;
}
