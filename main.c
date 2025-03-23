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
    
    tekLog(ymlGet(&yml_file, &yml_data, "hello", "ip_address"));
//    printf("ip address: %s\n", yml_data->value);
//    YmlData* yml_data;
//    ymlCreateStringData("hello", &yml_data);
//    YmlData* yml_2;
//    ymlCreateStringData("goodbye", &yml_2);
//    YmlData* yml_ptr = &yml_2;
//    tekLog(ymlCreate(&yml_file));
//    tekLog(ymlSet(&yml_file, yml_data, "test", "test2", "test3"));
//    tekLog(ymlSet(&yml_file, yml_2, "test", "test3"));
//    const char** keys;
//    hashtableGetKeys(&yml_file, &keys);
//    for (uint i = 0; i < yml_file.num_items; i++) {
//        printf("%s\n", keys[i]);
//    }
    tekLog(ymlPrint(&yml_file));

    printf("===> %s\n", yml_data->value);

    ymlDelete(&yml_file);
}

int main(void) {

    printf("size of void* = %lu", sizeof(double));
    tekInitExceptions();
    // if (render()) {
    //     tekPrintException();
    // }
    testYml();
    tekCloseExceptions();
}
