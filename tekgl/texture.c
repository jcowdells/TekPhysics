#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#include "glad/glad.h"

/**
 * Create a new texture from an image file, and return the id of the newly created texture.
 * It is recommended to have square textures with side lengths that are powers of 2.
 * e.g. 128x128, 512x512.
 * @param filename The name of the file that should be loaded into a texture.
 * @param texture_id A pointer to a uint that will have the new texture id written to it.
 * @throws STBI_EXCEPTION if the image could not be loaded.
 */
exception tekCreateTexture(const char* filename, uint* texture_id) {
    // load texture using stbi
    stbi_set_flip_vertically_on_load(1);
    int width, height, num_channels;
    byte* image_data = stbi_load(filename, &width, &height, &num_channels, 4);

    // catch any potential errors
    if (!image_data) tekThrow(STBI_EXCEPTION, "STBI failed to load image.")

    // create an empty image with OpenGL
    glGenTextures(1, texture_id);
    glBindTexture(GL_TEXTURE_2D, *texture_id);

    // set texture parameters for wrapping and zooming
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // send image data to OpenGL and create mipmap (different levels of detail)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image_data);
    return SUCCESS;
}

/**
 * Bind a texture to a specified texture slot, allows the texture to be accessed in shaders.
 * @param texture_id The id of the texture to bind
 * @param texture_slot The slot (0, 1, 2, ... etc) to bind to.
 */
void tekBindTexture(const uint texture_id, const byte texture_slot) {
    // set texture slot to bind texture to, then bind it
    glActiveTexture(GL_TEXTURE0 + texture_slot);
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

/**
 * Delete a texture using its id using opengl.
 * @param texture_id The id of the texture to delete.
 */
void tekDeleteTexture(const uint texture_id) {
    glDeleteTextures(1, &texture_id); // expects an array of texture ids to delete
    // so just point to the id and say length = 1
}