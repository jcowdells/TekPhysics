#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include "core/exception.h"
#include "tekgl/mesh.h"
#include "tekgl/shader.h"
#include "tekgl/texture.h"
#include "tekgl/font.h"

#define printException(x) if (x) tekPrintException()

int render() {
    printf("Hello World!\n");
    /* Initialize the library */
    if (!glfwInit())
        tekThrow(GLFW_EXCEPTION, "GLFW failed to initialise.")

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window) {
        glfwTerminate();
        tekThrow(GLFW_EXCEPTION, "GLFW failed to create window.")
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        tekThrow(GLAD_EXCEPTION, "GLAD failed to load loader.")
    }

    const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f
    };

    const unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    TekMesh mesh;
    if (tekCreateMesh(vertices, 20, indices, 6, &mesh)) {
        tekPrintException();
    }

    GLuint shader_program = 0;
    if (tekCreateShaderProgramVF("../shader/vertex.glvs", "../shader/fragment.glfs", &shader_program)) {
        tekPrintException();
    }
    glUseProgram(shader_program);



    TekGlyph glyphs[ATLAS_SIZE];

    printException(tekCreateFreeType());

    FT_Face font_face;
    printException(tekCreateFontFace("../res/verdana.ttf", 0, 20, &font_face));

    uint texture = 0;
    printException(tekCreateFontAtlasTexture(&font_face, &texture, glyphs));

    tekDeleteFontFace(font_face);
    tekDeleteFreeType();

    //if (tekCreateTexture("../res/xavier.png", &texture)) {
    //     tekPrintException();
    //}

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glViewport(0, 0, 640, 480);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window)) {
        /* Render here */
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);

        tekBindShaderProgram(shader_program);
        tekBindTexture(texture, 0);
        tekDrawMesh(&mesh);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    tekDeleteTexture(texture);
    tekDeleteMesh(&mesh);
    tekDeleteShaderProgram(shader_program);
    glfwTerminate();
    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    if (render()) {
        tekPrintException();
    }
    tekCloseExceptions();
}