#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include "core/exception.h"
#include "tekgl/mesh.h"
#include "tekgl/shader.h"
#include "tekgl/texture.h"
#include "tekgl/font.h"
#include "tekgl/text.h"

#include <cglm/cam.h>

#include "core/list.h"
#include "tekgl/manager.h"
#include "tekgui/primitives.h"
#include "core/yml.h"

#define printException(x) tekLog(x)

float width, height;

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
}

int render() {
    printException(tekInit("TekPhysics", 640, 480));
    width = 640.0f;
    height = 480.0f;

    tekAddFramebufferCallback(tekMainFramebufferCallback);

    TekBitmapFont verdana;
    printException(tekCreateFreeType());

    printException(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &verdana));

    TekText text;
    printException(tekCreateText("TekPhysics Alpha v1.0", 20, &verdana, &text));

    tekDeleteFreeType();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vec2 point_a = {0.0f, 0.0f};
    vec2 point_b = {640.0f, 480.0f};
    vec4 line_color = {1.f, 1.f, 1.f, 1.f};

    TekGuiLine line;

    printException(tekCreateLine(point_a, point_b, 1.f, line_color, &line));

    TekGuiOval oval;

    printException(tekCreateOval(point_a, point_b, 10.0f, 0, line_color, &oval));

    while (tekRunning()) {
        glClearColor(0.2f, 0.2f, 0.21f, 1.0f);

        printException(tekDrawLine(&line));
        printException(tekDrawOval(&oval));

        printException(tekDrawText(&text, width - 185.0f, 5.0f));
        tekUpdate();
    }

    tekDeleteTextEngine();
    tekDeleteText(&text);
    tekDelete();
    return SUCCESS;
}

void testYml() {
    YmlFile yml_file = {};
    YmlData* yml_data = (YmlData*)malloc(sizeof(YmlData));
    yml_data->type = 1;
    YmlData* yml_2 = (YmlData*)malloc(sizeof(YmlData));
    YmlData* yml_ptr = &yml_2;
    tekLog(ymlCreate(&yml_file));
    tekLog(ymlSet(&yml_file, yml_data, "test", "test2"));
    tekLog(ymlSet(&yml_file, yml_data, "test", "test3"));
    
    tekLog(ymlRemove(&yml_file, "test", "test3"));

    YmlData* internal_data;
    tekLog(hashtableGet(&yml_file, "test", &internal_data));
    HashTable* internal = internal_data->value;

    hashtablePrintItems(internal);
    tekLog(ymlGet(&yml_file, &yml_ptr, "test", "test3"));
    printf("yml_2 = {%d, %p}\n", yml_ptr->type, yml_ptr->value);
}

int main(void) {
    tekInitExceptions();
    // if (render()) {
    //     tekPrintException();
    // }
    testYml();
    tekCloseExceptions();
}
