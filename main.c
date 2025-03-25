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
    tekLog(ymlReadFile("../res/test.yml", &yml_file));
    YmlData* yml_data;

    tekLog(ymlGet(&yml_file, &yml_data, "hello", "list"));
    char** array; uint length;
    tekLog(ymlListToStringArray(yml_data, &array, &length));

    for (uint i = 0; i < length; i++) {
        printf("%s\n", array[i]);
    }

    ymlDelete(&yml_file);
}

int main(void) {
    tekInitExceptions();
    // if (render()) {
    //     tekPrintException();
    // }
    testYml();
    tekCloseExceptions();
}
