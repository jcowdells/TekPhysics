#include "option_window.h"

#include <errno.h>
#include <cglm/vec3.h>

#include "../core/yml.h"
#include "tekgui.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../core/priorityqueue.h"

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

    if (!strcmp(option_type, "$tek_button_input"))
        return TEK_BUTTON_INPUT;

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

exception tekGuiReadStringOption(TekGuiOptionWindow* window, const char* key, char** string) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *string = option_data->data.string;
    return SUCCESS;
}

exception tekGuiReadNumberOption(TekGuiOptionWindow* window, const char* key, double* number) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *number = option_data->data.number;
    return SUCCESS;
}

exception tekGuiReadBooleanOption(TekGuiOptionWindow* window, const char* key, flag* boolean) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    *boolean = option_data->data.boolean;
    return SUCCESS;
}

exception tekGuiReadVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(option_data->data.vector_3, vector);
    return SUCCESS;
}

exception tekGuiReadVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(option_data->data.vector_4, vector);
    return SUCCESS;
}

static exception tekGuiUpdateOptionInputText(TekGuiOptionWindow* window, TekGuiOption* option) {
    // for strings, we already have the text to write direct into text.
    if (option->type == TEK_STRING_INPUT) {
        char* string;
        tekChainThrow(tekGuiReadStringOption(window, option->display.input.name, &string));
        tekChainThrow(tekGuiSetTextInputText(&option->display.input.text_input, string));
        return SUCCESS;
    }

    // for other ones, gotta convert into a string first.
    const uint len_buffer = 128;
    char buffer[len_buffer];

    double number;
    flag boolean;
    vec4 vector;
    switch (option->type) {
    case TEK_NUMBER_INPUT:
        tekChainThrow(tekGuiReadNumberOption(window, option->display.input.name, &number));
        snprintf(buffer, len_buffer, "%.5f", number);
        break;
    case TEK_BOOLEAN_INPUT:
        tekChainThrow(tekGuiReadBooleanOption(window, option->display.input.name, &boolean));
        if (boolean) {
            snprintf(buffer, len_buffer, "True");
        } else {
            snprintf(buffer, len_buffer, "False");
        }
        break;
    case TEK_VEC3_INPUT:
        tekChainThrow(tekGuiReadVec3Option(window, option->display.input.name, vector));
        snprintf(buffer, len_buffer, "%.5f", vector[option->display.input.index]);
        break;
    case TEK_VEC4_INPUT:
        tekChainThrow(tekGuiReadVec4Option(window, option->display.input.name, vector));
        snprintf(buffer, len_buffer, "%.5f", vector[option->display.input.index]);
        break;
    default:
        tekThrow(FAILURE, "Unknown input type.");
    }

    tekChainThrow(tekGuiSetTextInputText(&option->display.input.text_input, buffer));

    return SUCCESS;
}

static flag tekGuiIsOptionInput(const TekGuiOption* option) {
    switch (option->type) {
    case TEK_LABEL:
    case TEK_BUTTON_INPUT:
    case TEK_UNKNOWN_INPUT:
        return 0;
    default:
        return 1;
    }
}

static exception tekGuiUpdateInputOption(TekGuiOptionWindow* window, const char* key) {
    for (uint i = 0; i < window->len_options; i++) {
        TekGuiOption* option = window->option_display + i;
        if (!tekGuiIsOptionInput(option))
            continue;
        if (!strcmp(option->display.input.name, key)) {
            tekChainThrow(tekGuiUpdateOptionInputText(window, option));
        }
    }
    return SUCCESS;
}

exception tekGuiWriteStringOption(TekGuiOptionWindow* window, const char* key, const char* string, const uint len_string) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    if (option_data->data.string)
        free(option_data->data.string);
    option_data->data.string = (char*)malloc(len_string * sizeof(char));
    if (!option_data->data.string)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate string buffer.");
    memcpy(option_data->data.string, string, len_string);
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

exception tekGuiWriteNumberOption(TekGuiOptionWindow* window, const char* key, const double number) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    option_data->data.number = number;
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

exception tekGuiWriteBooleanOption(TekGuiOptionWindow* window, const char* key, const flag boolean) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    option_data->data.boolean = boolean;
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

exception tekGuiWriteVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec3_copy(vector, option_data->data.vector_3);
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

exception tekGuiWriteVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));
    glm_vec4_copy(vector, option_data->data.vector_4);
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

static exception tekGuiWriteNumberOptionString(TekGuiOptionWindow* window, const char* key, const char* number_str) {
    char* endptr;
    const uint len_number_str = strlen(number_str) + 1;
    double number = strtod(number_str, &endptr);

    if (errno == ERANGE)
        number = 0.0;
    if (endptr != number_str + len_number_str - 1)
        number = 0.0;

    tekChainThrow(tekGuiWriteNumberOption(window,  key, number));
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

static exception tekGuiWriteBooleanOptionString(TekGuiOptionWindow* window, const char* key, const char* boolean_str) {
    flag boolean = 0;
    if (!strcasecmp(boolean_str, "TRUE") || !strcasecmp(boolean_str, "YES") || !strcasecmp(boolean_str, "OK"))
        boolean = 1;
    tekChainThrow(tekGuiWriteBooleanOption(window, key, boolean));
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

static exception tekGuiWriteVecIndexOptionString(TekGuiOptionWindow* window, const char* key, const char* element_str, const uint index) {
    TekGuiOptionData* option_data;
    tekChainThrow(hashtableGet(&window->option_data, key, (void**)&option_data));

    char* endptr;
    uint len_element_str = strlen(element_str) + 1;
    float element = strtof(element_str, &endptr);
    if (errno == ERANGE)
        element = 0.0f;
    if (endptr != element_str + len_element_str - 1)
        element = 0.0f;

    // vector_3 and vector_4 overlap, so doesn't matter if this was called for a vec3 or vec4.
    option_data->data.vector_4[index] = element;
    tekChainThrow(tekGuiUpdateInputOption(window, key));
    return SUCCESS;
}

static exception tekGuiWriteDefaultValue(TekGuiOptionWindow* window, const char* name, const flag type) {
    // mallocate the option data
    TekGuiOptionData* option_data = (TekGuiOptionData*)malloc(sizeof(TekGuiOptionData));
    if (!option_data)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for option data.");
    option_data->type = type;

    // set the data based on type.
    switch (type) {
    case TEK_STRING_INPUT:
        option_data->data.string = 0;
        break;
    case TEK_NUMBER_INPUT:
        option_data->data.number = 0.0;
        break;
    case TEK_BOOLEAN_INPUT:
        option_data->data.boolean = 0;
        break;
    case TEK_VEC3_INPUT:
        glm_vec3_zero(option_data->data.vector_3);
        break;
    case TEK_VEC4_INPUT:
        glm_vec4_zero(option_data->data.vector_4);
        break;
    default:
        free(option_data);
        tekThrow(FAILURE, "Unknown option type.");
    }

    // set the pointer at hash table.
    tekChainThrow(hashtableSet(&window->option_data, name, option_data));
    return SUCCESS;
}

static exception tekGuiOptionInputCallback(TekGuiTextInput* text_input, const char* text, const uint len_text) {
    struct TekGuiOptionPair* option_pair = (struct TekGuiOptionPair*)text_input->data;
    TekGuiOptionWindow* window = option_pair->window;
    TekGuiOption* option = option_pair->option;
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

    TekGuiOptionWindowCallbackData callback_data;
    callback_data.type = option->type;
    callback_data.name = name;
    if (window->callback) tekChainThrow(window->callback(window, callback_data));

    tekChainThrow(tekGuiUpdateOptionInputText(window, option));

    return SUCCESS;
}

static void tekGuiButtonInputCallback(TekGuiTextButton* button, TekGuiButtonCallbackData callback_data) {
    struct TekGuiOptionPair* option_pair = (struct TekGuiOptionPair*)button->data;
    TekGuiOptionWindow* window = option_pair->window;
    const TekGuiOption* option = option_pair->option;

    // if there is no callback just instantly quit
    if (!window->callback)
        return;

    // make sure that there is a left click or else quit
    if (callback_data.type != TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK
        || callback_data.data.mouse_button.action != GLFW_RELEASE
        || callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    // fill up some callback data
    TekGuiOptionWindowCallbackData outbound_callback_data;
    outbound_callback_data.type = TEK_BUTTON_INPUT;
    outbound_callback_data.name = option->display.button.name;

    // pass control onto the user
    window->callback(window, outbound_callback_data);
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
    if (!option_pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for option pair.");
    option_pair->window = window;
    option_pair->option = option;
    option->display.input.text_input.data = option_pair;
    option->display.input.text_input.callback = tekGuiOptionInputCallback;
    option->display.input.name = name;
    option->display.input.index = 0; // index only relevant for multi-item inputs

    // add an empty entry in the hash table.
    tekChainThrow(tekGuiWriteDefaultValue(window, name, type));
    tekChainThrow(tekGuiUpdateOptionInputText(window, option_display + *option_index - 1))

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

static exception tekGuiCreateButtonOption(TekGuiOptionWindow* window, const char* name, const char* label, TekGuiOption* option_display, uint* option_index) {
    // empty option struct
    TekGuiOption* option = option_display + *option_index;
    (*option_index)++;

    // set all the option data
    option->type = TEK_BUTTON_INPUT;
    tekChainThrow(tekGuiCreateTextButton(label, &option->display.button.button));
    option->height = option->display.button.button.button.hitbox_height + 10; // fudge factor 🔥🔥
    option->display.button.name = name;

    // set up callbacks
    struct TekGuiOptionPair* option_pair = (struct TekGuiOptionPair*)malloc(sizeof(struct TekGuiOptionPair));
    if (!option_pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for option pair.");
    option_pair->window = window;
    option_pair->option = option;
    option->display.button.button.callback = tekGuiButtonInputCallback;
    option->display.button.button.data = (void*)option_pair;

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
    // a button does not require a label, cuz it already has text on it
    if (type == TEK_BUTTON_INPUT) {
        tekChainThrow(tekGuiCreateButtonOption(window, name, label, option_display, option_index));
        return SUCCESS;
    }

    // for non-buttons, write a piece of text above
    tekChainThrow(tekGuiCreateLabelOption(label, text_height, option_display, option_index));

    // then, append the correct type of input below.
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
static exception tekGuiDrawOptionLabel(const TekGuiOption* option, const int x_pos, const int y_pos) {
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
static exception tekGuiDrawOptionInput(TekGuiOption* option, const int x_pos, const int y_pos) {
    tekChainThrow(tekGuiSetTextInputPosition(&option->display.input.text_input, x_pos + option->height, y_pos));
    tekChainThrow(tekGuiDrawTextInput(&option->display.input.text_input));
    return SUCCESS;
}

/**
 * Draw a button input, which is just a text button + link back to main window.
 * @param option The option to draw.
 * @param x_pos The x coordinate to draw at.
 * @param y_pos The y coordinate to draw at.
 * @return
 */
static exception tekGuiDrawButtonInput(TekGuiOption* option, const int x_pos, const int y_pos) {
    tekChainThrow(tekGuiSetTextButtonPosition(&option->display.button.button, x_pos + option->height * 0.4f, y_pos + 5));
    tekChainThrow(tekGuiDrawTextButton(&option->display.button.button));
    return SUCCESS;
}

/**
 * Draw an option display, this call will decide the appropriate method of drawing based on the type.
 * @param option A pointer to the option display to draw.
 * @param x_pos The x coordinate to draw at.
 * @param y_pos The y coordinate to draw at.
 * @throws OPENGL_EXCEPTION if something goes wrong graphically.
 */
static exception tekGuiDrawOption(TekGuiOption* option, const int x_pos, const int y_pos) {
    switch (option->type) {
    case TEK_LABEL:
        tekGuiDrawOptionLabel(option, x_pos, y_pos);
        break;
    case TEK_BUTTON_INPUT:
        tekGuiDrawButtonInput(option, x_pos, y_pos);
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

static exception tekGuiBringOptionToFront(TekGuiOption* option) {
    switch (option->type) {
    case TEK_BUTTON_INPUT:
        tekChainThrow(tekGuiBringButtonToFront(&option->display.button.button.button));
        break;
    case TEK_STRING_INPUT:
    case TEK_NUMBER_INPUT:
    case TEK_BOOLEAN_INPUT:
    case TEK_VEC3_INPUT:
    case TEK_VEC4_INPUT:
        tekChainThrow(tekGuiBringButtonToFront(&option->display.input.text_input.button));
        break;
    default:
        break;
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

static exception tekGuiOptionWindowSelectCallback(TekGuiWindow* window_ptr) {
    TekGuiOptionWindow* window = (TekGuiOptionWindow*)window_ptr->data;

    for (uint i = 0; i < window->len_options; i++) {
        TekGuiOption* option = window->option_display + i;
        tekChainThrow(tekGuiBringOptionToFront(option));
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
    window->window.select_callback = tekGuiOptionWindowSelectCallback;
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
    case TEK_BUTTON_INPUT:
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
    tekChainThrow(hashtableCreate(&window->option_data, 4));

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

void tekGuiDeleteOptionWindow(const TekGuiOptionWindow* window) {
    // TODO: this
}
