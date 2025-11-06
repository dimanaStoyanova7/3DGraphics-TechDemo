#pragma once
#include <glad/glad.h>
#include <string>
#include <array>

class CubemapTexture {
public:
    CubemapTexture() = default;
    ~CubemapTexture() { if (m_id) glDeleteTextures(1, &m_id); }

    // order: +X -X +Y -Y +Z -Z
    bool load(const std::array<std::string,6>& faces);
    void bind(GLint unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    }
private:
    GLuint m_id = 0;
};
