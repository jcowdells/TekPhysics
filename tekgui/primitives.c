#include "primitives.h"

#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/vec2.h>

#include "../tekgl/entity.h"
#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

mat4 projection;
uint line_shader_program = 0;
uint oval_shader_program = 0;
uint image_shader_program = 0;

/**
 * Framebuffer resize callback for primitives code. Called whenever window is resized.
 * @param window_width The new width of the window.
 * @param window_height The new height of the window.
 */
void tekPrimitiveFramebufferCallback(const int window_width, const int window_height) {
    // recreate projection matrix for shaders.
    glm_ortho(0.0f, (float)window_width, (float)window_height, 0.0f, -1.0f, 1.0f, projection);
}

/**
 * Create a simple rectangular mesh from a top left and bottom right position.
 * @param point_a The top left coordinate of the rectangle.
 * @param point_b The bottom right coordinate of the rectangle.
 * @param mesh The outputted rectangle mesh.
 * @throws OPENGL_EXCEPTION if could not create the mesh.
 */
static exception tekGuiCreateRectangularMesh(vec2 point_a, vec2 point_b, TekMesh* mesh) {
    // four vertices of the rectangle
    const float vertices[] = {
        point_a[0], point_a[1],
        point_a[0], point_b[1],
        point_b[0], point_b[1],
        point_b[0], point_a[1]
    };

    // draw order, to split into two triangles.
    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    // layout 1x vec2 per vertex
    const int layout[] = {
        2
    };

    // create the mesh
    tekChainThrow(tekCreateMesh(vertices, 8, indices, 6, layout, 1, mesh));
    return SUCCESS;
}

/**
 * Create the mesh needed to draw an image - requires there to be texture coordinates as well as positions.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param mesh The outputted mesh for the image.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekGuiCreateImageMesh(const float width, const float height, TekMesh* mesh) {
    // vertices with position + texture coordinate
    const float vertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, height, 0.0f, 0.0f,
        width, height, 1.0f, 0.0f,
        width, 0.0f, 1.0f, 1.0f
    };

    // indices / drawing order
    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    // layout - 2x vec2
    const int layout[] = {
        2, 2
    };

    // create the mesh
    tekChainThrow(tekCreateMesh(vertices, 16, indices, 6, layout, 2, mesh));
    return SUCCESS;
}

/**
 * Delete callback for primitives code, deletes shaders.
 */
static void tekDeletePrimitives() {
    // delete the shaders
    tekDeleteShaderProgram(line_shader_program);
    tekDeleteShaderProgram(oval_shader_program);
    tekDeleteShaderProgram(image_shader_program);
}

/**
 * The opengl load callback for the primitives code, sets up the shaders needed to draw primitive shapes.
 * @throws SHADER_EXCEPTION if could not create the shaders.
 */
static exception tekGLLoadPrimitives() {
    // create shaders
    tekChainThrow(tekCreateShaderProgramVF("../shader/line_vertex.glvs", "../shader/line_fragment.glfs", &line_shader_program));
    tekChainThrow(tekCreateShaderProgramVF("../shader/oval_vertex.glvs", "../shader/oval_fragment.glfs", &oval_shader_program));
    tekChainThrow(tekCreateShaderProgramVF("../shader/image_vertex.glvs", "../shader/image_fragment.glfs", &image_shader_program));

    // projection matrix stuff for the shaders
    int window_width, window_height;
    tekChainThrow(tekAddFramebufferCallback(tekPrimitiveFramebufferCallback));
    tekGetWindowSize(&window_width, &window_height);
    glm_ortho(0.0f, (float)window_width, 0.0f, (float)window_height, -1.0f, 1.0f, projection);
    return SUCCESS;
}

/**
 * Initialise the primitives code, setting up callbacks.
 */
tek_init tekInitPrimitives() {
    // set up opengl and delete callback.
    tekAddGLLoadFunc(tekGLLoadPrimitives);
    tekAddDeleteFunc(tekDeletePrimitives);
}

/**
 * Create a line that connects two points with a certain thickness.
 * @param point_a The first point of the line.
 * @param point_b The second point of the line.
 * @param thickness The thickness of the line in pixels.
 * @param color The colour to fill the line with.
 * @param line The outputted line.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekGuiCreateLine(vec2 point_a, vec2 point_b, const float thickness, vec4 color, TekGuiLine* line) {
    // find the direction that the line is pointing, and normalise it
    vec2 mod;
    glm_vec2_sub(point_b, point_a, mod);
    glm_vec2_normalize(mod);

    // get the perpendicular vector to the direction
    const float swap = mod[0];
    mod[0] = mod[1];
    mod[1] = -swap;

    // make vector be same length as half the thickness
    // adding on one side, subtracting on other side makes up whole thickness again
    mod[0] *= thickness;
    mod[1] *= thickness;

    // create mesh data for the line
    const float vertices[] = {
        point_a[0] - mod[0], point_a[1] - mod[1],
        point_a[0] + mod[0], point_a[1] + mod[1],
        point_b[0] + mod[0], point_b[1] + mod[1],
        point_b[0] - mod[0], point_b[1] - mod[1]
    };

    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    const int layout[] = {
        2
    };

    // create mesh and set color of line
    tekChainThrow(tekCreateMesh(vertices, 8, indices, 6, layout, 1, &line->mesh));
    glm_vec4_copy(color, line->color);

    return SUCCESS;
}

/**
 * Draw a line to the screen.
 * @param line The line to be drawn.
 * @throws SHADER_EXCEPTION .
 */
exception tekGuiDrawLine(const TekGuiLine* line) {
    // use line shader
    tekBindShaderProgram(line_shader_program);

    // set uniforms
    tekChainThrow(tekShaderUniformMat4(line_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformVec4(line_shader_program, "line_color", line->color));

    // draw
    tekDrawMesh(&line->mesh);
    return SUCCESS;
}

/**
 * Delete a line, freeing any associated memory.
 * @param line The line to delete.
 */
void tekGuiDeleteLine(const TekGuiLine* line) {
    // delete mesh
    tekDeleteMesh(&line->mesh);
}

/**
 * Create an oval based on two points, the oval will occupy the bounding box specified by the two points.
 * @param point_a The top left point of the bounding box.
 * @param point_b The bottom right point of the bounding box.
 * @param thickness The thickness of the line of the oval.
 * @param fill Should the oval be filled or not (1 or 0).
 * @param color The colour to use for the oval.
 * @param oval The outputted oval.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekGuiCreateOval(vec2 point_a, vec2 point_b, const float thickness, const flag fill, vec4 color, TekGuiOval* oval) {
    // generic rectangle mesh
    tekChainThrow(tekGuiCreateRectangularMesh(point_a, point_b, &oval->mesh));

    // some constants for this oval.
    oval->center[0] = (point_a[0] + point_b[0]) * 0.5f;
    oval->center[1] = (point_a[1] + point_b[1]) * 0.5f;

    oval->inv_width = 2.0f / (point_b[0] - point_a[0]);
    oval->inv_height = 2.0f / (point_b[1] - point_a[1]);

    // fill works by saying, if distance to edge is greaten than min_dist, colour in
    // if filled, this should always be true
    if (fill) {
        oval->min_dist = 0.0f;
    // otherwise, should only fill if near the edge
    } else {
        const float radius = (point_b[0] - point_a[0]) * 0.5f;
        const float fill_dist = radius - thickness;
        oval->min_dist = oval->inv_width * fill_dist;
    }

    // set up colour
    glm_vec4_copy(color, oval->color);

    return SUCCESS;
}

/**
 * Draw an oval to the screen.
 * @param oval The oval to draw.
 * @throws SHADER_EXCEPTION .
 */
exception tekGuiDrawOval(const TekGuiOval* oval) {
    // load shader
    tekBindShaderProgram(oval_shader_program);

    // set uniform
    // works using an oval equation hence all this maths
    tekChainThrow(tekShaderUniformMat4(oval_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "inv_width", oval->inv_width));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "inv_height", oval->inv_height));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "min_dist", oval->min_dist));
    tekChainThrow(tekShaderUniformVec2(oval_shader_program, "center", oval->center));
    tekChainThrow(tekShaderUniformVec4(oval_shader_program, "oval_color", oval->color));

    // draw
    tekDrawMesh(&oval->mesh);
    return SUCCESS;
}

/**
 * Delete an oval, freeing any allocated memory. Archaic function, was used in the noughts and crosses edition.
 * @param oval The oval to delete.
 */
void tekGuiDeleteOval(const TekGuiOval* oval) {
    // delete mesh
    tekDeleteMesh(&oval->mesh);
}

/**
 * Create an image from an image file, and create the mesh needed to draw it.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param texture_filename The name of the texture file.
 * @param image The outputted image.
 * @throws STBI_EXCEPTION if could not load the image.
 */
exception tekGuiCreateImage(const float width, const float height, const char* texture_filename, TekGuiImage* image) {
    // create the mesh and texture
    tekChainThrow(tekGuiCreateImageMesh(width, height, &image->mesh));
    tekChainThrowThen(tekCreateTexture(texture_filename, &image->texture_id), {
        tekDeleteMesh(&image->mesh);
    });

    return SUCCESS;
}

/**
 * Draw an image to the screen at the specified position.
 * @param image The image to be drawn.
 * @param x The x-coordinate to draw the image at.
 * @param y The y-coordinate to draw the image at.
 * @throws SHADER_EXCEPTION .
 */
exception tekGuiDrawImage(const TekGuiImage* image, const float x, const float y) {
    // load shader
    tekBindShaderProgram(image_shader_program);

    // set uniforms
    tekBindTexture(image->texture_id, 0);
    tekChainThrow(tekShaderUniformMat4(image_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformInt(image_shader_program, "texture_sampler", 0));
    tekChainThrow(tekShaderUniformVec2(image_shader_program, "start_position", (vec2){x, y}));

    // draw
    tekDrawMesh(&image->mesh);
    return SUCCESS;
}

/**
 * Delete the memory associated with a gui image.
 * @param image The image to delete.
 */
void tekGuiDeleteImage(const TekGuiImage* image) {
    // delete the mesh.
    tekDeleteMesh(&image->mesh);

    // delete the texture.
    tekDeleteTexture(image->texture_id);
}