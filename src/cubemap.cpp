#include "cubemap.h"
#include <framework/image.h>
#include <iostream>

bool CubemapTexture::load(const std::array<std::string,6>& faces) {
    if (!m_id) glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    for (int i = 0; i < 6; ++i) {
        Image img{ faces[i] }; // supports png/jpg via your framework
        GLenum fmt = (img.channels==4)? GL_RGBA : (img.channels==3)? GL_RGB : GL_RED;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt,
                     img.width, img.height, 0, fmt, GL_UNSIGNED_BYTE, img.get_data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return true;
}
