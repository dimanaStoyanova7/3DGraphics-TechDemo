#include "BezierPath.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <framework/shader.h>
#include <algorithm>
#include <cmath>

void BezierPath::initGL(const char* lineVertPath, const char* lineFragPath) {
    ShaderBuilder lb;
    lb.addStage(GL_VERTEX_SHADER,   lineVertPath);
    lb.addStage(GL_FRAGMENT_SHADER, lineFragPath);
    m_lineShader = lb.build();
    ensureBuffers();
    if (m_segments.empty()) buildDefaultClosedLoop();
    rebuildLineVBO(64);
}

void BezierPath::ensureBuffers() {
    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
}

void BezierPath::buildDefaultClosedLoop() {
    m_segments.clear();

    // Segment 0
    {
        CubicBezier c;
        c.p0 = { -2.0f, 1.2f, -2.0f };
        c.p1 = { -1.0f, 2.0f, -0.5f };
        c.p2 = {  0.0f, 2.2f,  0.5f };
        c.p3 = {  1.0f, 1.3f,  1.2f };
        m_segments.push_back(c);
    }
    // Segment 1 (mirror p1 for C1-ish continuity)
    {
        const auto& prev = m_segments.back();
        CubicBezier c;
        c.p0 = prev.p3;
        c.p1 = prev.p3 + (prev.p3 - prev.p2);
        c.p2 = {  1.5f, 1.8f, -0.8f };
        c.p3 = {  0.0f, 1.4f, -1.5f };
        m_segments.push_back(c);
    }
    // Segment 2
    {
        const auto& prev = m_segments.back();
        CubicBezier c;
        c.p0 = prev.p3;
        c.p1 = prev.p3 + (prev.p3 - prev.p2);
        c.p2 = { -1.8f, 2.1f, -0.6f };
        c.p3 = { -2.0f, 1.2f, -2.0f }; // close loop
        m_segments.push_back(c);
    }
}

void BezierPath::setSegments(const std::vector<CubicBezier>& segs) {
    m_segments = segs;
}

void BezierPath::rebuildLineVBO(int samplesPerSegment) {
    ensureBuffers();
    m_lineVerts.clear();
    if (m_segments.empty()) return;

    for (size_t s = 0; s < m_segments.size(); ++s) {
        for (int i = 0; i <= samplesPerSegment; ++i) {
            float t = float(i) / float(samplesPerSegment);
            m_lineVerts.push_back(m_segments[s].eval(t));
        }
    }
    m_lineVertCount = (int)m_lineVerts.size();

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_lineVerts.size() * sizeof(glm::vec3), m_lineVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

glm::vec3 BezierPath::evalGlobal(float u) const {
    if (m_segments.empty()) return glm::vec3(0.0f);
    int segCount = (int)m_segments.size();
    // wrap u
    float wrap = (float)segCount;
    float uf = u;
    // manual wrap to handle negatives too
    while (uf >= wrap) uf -= wrap;
    while (uf < 0.0f)  uf += wrap;

    int   seg = (int)std::floor(uf) % segCount;
    float t   = uf - std::floor(uf);
    return m_segments[seg].eval(t);
}

void BezierPath::drawCurve(const Viewport& vp, const glm::mat4& P, const glm::mat4& V, const glm::vec3& color) {
    if (!m_visible || m_lineVertCount == 0) return;

    glViewport(vp.x, vp.y, vp.w, vp.h);
    glScissor (vp.x, vp.y, vp.w, vp.h);

    glm::mat4 M = glm::mat4(1.0f);
    glm::mat4 MVP = P * V * M;

    m_lineShader.bind();
    glUniformMatrix4fv(m_lineShader.getUniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform3fv(m_lineShader.getUniformLocation("color"), 1, glm::value_ptr(color));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINE_STRIP, 0, m_lineVertCount);
    glBindVertexArray(0);
}
