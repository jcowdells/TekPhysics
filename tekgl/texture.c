#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#include "glad/glad.h"

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

void tekBindTexture(const uint texture_id, const byte texture_slot) {
    // set texture slot to bind texture to, then bind it
    glActiveTexture(GL_TEXTURE0 + texture_slot);
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

void tekDeleteTexture(const uint texture_id) {
    glDeleteTextures(1, &texture_id);
}