#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <framework/shader.h>

// Single cubic Bézier segment
struct CubicBezier {
    glm::vec3 p0, p1, p2, p3;

    glm::vec3 eval(float t) const {
        float u = 1.0f - t;
        float b0 = u*u*u;
        float b1 = 3.0f*u*u*t;
        float b2 = 3.0f*u*t*t;
        float b3 = t*t*t;
        return b0*p0 + b1*p1 + b2*p2 + b3*p3;
    }
    glm::vec3 tangent(float t) const {
        float u = 1.0f - t;
        return -3.0f*u*u*p0 + (3.0f*u*u - 6.0f*u*t)*p1 + (6.0f*u*t - 3.0f*t*t)*p2 + 3.0f*t*t*p3;
    }
};

class BezierPath {
public:
    BezierPath() = default;

    // Call once after GL is ready (e.g., in Application ctor), provide shader file paths
    void initGL(const char* lineVertPath, const char* lineFragPath);

    // Build a default closed loop of at least 3 segments (C0 closed, C1-ish continuity)
    void buildDefaultClosedLoop();

    // Or provide your own segments (must be >= 3)
    void setSegments(const std::vector<CubicBezier>& segs);

    // Rebuild the dense line-strip
    void rebuildLineVBO(int samplesPerSegment = 64);

    // Evaluate global parameter u in [0, segmentCount)
    glm::vec3 evalGlobal(float u) const;

    int  segmentCount() const { return (int)m_segments.size(); }

    // Draw the curve (if visible) into the given viewport+matrices
    struct Viewport { int x, y, w, h; };
    void drawCurve(const Viewport& vp, const glm::mat4& P, const glm::mat4& V, const glm::vec3& color);

    // Toggle visibility
    void setVisible(bool v) { m_visible = v; }
    bool isVisible() const { return m_visible; }

private:
    void ensureBuffers();

    std::vector<CubicBezier> m_segments;
    std::vector<glm::vec3>   m_lineVerts;
    int                      m_lineVertCount = 0;

    // GL
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    // Simple color line shader
    Shader m_lineShader;

    bool m_visible = true;
};
