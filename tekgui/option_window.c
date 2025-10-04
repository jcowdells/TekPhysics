#include "option_window.h"

#include <errno.h>
#include <cglm/vec3.h>

#include "../core/yml.h"
#include "tekgui.h"
#include <glad/glad.h>

#include "../core/priorityqueue.h"

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

/// Struct containing the pointer of the option window and the option display. Used in callbacks.
struct TekGuiOptionPair {
    TekGuiOptionWindow* window;
    TekGuiOption* option;
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
 * @return The input type, which will be -1 A.K.A. TEK_UNKNOWN_INPUT if the string was invalid.
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

static exception tekGuiLoadOptionIndex(YmlFile* yml_file, const char* key, double* index) {
    // empty variables to write into.
    YmlData* yml_data;

    // if no mention of an index, we can put it at the end.
    if (ymlGet(yml_file, &yml_data, "options", key, "index") != SUCCESS) {
        *index = UINT_MAX;
        return SUCCESS;
    }

    // otherwise, attempt to read the index that is wanted.
    tekChainThrow(ymlDataToFloat(yml_data, index));
    return SUCCESS;
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

    // order the keys in the order specified by the indices of each option.
    PriorityQueue priority_queue;
    priorityQueueCreate(&priority_queue);
    for (uint i = 0; i < num_keys; i++) {
        double option_index;
        tekChainThrow(tekGuiLoadOptionIndex(yml_file, keys[i], &option_index));
        tekChainThrow(priorityQueueEnqueue(&priority_queue, option_index, keys[i]));
    }

    // for each key, load the option and add it to the array.
    const PriorityQueueItem* item = priority_queue.queue;
    uint i = 0;
    while (item) {
        tekChainThrowThen(tekGuiLoadOption(yml_file, item->data, *options + i), {
            // on failure, we need to free the buffer.
            free(*options);
            *options = 0;
            priorityQueueDelete(&priority_queue);
        });
        item = item->next;
        i++;
    }

    priorityQueueDelete(&priority_queue);
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

static exception tekGuiWriteStringOption(TekGuiOptionWindow* window, const char* key, const char* string, const uint len_string) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    option_data->data.string = (char*)malloc(len_string * sizeof(char));
    if (!option_data->data.string)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate string buffer.");
    memcpy(option_data->data.string, string, len_string);
    return SUCCESS;
}

static exception tekGuiWriteNumberOption(TekGuiOptionWindow* window, const char* key, const double number) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    option_data->data.number = number;
    return SUCCESS;
}

static exception tekGuiWriteBooleanOption(TekGuiOptionWindow* window, const char* key, const flag boolean) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    option_data->data.boolean = boolean;
    return SUCCESS;
}

static exception tekGuiWriteVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(vector, option_data->data.vector_3);
    return SUCCESS;
}

static exception tekGuiWriteVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec4_copy(vector, option_data->data.vector_4);
    return SUCCESS;
}

static exception tekGuiWriteNumberOptionString(TekGuiOptionWindow* window, const char* key, const char* number_str) {
    char* endptr;
    const uint len_number_str = strlen(number_str) + 1;
    double number = strtod(number_str, &endptr);
    if (errno == ERANGE)
        number = 0.0;
    if (endptr != number_str + len_number_str)
        number = 0.0;
    tekChainThrow(tekGuiWriteNumberOption(window,  key, number));
    return SUCCESS;
}

static exception tekGuiWriteBooleanOptionString(TekGuiOptionWindow* window, const char* key, const char* boolean_str) {
    flag boolean = 0;
    if (!strcasecmp(boolean_str, "TRUE") || !strcasecmp(boolean_str, "YES") || !strcasecmp(boolean_str, "OK"))
        boolean = 1;
    tekChainThrow(tekGuiWriteBooleanOption(window, key, boolean));
    return SUCCESS;
}

static exception tekGuiWriteVecIndexOptionString(TekGuiOptionWindow* window, const char* key, const char* element_str, const uint index) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));

    char* endptr;
    uint len_element_str = strlen(element_str);
    float element = strtof(element_str, &endptr);
    if (errno == ERANGE)
        element = 0.0f;
    if (endptr != element_str + len_element_str)
        element = 0.0f;

    // vector_3 and vector_4 overlap, so doesn't matter if this was called for a vec3 or vec4.
    option_data->data.vector_4[index] = element;
    return SUCCESS;
}

static exception tekGuiReadStringOption(TekGuiOptionWindow* window, const char* key, char** string) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *string = option_data->data.string;
    return SUCCESS;
}

static exception tekGuiReadNumberOption(TekGuiOptionWindow* window, const char* key, double* number) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *number = option_data->data.number;
    return SUCCESS;
}

static exception tekGuiReadBooleanOption(TekGuiOptionWindow* window, const char* key, flag* boolean) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *boolean = option_data->data.boolean;
    return SUCCESS;
}

static exception tekGuiReadVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(option_data->data.vector_3, vector);
    return SUCCESS;
}

static exception tekGuiReadVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(option_data->data.vector_4, vector);
    return SUCCESS;
}

static exception tekGuiOptionInputCallback(TekGuiTextInput* text_input, const char* text, const uint len_text) {
    struct TekGuiOptionPair* option_pair = (struct TekGuiOptionPair*)text_input->data;
    TekGuiOptionWindow* window = option_pair->window;
    const TekGuiOption* option = option_pair->option;
    const char* name = option->display.input.name;
    const uint index = option->display.input.index;

    switch (option->type) {
    case TEK_STRING_INPUT:
        tekChainThrow(tekGuiWriteStringOption(window, name, text, len_text));
        break;
    case TEK_NUMBER_INPUT:
        tekChainThrow(tekGuiWriteNumberOptionString(window, name, text));
        break;
    case TEK_BOOLEAN_INPUT:
        tekChainThrow(tekGuiWriteBooleanOptionString(window, name, text));
        break;
    case TEK_VEC3_INPUT:
    case TEK_VEC4_INPUT:
        tekChainThrow(tekGuiWriteVecIndexOptionString(window, name, text, index));
        break;
    default:
        tekThrow(FAILURE, "Input called on unknown input type.");
    }

    return SUCCESS;
}

/**
 * Create a label for the window and add it to the list of displayed items.
 * @param[in] label The text that the label should display.
 * @param[in] text_height The height of the text for this label.
 * @param[in/out] option_display The array for which to add the label display to.
 * @param[in/out] option_index A pointer to the last written index of the array, will be incremented as more displays are added.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 */
static exception tekGuiCreateLabelOption(const char* label, const uint text_height, TekGuiOption* option_display, uint* option_index) {
    // empty option struct
    TekGuiOption* option = option_display + *option_index;
    (*option_index)++;

    // fill with label data
    option->type = TEK_LABEL;
    option->height = text_height * 5 / 4;
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText(label, text_height, font, &option->display.label));

    return SUCCESS;
}

/**
 * Create a single input display and add it to the list of displayed items.
 * @param[in] window The window which the input belongs to.
 * @param[in] name The name of the input, will be used to access it.
 * @param[in] type The type of the input, something like TEK_..._INPUT.
 * @param[in/out] option_display The array for which to add the option display to.
 * @param[in/out] option_index A pointer to the last written index of the array, will be incremented as more displays are added.
 * @throws OPENGL_EXCEPTION if something graphical fails.
 */
static exception tekGuiCreateSingleInput(TekGuiOptionWindow* window, const char* name, const flag type, const uint input_width, TekGuiOption* option_display, uint* option_index) {
    // get pointer to struct
    TekGuiOption* option = option_display + *option_index;
    (*option_index)++;

    // fill with input data
    option->type = type;
    tekChainThrow(tekGuiCreateTextInput(&option->display.input.text_input));
    option->height = option->display.input.text_input.button.hitbox_height;
    tekChainThrow(tekGuiSetTextInputSize(&option->display.input.text_input, input_width, option->height));

    // set up callbacks
    struct TekGuiOptionPair* option_pair = (struct TekGuiOptionPair*)malloc(sizeof(struct TekGuiOptionPair));
    option_pair->window = window;
    option_pair->option = option;
    option->display.input.text_input.data = option_pair;
    option->display.input.text_input.callback = tekGuiOptionInputCallback;
    option->display.input.name = name;
    option->display.input.index = 0; // index only relevant for multi-item inputs

    return SUCCESS;
}

/**
 * Create multiple input displays which are all linked to the same name, used for vector inputs
 * @param[in] window The window which the input belongs to.
 * @param[in] name The name of the input, used to access the inputted data.
 * @param[in] type The input type, something like TEK_..._INPUT.
 * @param[in] num_inputs The number of inputs to add.
*  @param[in/out] option_display The array for which to add the option displays to.
 * @param[in/out] option_index A pointer to the last written index of the array, will be incremented as more displays are added.
 * @throws OPENGL_EXCEPTION if any of the single inputs failed to be created.
 */
static exception tekGuiCreateMultiInput(TekGuiOptionWindow* window, const char* name, const flag type, const uint input_width, const uint num_inputs, TekGuiOption* option_display, uint* option_index) {
    // iterate for each input needed
    for (uint i = 0; i < num_inputs; i++) {
        // get the option pointer before the index is incremented.
        TekGuiOption* option = option_display + *option_index;

        // each has the same name, as pointing to the same option
        tekChainThrow(tekGuiCreateSingleInput(window, name, type, input_width, option_display, option_index));

        // once created, update the display index to whatever
        option->display.input.index = i;
    }

    return SUCCESS;
}

/**
 * Add a chunk of option displays based on a set of parameters. For example, a vec3 option will create 4 option displays, one for the label, and 3 inputs.
 * @param[in] window The window which to create the options for.
 * @param name The name of the option display, used to retrieve the data later on.
 * @param label The label, what will be displayed on the gui.
 * @param type The type of option, in the form TEK_..._INPUT or TEK_LABEL
 * @param text_height The height of the label text.
 * @param input_width The width of the text inputs.
 * @param option_display An array of option displays to add to.
 * @param option_index The last written index of the option_display array.
 * @throws OPENGL_EXCEPTION if something went wrong graphically at any stage.
 */
static exception tekGuiCreateOption(TekGuiOptionWindow* window, const char* name, const char* label, const flag type, const uint text_height, const uint input_width, TekGuiOption* option_display, uint* option_index) {
    tekChainThrow(tekGuiCreateLabelOption(label, text_height, option_display, option_index));

    switch (type) {
    case TEK_LABEL:
        return SUCCESS;
    case TEK_STRING_INPUT:
    case TEK_NUMBER_INPUT:
    case TEK_BOOLEAN_INPUT:
        tekChainThrow(tekGuiCreateSingleInput(window, name, type, input_width, option_display, option_index));
        break;
    case TEK_VEC3_INPUT:
        tekChainThrow(tekGuiCreateMultiInput(window, name, type, input_width, 3, option_display, option_index));
        break;
    case TEK_VEC4_INPUT:
        tekChainThrow(tekGuiCreateMultiInput(window, name, type, input_width, 4, option_display, option_index));
        break;
    default:
        tekThrow(YML_EXCEPTION, "Invalid input type for option.");
    }

    return SUCCESS;
}

/**
 * Draw a label, just a piece of text.
 * @param option A pointer to the option display where the label is stored.
 * @param x_pos The x coordinate to draw at.
 * @param y_pos The y coordinate to draw at.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 */
static exception tekGuiDrawOptionLabel(TekGuiOption* option, int x_pos, int y_pos) {
    float x = (float)x_pos + (float)option->height * 0.4f;
    float y = (float)y_pos;
    tekChainThrow(tekDrawText(&option->display.label, x, y));
    return SUCCESS;
}

/**
 * Draw an option input, which is a text input + link back to the option table.
 * @param option A pointer to the option display to draw.
 * @param x_pos The x coordinate to draw at.
 * @param y_pos The y coordinate to draw at.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 */
static exception tekGuiDrawOptionInput(TekGuiOption* option, int x_pos, int y_pos) {
    tekChainThrow(tekGuiSetTextInputPosition(&option->display.input.text_input, x_pos + option->height, y_pos));
    tekChainThrow(tekGuiDrawTextInput(&option->display.input.text_input));
    return SUCCESS;
}

/**
 * Draw an option display, this call will decide the appropriate method of drawing based on the type.
 * @param option A pointer to the option display to draw.
 * @param x_pos The x coordinate to draw at.
 * @param y_pos The y coordinate to draw at.
 * @throws OPENGL_EXCEPTION if something goes wrong graphically.
 */
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

/**
 * Draw callback function for option window, called after the base window is drawn.
 * @param window_ptr The base window.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 */
static exception tekGuiOptionWindowDrawCallback(TekGuiWindow* window_ptr) {
    TekGuiOptionWindow* window = (TekGuiOptionWindow*)window_ptr->data;

    int x_pos = window_ptr->x_pos;
    int y_pos = window_ptr->y_pos;
    for (uint i = 0; i < window->len_options; i++) {
        TekGuiOption* option = window->option_display + i;
        tekChainThrow(tekGuiDrawOption(option, x_pos, y_pos));
        y_pos += (int)option->height;
    }

    return SUCCESS;
}

/**
 * Create the base window of the option window. Simple stuff like setting size, position and title.
 * @param window The option window to create the window for.
 * @param defaults The default settings to apply to the window.
 * @throws MEMORY_EXCEPTION for a lot of the text buffer related things that can fail.
 */
static exception tekGuiCreateBaseWindow(TekGuiOptionWindow* window, struct TekGuiOptionsWindowDefaults* defaults) {
    tekChainThrow(tekGuiCreateWindow(&window->window));
    tekGuiSetWindowPosition(&window->window, defaults->x_pos, defaults->y_pos);
    tekGuiSetWindowSize(&window->window, defaults->width, defaults->height);
    tekChainThrow(tekGuiSetWindowTitle(&window->window, defaults->title));
    window->window.draw_callback = tekGuiOptionWindowDrawCallback;
    window->window.data = window;
    return SUCCESS;
}

/**
 * Get the size (the number of option displays required) to fit a type of option display.
 * @param type The type of option display.
 * @return The size.
 */
static uint tekGuiGetOptionDisplaySize(const flag type) {
    switch (type) {
    case TEK_LABEL:
        return 1;
    case TEK_STRING_INPUT:
    case TEK_NUMBER_INPUT:
    case TEK_BOOLEAN_INPUT:
        return 2;
    case TEK_VEC3_INPUT:
        return 4;
    case TEK_VEC4_INPUT:
        return 5;
    default:
        return 0;
    }
}

/**
 * Get the total size of an array of options, for example a label, a vec3 input and a string input:
 * @code
 * 1 A Label
 * 2 A Vec3 Input:
 * 3  [    ]
 * 4  [    ]
 * 5  [    ]
 * 6 A String Input:
 * 7  [    ]
 * @endcode
 * Will be a size of 7.
 * @param options The array of options.
 * @param len_options The number of options in the array.
 * @return The total size.
 */
static uint tekGuiGetOptionsDisplayTotalSize(struct TekGuiOptionsWindowOption* options, const uint len_options) {
    uint total_len = 0;
    for (uint i = 0; i < len_options; i++) {
        total_len += tekGuiGetOptionDisplaySize(options[i].type);
    }
    return total_len;
}

/**
 * Create an option window from a yml file.
 * @param options_yml The filepath of the yml file.
 * @param window A pointer to an empty struct which will be filled with the option window data.
 * @throws OPENGL_EXCEPTION if something fails graphically.
 * @throws MEMORY_EXCEPTION in a few scenarios to do with allocating buffers for text/titles.
 */
exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window) {
    YmlFile yml_file = {};
    tekChainThrow(ymlReadFile(options_yml, &yml_file));

    struct TekGuiOptionsWindowDefaults defaults = {};
    struct TekGuiOptionsWindowOption* options;
    uint len_options = 0;
    tekChainThrowThen(tekGuiLoadOptionsYml(&yml_file, &defaults, &options, &len_options), {
        ymlDelete(&yml_file);
    });

    window->len_options = tekGuiGetOptionsDisplayTotalSize(options, len_options);
    window->option_display = (TekGuiOption*)malloc(window->len_options * sizeof(TekGuiOption));
    if (!window->option_display)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for options display array.");

    uint option_index = 0;
    for (uint i = 0; i < len_options; i++) {
        tekChainThrowThen(tekGuiCreateOption(window, options[i].name, options[i].label, options[i].type, defaults.text_height, defaults.input_width, window->option_display, &option_index), {
            ymlDelete(&yml_file);
        });
    }

    tekChainThrowThen(tekGuiCreateBaseWindow(window, &defaults), {
        ymlDelete(&yml_file);
    });

    ymlDelete(&yml_file);
    return SUCCESS;
}
