#include "option_window.h"

#include "../core/yml.h"

#define TEK_UNKNOWN_INPUT -1
#define TEK_STRING_INPUT   0
#define TEK_NUMBER_INPUT   1

struct TekGuiOptionsWindowDefaults {
    const char* title;
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
};

struct TekGuiOptionsWindowOption {
    char* name;
    char* label;
    flag type;
};

static exception tekGuiLoadOptionsUint(YmlFile* yml_file, const char* key, uint* uint_ptr) {
    YmlData* yml_data;
    long yml_integer;

    tekChainThrow(ymlGet(yml_file, &yml_data, key));
    tekChainThrow(ymlDataToInteger(yml_data, &yml_integer));
    *uint_ptr = (uint)yml_integer;

    return SUCCESS;
}

/**
 * Load a string from a yml file given a key.
 * @param yml_file The yml file to read from
 * @param key The key from which to get the string.
 * @param string A pointer to a char pointer which will be set to a pointer to a new buffer containing the string.
 * @throws YML_EXCEPTION if the yml key does not exist.
 * @throws MEMORY_EXCEPTION if the string buffer could not be allocated.
 * @note This function allocates memory which needs to be freed.
 */
static exception tekGuiLoadOptionsString(YmlFile* yml_file, const char* key, char** string) {
    YmlData* yml_data;

    tekChainThrow(ymlGet(yml_file, &yml_data, key));
    tekChainThrow(ymlDataToString(yml_data, string));

    return SUCCESS;
}

static flag tekGuiGetOptionInputType(const char* option_type) {
    if (!strcmp(option_type, "$tek_string_input"))
        return TEK_STRING_INPUT;

    if (!strcmp(option_type, "$tek_number_input"))
        return TEK_NUMBER_INPUT;

    return TEK_UNKNOWN_INPUT;
}

static exception tekGuiLoadOption(YmlFile* yml_file, char* key, struct TekGuiOptionsWindowOption* option) {
    YmlData* yml_data;

    option->name = key;

    tekChainThrow(ymlGet(yml_file, &yml_data, "options", key, "label"));
    tekChainThrow(ymlDataToString(yml_data, &option->label));

    char* type_string;
    tekChainThrow(ymlGet(yml_file, &yml_data, "options", key, "type"));
    tekChainThrow(ymlDataToString(yml_data, &type_string));

    option->type = tekGuiGetOptionInputType(type_string);
    if (option->type == TEK_UNKNOWN_INPUT)
        tekThrow(YML_EXCEPTION, "Unknown input type for option window.");

    return SUCCESS;
}

static exception tekGuiLoadOptions(YmlFile* yml_file, char** keys, uint num_keys, struct TekGuiOptionsWindowOption** options) {
    *options = (struct TekGuiOptionsWindowOption*)malloc(num_keys * sizeof(struct TekGuiOptionsWindowOption));
    if (!(*options))
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for options buffer.");
    for (uint i = 0; i < num_keys; i++) {
        tekChainThrowThen(tekGuiLoadOption(yml_file, keys[i], &options[i]), {
            free(*options);
            *options = 0;
        });
    }
    return SUCCESS;
}

static exception tekGuiLoadOptionsYml(YmlFile* yml_file, struct TekGuiOptionsWindowDefaults* defaults, struct TekGuiOptionsWindowOption** options, uint* len_options) {
    *len_options = 0;

    tekChainThrow(tekGuiLoadOptionsString(yml_file, "title", &defaults->title));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "x_pos", &defaults->x_pos));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "y_pos", &defaults->y_pos));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "width", &defaults->width));
    tekChainThrow(tekGuiLoadOptionsUint(yml_file, "height", &defaults->height));

    char** keys;
    uint num_keys;
    tekChainThrow(ymlGetKeys(yml_file, &keys, &num_keys, "options"));

    tekChainThrow(tekGuiLoadOptions(yml_file, keys, num_keys, options));
    *len_options = num_keys;

    return SUCCESS;
}

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window) {
    YmlFile yml_file = {};
    tekChainThrow(ymlReadFile(options_yml, &yml_file));

    struct TekGuiOptionsWindowDefaults defaults = {};
    struct TekGuiOptionsWindowOption* options;
    uint len_options;

    tekChainThrowThen(tekGuiLoadOptionsYml(&yml_file, &defaults, &options, &len_options), {
        ymlDelete(&yml_file);
    });

    printf("window defaults: %s %u %u %u %u\n", defaults.title, defaults.x_pos, defaults.y_pos, defaults.width, defaults.height);
    printf("options:\n");
    for (uint i = 0; i < len_options; i++) {
        printf("    %s %s %d\n", options[i].name, options[i].label, options[i].type);
    }

    ymlDelete(&yml_file);
    return SUCCESS;
}
