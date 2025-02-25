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

#include "tekgl/manager.h"
#include "tekgui/primitives.h"

#define printException(x) if (x) tekPrintException()

#define GRID_SIZE 3
#define PADDING_X 120.0f
#define PADDING_Y 40.0f
#define GRID_THICKNESS 1.0f
#define X_TURN 1
#define O_TURN 2

float width, height;
double mouse_x = 0, mouse_y = 0;
flag turn = X_TURN;
vec4 black = {0.0f, 0.0f, 0.0f, 1.0f};

typedef struct Bbox {
    vec2 a;
    vec2 b;
} Bbox;

Bbox grid_sections[GRID_SIZE][GRID_SIZE];
flag grid_data[GRID_SIZE][GRID_SIZE];

typedef struct Cross {
    TekGuiLine left;
    TekGuiLine right;
} Cross;

int cross_index = 0, nought_index = 0;
Cross crosses[GRID_SIZE * GRID_SIZE / 2 + 1];
TekGuiOval noughts[GRID_SIZE * GRID_SIZE / 2 + 1];

exception createCross(vec2 point_a, vec2 point_b, const float thickness, vec4 color, Cross* cross) {
    tekChainThrow(tekCreateLine(point_a, point_b, thickness, color, &cross->left));
    vec2 flip_a = {point_a[0], point_b[1]};
    vec2 flip_b = {point_b[0], point_a[1]};
    tekChainThrow(tekCreateLine(flip_a, flip_b, thickness, color, &cross->right));
    return SUCCESS;
}

exception drawCross(const Cross* cross) {
    tekChainThrow(tekDrawLine(&cross->left));
    tekChainThrow(tekDrawLine(&cross->right));
    return SUCCESS;
}

void deleteCross(const Cross* cross) {
    tekDeleteLine(&cross->left);
    tekDeleteLine(&cross->right);
}

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
}

void mousePosCallback(GLFWwindow* window, const double x_pos, const double y_pos) {
    mouse_x = x_pos;
    mouse_y = y_pos;
}

void mouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods) {
    if (action == GLFW_PRESS) return;
    if (button != GLFW_MOUSE_BUTTON_1) return;
    int x_index = -1;
    int y_index = -1;
    for (int x = 0; x < GRID_SIZE; x++) {
        const float x_min = grid_sections[0][x].a[0];
        const float x_max = grid_sections[0][x].b[0];
        if ((mouse_x > x_min) && (mouse_x < x_max)) {
            x_index = x;
            break;
        }
    }
    for (int y = 0; y < GRID_SIZE; y++) {
        const float y_min = grid_sections[y][0].a[1];
        const float y_max = grid_sections[y][0].b[1];
        if ((mouse_y > y_min) && (mouse_y < y_max)) {
            y_index = y;
            break;
        }
    }
    if ((x_index != -1) && (y_index != -1)) {
        if (grid_data[y_index][x_index]) return;
        grid_data[y_index][x_index] = turn;
        Bbox section = grid_sections[y_index][x_index];
        if (turn == X_TURN) {
            printException(createCross(section.a, section.b, 2.f, black, &crosses[cross_index++]));
            turn = O_TURN;
        } else {
            printException(tekCreateOval(section.a, section.b, 2.f, 0, black, &noughts[nought_index++]));
            turn = X_TURN;
        }
    }
}

int render() {
    width = 640.0f;
    height = 480.0f;
    printException(tekInit("TekPhysics", width, height));
    tekSetMousePositionCallback(mousePosCallback);
    tekSetMouseButtonCallback(mouseButtonCallback);

    tekAddFramebufferCallback(tekMainFramebufferCallback);

    TekBitmapFont verdana;
    printException(tekCreateFreeType());

    printException(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &verdana));

    TekText tekgl_version;
    printException(tekCreateText("TekPhysics Alpha v1.0", 20, &verdana, &tekgl_version));

    TekText x_turn, o_turn, x_win, o_win;
    printException(tekCreateText("It's X's turn", 20, &verdana, &x_turn));
    printException(tekCreateText("It's O's turn", 20, &verdana, &o_turn));
    printException(tekCreateText("X has won!", 20, &verdana, &x_win));
    printException(tekCreateText("O has won!", 20, &verdana, &o_win));

    tekDeleteFreeType();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TekGuiLine grid[GRID_SIZE + GRID_SIZE - 2];

    vec2 a_ptr;
    vec2 b_ptr;

    const float section_width = (width - 2.f * PADDING_X) / (float)GRID_SIZE;
    const float section_height = (height - 2.f * PADDING_Y) / (float)GRID_SIZE;

    for (int x = 0; x < GRID_SIZE; x++) {
        a_ptr[0] = PADDING_X + (float)x * section_width;
        b_ptr[0] = a_ptr[0] + section_width;
        for (int y = 0; y < GRID_SIZE; y++) {
            a_ptr[1] = PADDING_Y + (float)y * section_height;
            b_ptr[1] = a_ptr[1] + section_height;

            glm_vec2_copy(a_ptr, grid_sections[y][x].a);
            glm_vec2_copy(b_ptr, grid_sections[y][x].b);
        }

        for (int i = 0; i < nought_index; i++) {
            printException(tekDrawOval(&noughts[i]));
        }

        for (int i = 0; i < cross_index; i++) {
            printException(drawCross(&crosses[i]));
        }
    }

    const vec4 grid_color = {0.0f, 0.0f, 0.0f, 1.0f};

    int grid_ptr = 0;
    for (int x = 1; x < GRID_SIZE; x++) {
        printException(tekCreateLine(grid_sections[0][x].a,
            grid_sections[GRID_SIZE - 1][x - 1].b,
            GRID_THICKNESS, grid_color, &grid[grid_ptr++]));
    }

    for (int y = 1; y < GRID_SIZE; y++) {
        printException(tekCreateLine(grid_sections[y][0].a,
            grid_sections[y - 1][GRID_SIZE - 1].b,
            GRID_THICKNESS, grid_color, &grid[grid_ptr++]));
    }

    Cross cross;
    vec2 a = {0.0f, 0.0f};
    vec2 b = {100.0f, 100.0f};
    printException(createCross(grid_sections[0][0].a, grid_sections[0][0].b, 2.f, black, &crosses[0]));

    const int num_lines = GRID_SIZE + GRID_SIZE - 2;

    const float text_x = grid_sections[GRID_SIZE - 1][0].a[0];
    const float text_y = grid_sections[GRID_SIZE - 1][0].b[1] + 10.0f;

    while (tekRunning()) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        for (int i = 0; i < num_lines; i++) {
            printException(tekDrawLine(&grid[i]));
        }

        printException(drawCross(&cross));

        printException(tekDrawText(&tekgl_version, width - 185.0f, 5.0f));
        printException(tekDrawText(&x_turn, text_x, text_y));
        tekUpdate();
    }

    tekDeleteTextEngine();
    tekDeleteText(&tekgl_version);
    tekDeleteText(&x_turn);
    tekDeleteText(&o_turn);
    tekDeleteText(&x_win);
    tekDeleteText(&o_win);
    for (int i = 0; i < num_lines; i++) {
        tekDeleteLine(&grid[i]);
    }
    tekDelete();
    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    if (render()) {
        tekPrintException();
    }
    tekCloseExceptions();
}