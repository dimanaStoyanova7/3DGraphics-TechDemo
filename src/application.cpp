//#include "Image.h"
#include "mesh.h"
#include "texture.h"
#include "tile.h"
#include "BezierPath.h"
#include "cubemap.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <framework/trackball.h>
#include <functional>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <time.h>
#include <unordered_set>
#include <cstdint>
#include <random>

struct DynamicEnvCapture {
    GLuint cubemap = 0;
    GLuint fbo = 0;
    GLuint depthRbo = 0;
    int size = 512; // 256–1024 depending on perf

    void init(int s = 512) {
        size = s;
        // Cubemap
        glGenTextures(1, &cubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8,
                         size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        // FBO + depth RBO
        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
};

// +X, -X, +Y, -Y, +Z, -Z (dir, up)
static const struct { glm::vec3 dir, up; } kCubeViews[6] = {
    {{+1,0,0}, {0,-1,0}}, {{-1,0,0}, {0,-1,0}},
    {{0,+1,0}, {0, 0,1}}, {{0,-1,0}, {0, 0,-1}},
    {{0,0,+1}, {0,-1,0}}, {{0,0,-1}, {0,-1,0}},
};


struct RobotArm {
    // Index lists for each sub-part
    std::array<int, 13> base{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    std::array<int, 4>  first{ 13, 14, 15, 16 };
    std::array<int, 10> second{ 17, 18, 19, 20, 21, 22, 23, 24, 25, 26 };
    std::array<int, 5>  third{ 27, 28, 29, 30, 31 };
    std::array<int, 8>  hand{ 32, 33, 34, 35, 36, 37, 38, 39 };


    long indexOffset{ 0 };  // offset in your index buffer, if needed

    //not updated automaticly :( so index offset needs to be added when arm is made
    long startArm = 13 + indexOffset;
    long startSecondJoint = 17 + indexOffset;
    long starThirdJoint = 27 + indexOffset;
    long startHand = 32 + indexOffset;
    int size = 40;

    glm::mat4 firstTransformation;
    glm::mat4 secondTransformation;
    glm::mat4 thirdTransformation;
    glm::mat4 handRotation;

    glm::vec3 offsetFirst = glm::vec3(0.0, 0.7, 0.0);
    glm::vec3 offsetSecond = glm::vec3(0.0, 2.0, 1.2);
    glm::vec3 offsetThird = glm::vec3(0.0, 3.2, 0.2);
    glm::vec3 offsetFourth = glm::vec3(0.0, 2.9, -1.2);
    
    
    glm::vec3 rotationAxis = glm::vec3(1.0, 0.0, 0.0);
    glm::vec3 handAxis = glm::vec3(0.0, 0.0, 1.0);


    glm::vec3 origin{ glm::vec3(0.0, 0.0, 0.0) };
};


class Application {
public:
    Application()
    : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
    , m_trackball(&m_window, glm::radians(60.0f), /*dist*/ 3.0f, /*rotX*/ 0.2f, /*rotY*/ 0.8f)
        //, m_texture(RESOURCE_ROOT "resources/wall-e/Atlas_Metal.png")
    {

        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        //for (auto& g : m_mirrorMeshes) m_mirrorMeshes.emplace_back(std::move(g));

 // ---------------------------- Helper function to load Meshes and Textures ---------------------------
        loadMeshesandTextures();

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_GEOMETRY_SHADER, RESOURCE_ROOT "shaders/shader_geom.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            ShaderBuilder envBuilder;
            envBuilder.addStage(GL_VERTEX_SHADER,   RESOURCE_ROOT "shaders/env_vert.glsl");
            envBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/env_frag.glsl");
            m_envShader = envBuilder.build();
            
            // Init path renderer (line shader) and default closed loop
            m_bezierPath.initGL(RESOURCE_ROOT "shaders/line_vert.glsl",
                                RESOURCE_ROOT "shaders/line_frag.glsl");
            m_bezierPath.setVisible(m_showCurve);
            m_prevTime = glfwGetTime();
            start = clock();

            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....

            std::array<std::string,6> faces = {
                RESOURCE_ROOT "resources/envmap/posx.png",
                RESOURCE_ROOT "resources/envmap/negx.png",
                RESOURCE_ROOT "resources/envmap/posy.png",
                RESOURCE_ROOT "resources/envmap/negy.png",
                RESOURCE_ROOT "resources/envmap/posz.png",
                RESOURCE_ROOT "resources/envmap/negz.png"
            };
            m_envMap.load(faces); 
            m_dynamicEnv.init(512);

            glm::vec3 night = {0.10f, 0.18f, 0.40f};
            glm::vec3 dawn  = {1.00f, 0.55f, 0.25f};
            glm::vec3 noon  = {1.00f, 1.00f, 0.95f};
            glm::vec3 dusk  = {1.00f, 0.45f, 0.25f};

            auto seg = [&](glm::vec3 a, glm::vec3 b, glm::vec3 dirA, glm::vec3 dirB) {
                Bezier3D c;
                c.p0 = a;
                c.p1 = a + dirA;
                c.p2 = b - dirB;
                c.p3 = b;
                return c;
            };
            // gentle tangents
            m_dayColor = {
                seg(night, dawn, {0.00f,0.00f,0.00f}, {0.15f,0.10f,0.05f}),
                seg(dawn,  noon, {0.15f,0.10f,0.05f}, {0.10f,0.10f,0.10f}),
                seg(noon,  dusk, {0.10f,0.10f,0.10f}, {0.15f,0.08f,0.05f}),
                seg(dusk,  night,{0.15f,0.08f,0.05f}, {0.00f,0.00f,0.00f})
            };

            auto seg1 = [&](float a, float b, float da, float db) {
                Bezier1D c;
                c.p0 = a; c.p1 = a + da; c.p2 = b - db; c.p3 = b;
                return c;
            };
            // Intensities: night 0.10, dawn 0.60, noon 1.00, dusk 0.60, back to night
            m_dayIntensity = {
                seg1(0.10f, 0.60f, 0.00f, 0.10f),
                seg1(0.60f, 1.00f, 0.10f, 0.10f),
                seg1(1.00f, 0.60f, 0.10f, 0.10f),
                seg1(0.60f, 0.10f, 0.10f, 0.00f)
    };
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }


    // --- Camera modes ---
    enum class CamMode { BirdsEye = 0, Follow = 1, Trackball = 2 };
    CamMode m_camMode = CamMode::BirdsEye;
    
    // Follow-cam parameters (object-space offset that’s transformed by m_modelMatrix)
    glm::vec3 m_followOffsetOS { 0.0f, 1.5f, 5.0f }; // behind & slightly above

    // --- Multiple views ---
    struct Viewport { int x, y, w, h; };

    struct FreeCam {
        glm::vec3 pos { -1.5f, 1.0f, -1.5f };
        glm::vec3 fwd {  0.6f, -0.2f,  0.7f }; 
        glm::vec3 up  {  0.0f, 1.0f,  0.0f };
        double prevMouseX = 0.0, prevMouseY = 0.0;
        bool rotating = false;
    };

    FreeCam m_freeCam;

    // Bird’s-eye parameters
    float m_birdsEyeWorldHalfSize = 2.0f;  
    glm::vec3 m_birdsEyeCenter { 0.0f, 0.0f, 0.0f };
    float m_birdsEyeHeight = 5.0f;

    // ---- Lamp & path ----
    BezierPath m_bezierPath;
    bool   m_showCurve   = true;          // curve visibility toggle (mirrors BezierPath)
    bool   m_pauseLamp   = false;         // pause animation
    float  m_lampSpeed   = 0.15f;         // segments per second
    float  m_pathU       = 0.0f;          // global path parameter
    glm::vec3 m_lampPos  = {0.0f, 1.5f, 0.0f};
    glm::vec3 m_lampColor= {1.0f, 1.0f, 1.0f}; // bright, warm
    double m_prevTime    = 0.0;

    // ---- Light reach + intensity/exposure ----
    float m_lightRadius = 100.0f;          
    float m_baseIntensity = 2.5f;      
    float m_noonBoost     = 3.0f;     
    float m_nightBoost    = 3.0f;       
    float m_currentLightIntensity = 2.5f; 
    float m_exposure = 5.5f;

    // ---- Day/Night (Bezier) ----
    struct Bezier1D {
            float p0, p1, p2, p3;
            float eval(float t) const {
                float u = 1.0f - t;
                return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
            }
        };
    struct Bezier3D {
            glm::vec3 p0, p1, p2, p3;
            glm::vec3 eval(float t) const {
                float u = 1.0f - t;
                float b0 = u*u*u, b1 = 3.0f*u*u*t, b2 = 3.0f*u*t*t, b3 = t*t*t;
                return b0*p0 + b1*p1 + b2*p2 + b3*p3;
            }
    };

    float m_dayU        = 0.0f;     
    float m_daySpeed    = 1.0f/60.0f; 
    bool  m_pauseDay    = false;

    std::vector<Bezier3D> m_dayColor;
    std::vector<Bezier1D> m_dayIntensity;


    // env mapping feature
    std::vector<GPUMesh> m_mirrorMeshes;  // the dome mirror
    CubemapTexture m_envMap;
    Shader m_envShader;
    glm::mat4 m_mirrorModel {1.0f};
    DynamicEnvCapture m_dynamicEnv; 

    // tile streaming state 
    glm::vec3 m_worldOrigin { -5.0f, 0.0f, -5.0f };  
    float     m_tileWidth   = 10.0f;                 
    float     m_tileDepth   = 10.0f;                 
    glm::ivec2 m_currentTile { 0, 0 };               

    std::unordered_set<std::uint64_t> m_generatedTileKeys;

    static std::uint64_t tileKey(glm::ivec2 tc) {
        return ( (std::uint64_t)( (std::int64_t)tc.x & 0xffffffff ) << 32 )
            | ( (std::uint64_t)( (std::int64_t)tc.y & 0xffffffff )      );
    }
    glm::ivec2 worldToTileCoord(const glm::vec3& p) const {
        glm::vec3 rel = p - m_worldOrigin;
        int tx = (int)std::floor(rel.x / m_tileWidth);
        int tz = (int)std::floor(rel.z / m_tileDepth);
        return { tx, tz };
    }
    glm::vec3 tileStartWS(glm::ivec2 tc) const {
        return m_worldOrigin + glm::vec3(tc.x * m_tileWidth, 0.0f, tc.y * m_tileDepth);
    }
    glm::vec3 positionInTileWS(glm::ivec2 tc, float u, float v) const {
        glm::vec3 start = tileStartWS(tc);
        return start + glm::vec3(u * m_tileWidth, 0.0f, v * m_tileDepth);
    }

    void spawnTileAt(glm::ivec2 tc);

    void updateTileStreaming();

    // UI / control
    bool  m_activeFreeCam = true;          // which camera gets input
    
    float birdsEyeHalfSize = 2.0f;         // world half-extent visible in ortho
    float birdsEyeHeight   = 5.0f;         // camera height

    void update()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_SCISSOR_TEST);

        bool  splitVertical = true;            // left/right (vs top/bottom)
        bool  useMaterialUI = m_useMaterial;   // mirror your UI toggle
        
        glm::vec3 birdsEyeCenter(0.0f);

        // (optional) free-cam state – if you already manage this elsewhere, remove these
        static glm::vec3 fcPos(-1.5f, 1.0f, -1.5f);
        static glm::vec3 fcFwd(0.6f, -0.2f, 0.7f);
        static glm::vec3 fcUp (0.0f, 1.0f,  0.0f);

        auto freeCamView = [&] {
            return glm::lookAt(fcPos, fcPos + fcFwd, fcUp);
        };
        auto freeCamProj = [&](float aspect) {
            return glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        };

        struct Viewport { int x, y, w, h; };

        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();
            imgui();
            updateWallePosition();
            updateTileStreaming();

            // --- Lamp path advance ---
            double now = glfwGetTime();
            float dt = float(now - m_prevTime);
            m_prevTime = now;
            // ---- Day/Night advance ----
            if (!m_pauseDay) {
                m_dayU += m_daySpeed * dt; 
            }
            int segCount = (int)m_dayColor.size();
            float wrap = float(segCount);
            while (m_dayU >= wrap) m_dayU -= wrap;
            while (m_dayU < 0.0f)  m_dayU += wrap;

            int   s = (int)std::floor(m_dayU) % segCount;
            float t = m_dayU - std::floor(m_dayU);

            glm::vec3 dayCol = m_dayColor[s].eval(t);
            float     dayI   = m_dayIntensity[s].eval(t);
            float     dayIAdj = glm::clamp(dayI * 3.0f, 0.0f, 1.0f);
            float intensityScale = glm::mix(m_nightBoost, m_noonBoost, std::pow(dayIAdj, 0.6f));
            m_currentLightIntensity = m_baseIntensity * intensityScale;
            m_lampColor = dayCol * dayIAdj;
   


            if (!m_pauseLamp) {
                m_pathU += m_lampSpeed * dt; // segments per second
            }
            m_lampPos = m_bezierPath.evalGlobal(m_pathU);

            

            // Clear the screen (full-frame)
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Compute viewports
            const glm::ivec2 fb = m_window.getFrameBufferSize();
            glViewport(0, 0, fb.x, fb.y);
            glScissor (0, 0, fb.x, fb.y);

            // ── helpers that return P and V for the active camera mode ──
            auto getProj = [this](CamMode mode, float aspect) -> glm::mat4 {
                if (mode == CamMode::BirdsEye)
                    return birdsEyeProj(aspect);
                // Trackball & Follow use perspective
                return glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
            };

            auto getView = [this](CamMode mode) -> glm::mat4 {
                if (mode == CamMode::BirdsEye)
                    return birdsEyeView();
                if (mode == CamMode::Trackball)
                    return m_trackball.viewMatrix();

                // Follow: camera at object-space offset transformed to world, looking at object origin
                glm::vec3 objWorld = glm::vec3(m_walleMatrix * glm::vec4(0,0,0,1));
                glm::vec3 camWorld = glm::vec3(m_walleMatrix * glm::vec4(m_followOffsetOS, 1.0f));
                return glm::lookAt(camWorld, objWorld, glm::vec3(0,1,0));
            };


            // Build P & V for the currently selected mode
            const float aspect = fb.y ? float(fb.x) / float(fb.y) : 1.0f;
            const glm::mat4 P = getProj(m_camMode, aspect);
            const glm::mat4 V = getView(m_camMode);
            // updating the live env cubemap from the mirror's world position
            glm::vec3 mirrorPosWS = glm::vec3(m_mirrorModel[3]);
            updateDynamicEnv(mirrorPosWS);

            // Draw one view (with lamp lighting + texture cache)
            {
                m_defaultShader.bind();

                setCommonUniforms(m_defaultShader, P);

                for (GPUMesh& mesh : m_meshes) {
                    // Choose per-mesh model matrix
                    glm::mat4 M = mesh.getIsMovable() ? m_walleMatrix : m_modelMatrix; 
                    glm::mat4 MVP = P * V * M;
                    glm::mat3 NMM = glm::inverseTranspose(glm::mat3(M));

                    // Set per-mesh matrices
                    glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(MVP));
                    glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(M));
                    glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(NMM));                    

                    // Texture/material toggle (unchanged)
                    bool boundTexture = false;
                   
                    // Diffuse map
                    bindTextureIfAvailable(mesh.texturePath, m_defaultShader, "colorMap", GL_TEXTURE0, "hasTexCoords", boundTexture);

                    // Ambient map
                    bindTextureIfAvailable(mesh.ambientTexture, m_defaultShader, "ambientMap", GL_TEXTURE1, "hasAmbientTexture", boundTexture);

                    // Metalness map
                    bindTextureIfAvailable(mesh.metalnessTexture, m_defaultShader, "metalnessMap", GL_TEXTURE2, "hasMetalnessTexture", boundTexture);

                    // Roughness map
                    bindTextureIfAvailable(mesh.roughnessTexture, m_defaultShader, "roughnessMap", GL_TEXTURE3, "hasRoughnessTexture", boundTexture);

                    // Normal map
                    bindTextureIfAvailable(mesh.normalMap, m_defaultShader, "normalMap", GL_TEXTURE4, "hasNormalMap", boundTexture);

                    if (!boundTexture) {
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial ? GL_TRUE : GL_FALSE);
                    }
                    mesh.draw(m_defaultShader);
                    for (int i = 0; i < 8; ++i) {
                        glActiveTexture(GL_TEXTURE0 + i);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                }
            }

            drawMirror(P, V);

            drawRobbotArm(P, V);

            // Optional curve overlay (same P,V)
            m_bezierPath.drawCurve({0,0,fb.x,fb.y}, P, V, glm::vec3(0.9f, 0.2f, 0.1f));

            m_window.swapBuffers();
        }
    }

    //-----------------------Helper Functions use throughout the file ----------------------------------
    void onKeyPressed(int key, int mods) {
        //if (!m_activeFreeCam) return;

        const float move = 0.08f;
        glm::vec3 right = glm::normalize(glm::cross(m_freeCam.fwd, m_freeCam.up));
        if (key == GLFW_KEY_W) m_freeCam.pos += move * m_freeCam.fwd;
        if (key == GLFW_KEY_S) m_freeCam.pos -= move * m_freeCam.fwd;
        if (key == GLFW_KEY_A) m_freeCam.pos -= move * right;
        if (key == GLFW_KEY_D) m_freeCam.pos += move * right;
        if (key == GLFW_KEY_SPACE) m_freeCam.pos += move * m_freeCam.up;
        if (key == GLFW_KEY_C)     m_freeCam.pos -= move * m_freeCam.up;

        if (key == GLFW_KEY_UP)    m_moveFwd = true;
        if (key == GLFW_KEY_DOWN)  m_moveBack = true;
        if (key == GLFW_KEY_LEFT)  m_rotateLeft = true;
        if (key == GLFW_KEY_RIGHT) m_rotateRight = true;

        // Robot arm joint controls
        bool shiftHeld = (mods & GLFW_MOD_SHIFT);

        if (key == GLFW_KEY_1) {
            m_robotArmAngle1 += shiftHeld ? -m_ra_da : m_ra_da;
        }
        if (key == GLFW_KEY_2) {
            m_robotArmAngle2 += shiftHeld ? -m_ra_da : m_ra_da;
        }
        if (key == GLFW_KEY_3) {
            m_robotArmAngle3 += shiftHeld ? -m_ra_da : m_ra_da;
        }
        if (key == GLFW_KEY_4) {
            m_robotArmAngle4 += shiftHeld ? -m_ra_da : m_ra_da;
        }


        if (key == GLFW_KEY_L) { m_showCurve = !m_showCurve; m_bezierPath.setVisible(m_showCurve); }
        if (key == GLFW_KEY_P) { m_pauseLamp = !m_pauseLamp; }

        if (key == GLFW_KEY_TAB) m_activeFreeCam = !m_activeFreeCam;
    }

    bool trackballActive() const {
        return m_camMode == CamMode::Trackball;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;

        if (key == GLFW_KEY_UP)    m_moveFwd = false;
        if (key == GLFW_KEY_DOWN)  m_moveBack = false;
        if (key == GLFW_KEY_LEFT)  m_rotateLeft = false;
        if (key == GLFW_KEY_RIGHT) m_rotateRight = false;
    }

    void onMouseMove(const glm::dvec2& cursorPos) {
        if (trackballActive()) return; 
        if (!m_activeFreeCam || !m_freeCam.rotating) return;
        const float lookSpeed = 0.0015f;
        double dx = cursorPos.x - m_freeCam.prevMouseX;
        double dy = cursorPos.y - m_freeCam.prevMouseY;
        m_freeCam.prevMouseX = cursorPos.x; m_freeCam.prevMouseY = cursorPos.y;

        // yaw around world up:
        glm::mat4 yaw   = glm::rotate(glm::mat4(1.0f), float(-dx * lookSpeed), glm::vec3(0,1,0));
        // pitch around camera right:
        glm::vec3 right = glm::normalize(glm::cross(m_freeCam.fwd, m_freeCam.up));
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), float(-dy * lookSpeed), right);

        glm::vec3 dir = glm::vec3(pitch * yaw * glm::vec4(m_freeCam.fwd, 0.0f));
        m_freeCam.fwd  = glm::normalize(dir);
        m_freeCam.up   = glm::normalize(glm::cross(glm::cross(m_freeCam.fwd, glm::vec3(0,1,0)), m_freeCam.fwd));
    }


    void onMouseClicked(int button, int mods) {
        if (trackballActive()) return; 
        if (m_activeFreeCam && button == GLFW_MOUSE_BUTTON_LEFT) {
            m_freeCam.rotating = true;
            auto c = m_window.getCursorPos();
            m_freeCam.prevMouseX = c.x; m_freeCam.prevMouseY = c.y;
        }
    }

    void onMouseReleased(int button, int mods) {
        if (trackballActive()) return; 
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            m_freeCam.rotating = false;
    }

    void loadMeshesandTextures() {
        // last arguemnt isMovable indicates can the object be moved by key input
        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/wall-e/wall-e_scaled.obj", false, true);

        m_worldOrigin = glm::vec3(-5.0f, 0.0f, -5.0f);
        m_tileWidth   = 10.0f;
        m_tileDepth   = 10.0f;
        m_currentTile = { 0, 0 };
        m_generatedTileKeys.clear();

        spawnTileAt({0,0});

        // -- Example static object with speciffic postion generation ---
        glm::mat4 identity = glm::mat4(1.0);
        glm::vec3 armPos = positionInTileWS({ 0,0 }, 0.5f, 1.0f);
        identity = glm::translate(identity, armPos);
        //std::vector<GPUMesh> mm = GPUMesh::loadMeshGPU(identity, RESOURCE_ROOT "resources/car.obj");

        identity = glm::scale(glm::rotate(identity, glm::radians(90.0f), glm::vec3(1.0, 0.0, 0.0)), glm::vec3(4.0));
        m_meshes_robotArm  = GPUMesh::loadMeshGPU(identity, RESOURCE_ROOT "resources/robotArm/arm.obj");
        m_robotArm.indexOffset = m_meshes_robotArm[0].getMeshID();

        //update offsets
        m_robotArm.startArm += m_robotArm.indexOffset;
        m_robotArm.startSecondJoint += m_robotArm.indexOffset;
        m_robotArm.starThirdJoint += m_robotArm.indexOffset;
        m_robotArm.startHand += m_robotArm.indexOffset;
        m_robotArm.origin = armPos;
        
 
            
        // mirror obj
        glm::vec3 carPos   = positionInTileWS({0,0}, 0.5f, 1.0f);
        glm::vec3 sceneCtr = positionInTileWS({0,0}, 0.5f, 0.5f);
        glm::vec3 offsetFromCar = glm::vec3(2.0f, 0.1f, -2.7f);
        glm::vec3 desiredPos = carPos + offsetFromCar;

        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, desiredPos);
        M = glm::rotate(M, glm::radians(230.0f), glm::vec3(0, 1, 0)); // face –Z if needed
        M = glm::scale(M, glm::vec3(0.85f)); // adjust to your scene scale
        m_mirrorModel = M;

        m_mirrorMeshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/mirror/convex_mirror.obj");

        addTextures(m_meshes);
        addTextures(m_meshes_robotArm);

        
    }

    void setCommonUniforms(Shader& shader, const glm::mat4& P) {
        // Per-pass uniforms - g because used in geometry shader and then passed
        glUniform3fv(shader.getUniformLocation("glightPos"), 1, glm::value_ptr(m_lampPos));
        glUniform3fv(shader.getUniformLocation("gcolor"), 1, glm::value_ptr(m_lampColor));
        //replace with proper camera logic

        glm::vec3 camera_position = getCameraPosition();
        glUniform3fv(shader.getUniformLocation("gcamPos"), 1, glm::value_ptr(camera_position));

        // use prb and nm
        glUniform1i(shader.getUniformLocation("pbr"), m_pbr);
        glUniform1i(shader.getUniformLocation("nm"), m_normalMapping);

        glUniform1f(shader.getUniformLocation("glightRadius"), m_lightRadius);
        glUniform1f(shader.getUniformLocation("glightIntensity"), m_currentLightIntensity); 
        glUniform1f(shader.getUniformLocation("uExposure"),       m_exposure);


        if (m_normalMapping)glUniformMatrix4fv(m_defaultShader.getUniformLocation("gprojection"), 1, GL_FALSE, glm::value_ptr(P));
    }

    glm::vec3 getCameraPosition() {
        if (m_camMode == CamMode::BirdsEye){
            return glm::vec3(m_birdsEyeCenter.x, m_birdsEyeHeight, m_birdsEyeCenter.z);
        }
        else if (m_camMode == CamMode::Follow) {
            return m_freeCam.pos;
        }
        else {
            return m_trackball.position();
        }

    }

    void imgui() {
        // Use ImGui for easy input/output of ints, floats, strings, etc...

        

        ImGui::Begin("Views");

        ImGui::Text("Control Wall-e with arrows");
        ImGui::Text("Control robot arm with 1 2 3 4 and shift + 1 2 3 4");

        ImGui::Separator();

        ImGui::Checkbox("Use material if no texture", &m_useMaterial);
        ImGui::SliderFloat("BirdsEye half-size", &birdsEyeHalfSize, 0.5f, 10.0f); //don't update anything yet
        ImGui::SliderFloat("BirdsEye height", &birdsEyeHeight, 1.0f, 20.0f); //don't update anything yet

        ImGui::Checkbox("PBR", &m_pbr);
        ImGui::Checkbox("Normla mapping", &m_normalMapping);

        ImGui::Separator();
        ImGui::TextUnformatted("Lamp / Path");
        if (ImGui::Checkbox("Show Bézier curve", &m_showCurve)) {
            m_bezierPath.setVisible(m_showCurve);
        }
        ImGui::Checkbox("Pause lamp", &m_pauseLamp);
        ImGui::SliderFloat("Lamp speed (segments/s)", &m_lampSpeed, 0.0f, 1.0f);
        auto camModeCombo = [](const char* label, CamMode& mode) {
            int current = static_cast<int>(mode);
            const char* items[] = { "Birds-eye", "Follow", "Trackball" };
            if (ImGui::Combo(label, &current, items, IM_ARRAYSIZE(items))) {
                mode = static_cast<CamMode>(current);
            }
            };

        ImGui::Separator();
        ImGui::TextUnformatted("Camera");
        camModeCombo("Active view", m_camMode);
        ImGui::Separator();
        ImGui::TextUnformatted("Day/Night + Light");
        ImGui::SliderFloat("Light radius",     &m_lightRadius,     1.0f, 20.0f);
        ImGui::SliderFloat("Base intensity", &m_baseIntensity, 0.0f, 200.0f);
        ImGui::Text("Computed intensity: %.2f", m_currentLightIntensity);
        ImGui::Checkbox   ("Pause day/night",  &m_pauseDay);
        ImGui::SliderFloat("Day speed (segs/s)", &m_daySpeed, 0.0f, 2.0f);



        ImGui::End();
    }

    void drawRobbotArm(const glm::mat4& P, const glm::mat4& V) {


        m_defaultShader.bind();

        setCommonUniforms(m_defaultShader, P);

        for (GPUMesh& mesh : m_meshes_robotArm) {

            // Choose per-mesh model matrix
            glm::mat4 M{ 1.0f };
            if (mesh.getMeshID()  < m_robotArm.startArm){
                M = m_modelMatrix;
            }
            else if (mesh.getMeshID() >= m_robotArm.startArm && mesh.getMeshID()< m_robotArm.startSecondJoint) {
                M = firstTransformationRA(m_robotArmAngle1, m_robotArm);
            }
            else if (mesh.getMeshID() >= m_robotArm.startSecondJoint && mesh.getMeshID() < m_robotArm.starThirdJoint) {
                M = secondTransformationRA(m_robotArmAngle1, m_robotArmAngle2,  m_robotArm);
                //M = m_modelMatrix;
            }
            else if (mesh.getMeshID() >= m_robotArm.starThirdJoint && mesh.getMeshID() < m_robotArm.startHand) {
                M = thirdTransformationRA(m_robotArmAngle1, m_robotArmAngle2, m_robotArmAngle3, m_robotArm);
                //M = m_modelMatrix;
            }
            else {
                M = forthTransformationRA(m_robotArmAngle1, m_robotArmAngle2, m_robotArmAngle3, m_robotArmAngle4, m_robotArm);
                //M = m_modelMatrix;
            }
            glm::mat4 MVP = P * V * M;
            glm::mat3 NMM = glm::inverseTranspose(glm::mat3(M));

            // Set per-mesh matrices
            glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(MVP));
            glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(M));
            glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(NMM));

            // Texture/material toggle (unchanged)
            bool boundTexture = false;

            // Diffuse map
            bindTextureIfAvailable(mesh.texturePath, m_defaultShader, "colorMap", GL_TEXTURE0, "hasTexCoords", boundTexture);

            // Ambient map
            bindTextureIfAvailable(mesh.ambientTexture, m_defaultShader, "ambientMap", GL_TEXTURE1, "hasAmbientTexture", boundTexture);

            // Metalness map
            bindTextureIfAvailable(mesh.metalnessTexture, m_defaultShader, "metalnessMap", GL_TEXTURE2, "hasMetalnessTexture", boundTexture);

            // Roughness map
            bindTextureIfAvailable(mesh.roughnessTexture, m_defaultShader, "roughnessMap", GL_TEXTURE3, "hasRoughnessTexture", boundTexture);

            // Normal map
            bindTextureIfAvailable(mesh.normalMap, m_defaultShader, "normalMap", GL_TEXTURE4, "hasNormalMap", boundTexture);

            if (!boundTexture) {
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial ? GL_TRUE : GL_FALSE);
            }
            mesh.draw(m_defaultShader);
            for (int i = 0; i < 8; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

    }

    void drawMirror(const glm::mat4& P, const glm::mat4& V) {
        const glm::mat4& MM = m_mirrorModel;
        const glm::mat4 MVP = P * V * MM;
        const glm::mat3 NMM = glm::inverseTranspose(glm::mat3(MM));

        // camera position from inverse view
        glm::vec3 camPos = glm::vec3(glm::inverse(V)[3]);

        m_envShader.bind();
        glUniformMatrix4fv(m_envShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(MVP));
        glUniformMatrix4fv(m_envShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(MM));
        glUniformMatrix3fv(m_envShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(NMM));
        //glUniformMatrix4fv(m_envShader.getUniformLocation("viewMatrix"), 1, GL_FALSE, glm::value_ptr(V));
        glUniform3fv(m_envShader.getUniformLocation("cameraPos"), 1, glm::value_ptr(camPos));

        glUniform1f(m_envShader.getUniformLocation("fresnelStrength"), 0.65f);
        glUniform1f(m_envShader.getUniformLocation("roughness"), 0.05f); // nice and glossy


        // TODO - MAYBE REMOVE
        glDisable(GL_CULL_FACE);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_dynamicEnv.cubemap);
        glUniform1i(m_envShader.getUniformLocation("envMap"), 0);

        for (GPUMesh& mesh : m_mirrorMeshes)
            mesh.draw(m_envShader);
        // TODO - MAYBE REMOVE
        glEnable(GL_CULL_FACE);
    }
    void renderSceneNoMirror(const glm::mat4& P, const glm::mat4& V)
    {
        m_defaultShader.bind();
        setCommonUniforms(m_defaultShader, P);

        for (GPUMesh& mesh : m_meshes) {
            const glm::mat4 M   = mesh.getIsMovable() ? m_walleMatrix : m_modelMatrix;
            const glm::mat4 MVP = P * V * M;
            const glm::mat3 NMM = glm::inverseTranspose(glm::mat3(M));

            glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"),        1, GL_FALSE, glm::value_ptr(MVP));
            glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"),      1, GL_FALSE, glm::value_ptr(M));
            glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"),1, GL_FALSE, glm::value_ptr(NMM));

            bool boundTexture = false;
 

            // Diffuse map
            bindTextureIfAvailable(mesh.texturePath, m_defaultShader, "colorMap", GL_TEXTURE0, "hasTexCoords",  boundTexture);

            // Ambient map
            bindTextureIfAvailable(mesh.ambientTexture, m_defaultShader, "ambientMap", GL_TEXTURE1, "hasAmbientTexture", boundTexture);

            // Metalness map
            bindTextureIfAvailable(mesh.metalnessTexture, m_defaultShader, "metalnessMap", GL_TEXTURE2, "hasMetalnessTexture", boundTexture);

            // Roughness map
            bindTextureIfAvailable(mesh.roughnessTexture, m_defaultShader, "roughnessMap", GL_TEXTURE3, "hasRoughnessTexture", boundTexture);

            // Normal map
            bindTextureIfAvailable(mesh.normalMap, m_defaultShader, "normalMap", GL_TEXTURE4, "hasNormalMap", boundTexture);

            if (!boundTexture) {
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial ? GL_TRUE : GL_FALSE);
            }
            mesh.draw(m_defaultShader);
        }
    }

    void updateDynamicEnv(const glm::vec3& probePosWS)
    {
        const glm::ivec2 fb = m_window.getFrameBufferSize();
        GLint prevFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

        glBindFramebuffer(GL_FRAMEBUFFER, m_dynamicEnv.fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_dynamicEnv.depthRbo);

        glViewport(0, 0, m_dynamicEnv.size, m_dynamicEnv.size);
        glScissor(0, 0, m_dynamicEnv.size, m_dynamicEnv.size);

        const glm::mat4 P = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, 100.0f);

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                m_dynamicEnv.cubemap, 0);

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const auto& v = kCubeViews[face];
            glm::mat4 V = glm::lookAt(probePosWS, probePosWS + v.dir, v.up);

            // drawing the scene EXCEPT the mirror
            renderSceneNoMirror(P, V);
        }

        // building mip chain for roughness LOD
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_dynamicEnv.cubemap);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        // restoreingmain framebuffer + viewport
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        glViewport(0, 0, fb.x, fb.y);
        glScissor(0, 0, fb.x, fb.y);
    }


    glm::mat4 birdsEyeView() const {
        return glm::lookAt(glm::vec3(m_birdsEyeCenter.x, m_birdsEyeHeight, m_birdsEyeCenter.z),
            m_birdsEyeCenter, glm::vec3(0, 0, -1));
    }

    glm::mat4 birdsEyeProj(float aspect) const {
        const float sx = m_birdsEyeWorldHalfSize;
        const float sy = m_birdsEyeWorldHalfSize / aspect;
        return glm::ortho(-sx, +sx, -sy, +sy, 0.01f, 100.0f);
    }

    glm::mat4 freeCamView() const {
        return glm::lookAt(m_freeCam.pos, m_freeCam.pos + m_freeCam.fwd, m_freeCam.up);
    }

    glm::mat4 freeCamProj(float aspect) const {
        return glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
    }

    void updateWallePosition()
    {
        //glm::vec3 fwdWS = glm::normalize(glm::vec3(m_walleMatrix * glm::vec4(0,0,-1,0)));
        //fwdWS.y = 0.0f;
        //if (glm::dot(fwdWS, fwdWS) > 0.0f) fwdWS = glm::normalize(fwdWS);

        glm::vec3 moveDir(0.0f);
        //if (m_moveFwd)  moveDir += fwdWS * m_moveSpeed;
        //if (m_moveBack) moveDir -= fwdWS * m_moveSpeed;

        if (m_moveFwd)  moveDir += fwd;
        if (m_moveBack) moveDir -= fwd;

        glm::vec3 currPos = glm::vec3(m_walleMatrix[3]);
        glm::vec3 nextPos = currPos + moveDir;
        if (m_rotateLeft)
            m_walleMatrix = glm::rotate(m_walleMatrix, glm::radians(m_rotationSpeed), glm::vec3(0, 1, 0));
        if (m_rotateRight)
            m_walleMatrix = glm::rotate(m_walleMatrix, -glm::radians(m_rotationSpeed), glm::vec3(0, 1, 0));
        glm::vec3 delta = nextPos - currPos;
        m_walleMatrix = glm::translate(m_walleMatrix, delta);
        glm::vec3 posWS = glm::vec3(m_walleMatrix[3]);
        depenetrateXZ(posWS);
        m_walleMatrix[3] = glm::vec4(posWS, 1.0f);

        //m_walleMatrix = glm::rotate(m_walleMatrix, side * glm::radians(m_rotationSpeed), fwd);
        //if (clock() - start > duration) {
          //  start = clock();
            //side *= -1;
        //}

    }

    void bindTextureIfAvailable(
        const std::string& texturePath,
        Shader& shader,
        const std::string& uniformName,
        GLenum textureUnit,
        const std::string& hasTextureName,
        bool& boundFlag)
    {
        if (texturePath.empty())
            return;

        auto it = textureCache.find(texturePath);
        if (it != textureCache.end()) {
            it->second.bind(textureUnit);
            glUniform1i(shader.getUniformLocation(uniformName), textureUnit - GL_TEXTURE0);
            glUniform1i(m_defaultShader.getUniformLocation(hasTextureName), GL_TRUE);
            glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
            boundFlag = true;
        }
    }

    void addTextures(std::vector<GPUMesh>& meshes){
        // --- Create Textures ---
        for (GPUMesh& mesh : meshes) {

            // Diffuse / color map
            if (mesh.hasTextureCoords() && !mesh.texturePath.empty()) {
                const std::string path = mesh.texturePath;
                if (textureCache.find(path) == textureCache.end()) {
                    std::cout << "Loading unique diffuse texture: " << path
                        << " (" << mesh.m_numIndices << " indices)" << std::endl;
                    textureCache.emplace(path, Texture(path));
                }
            }

            // Ambient map
            if (!mesh.ambientTexture.empty()) {
                const std::string path = mesh.ambientTexture;
                if (textureCache.find(path) == textureCache.end()) {
                    std::cout << "Loading unique ambient texture: " << path
                        << " (" << mesh.m_numIndices << " indices)" << std::endl;
                    textureCache.emplace(path, Texture(path));
                }
            }

            // Metalness map
            if (!mesh.metalnessTexture.empty()) {
                const std::string path = mesh.metalnessTexture;
                if (textureCache.find(path) == textureCache.end()) {
                    std::cout << "Loading unique metalness texture: " << path
                        << " (" << mesh.m_numIndices << " indices)" << std::endl;
                    textureCache.emplace(path, Texture(path));
                }
            }

            // Roughness map
            if (!mesh.roughnessTexture.empty()) {
                const std::string path = mesh.roughnessTexture;
                if (textureCache.find(path) == textureCache.end()) {
                    std::cout << "Loading unique roughness texture: " << path
                        << " (" << mesh.m_numIndices << " indices)" << std::endl;
                    textureCache.emplace(path, Texture(path));
                }
            }

            // Normal map
            if (!mesh.normalMap.empty()) {
                const std::string path = mesh.normalMap;
                if (textureCache.find(path) == textureCache.end()) {
                    std::cout << "Loading unique normal map: " << path
                        << " (" << mesh.m_numIndices << " indices)" << std::endl;
                    textureCache.emplace(path, Texture(path));
                }
            }
        }
    }

    glm::mat4 firstTransformationRA(float angle1, RobotArm arm) {
        // 1. Move to Pivot 1 (P1)
        glm::mat4 M = glm::translate(glm::mat4(1.0f), arm.origin + arm.offsetFirst);

        // 2. Rotate 1
        M = glm::rotate(M, angle1, arm.rotationAxis);

        // 3. Move back from P1
        M = glm::translate(M, -arm.origin - arm.offsetFirst);

        return M;
    }

    glm::mat4 secondTransformationRA(float angle1, float angle2, RobotArm arm) {
        // 1. Move to Pivot 1 (P1)
        glm::mat4 M = glm::translate(glm::mat4(1.0f), arm.origin + arm.offsetFirst);

        // 2. Rotate 1
        M = glm::rotate(M, angle1, arm.rotationAxis);

        // 3. Translate from P1 to P2
        M = glm::translate(M, arm.offsetSecond - arm.offsetFirst);

        // 4. Rotate 2
        M = glm::rotate(M, angle2, arm.rotationAxis);

        // 5. Move back from Pivot 2 (P2)
        M = glm::translate(M, -arm.origin - arm.offsetSecond);

        return M;
    }

    glm::mat4 thirdTransformationRA(float angle1, float angle2, float angle3, RobotArm arm) {
        // 1. Move to Pivot 1 (P1)
        glm::mat4 M = glm::translate(glm::mat4(1.0f), arm.origin + arm.offsetFirst);

        // 2. Rotate 1
        M = glm::rotate(M, angle1, arm.rotationAxis);

        // 3. Translate from P1 to P2
        M = glm::translate(M, arm.offsetSecond - arm.offsetFirst);

        // 4. Rotate 2
        M = glm::rotate(M, angle2, arm.rotationAxis);

        // 5. Translate from P2 to P3 (Correction applied here: connects P2 and P3)
        M = glm::translate(M, arm.offsetThird - arm.offsetSecond);

        // 6. Rotate 3
        M = glm::rotate(M, angle3, arm.rotationAxis);

        // 7. Move back from Pivot 3 (P3)
        M = glm::translate(M, -arm.origin - arm.offsetThird);

        return M;
    }

    glm::mat4 forthTransformationRA(float angle1, float angle2, float angle3, float angle4, RobotArm arm) {
        // 1. Move to Pivot 1 (P1)
        glm::mat4 M = glm::translate(glm::mat4(1.0f), arm.origin + arm.offsetFirst);

        // 2. Rotate 1
        M = glm::rotate(M, angle1, arm.rotationAxis);

        // 3. Translate from P1 to P2
        M = glm::translate(M, arm.offsetSecond - arm.offsetFirst);

        // 4. Rotate 2
        M = glm::rotate(M, angle2, arm.rotationAxis);

        // 5. Translate from P2 to P3
        M = glm::translate(M, arm.offsetThird - arm.offsetSecond);

        // 6. Rotate 3
        M = glm::rotate(M, angle3, arm.rotationAxis);

        // 7. Translate from P3 to P4
        M = glm::translate(M, arm.offsetFourth - arm.offsetThird);

        // 8. Rotate 4
        M = glm::rotate(M, angle4, arm.handAxis);

        // 9. Move back from Pivot 4 (P4)
        M = glm::translate(M, -arm.origin - arm.offsetFourth);

        return M;
    }



private:
    Window m_window;
    // Trackball camera (debug cam)
    Trackball m_trackball;


    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;

    std::vector<GPUMesh> m_meshes;
    std::map<std::string, Texture> textureCache;
	Texture m_texture;
    bool m_useMaterial { true };
	//bool m_useTrackBall{ false };

    //Trackball m_trackball{ &m_window, glm::radians(80.0f) };
    // Projection and view matrices for you to fill in and use
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-6, 6, 1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };
    glm::mat4 m_walleMatrix{ 1.0f };

    RobotArm m_robotArm;
    std::vector<GPUMesh> m_meshes_robotArm;

    bool m_pbr = false;
    bool m_normalMapping = false;
    bool m_moveFwd = false;
    bool m_moveBack = false;
    bool m_rotateLeft = false;
    bool m_rotateRight = false;
    float m_moveSpeed = 0.01f;
    float m_rotationSpeed = 0.5f;

    float m_robotArmAngle1{ 0.0f };
    float m_robotArmAngle2{ 0.0f };
    float m_robotArmAngle3{ 0.0f };
    float m_robotArmAngle4{ 0.0f };

    float m_ra_da = 0.05;

    int side = -1;
    clock_t start{};   
    double duration = CLOCKS_PER_SEC * 0.2;

    glm::vec3 fwd = glm::vec3(m_walleMatrix * glm::vec4(1, 0, 0, 0));

    struct Obstacle {
        glm::ivec2 tile;   // which tile it belongs to
        glm::vec3  posWS;  // world position (center on ground)
        float      radius; // for 2D circle collision
    };

    std::vector<Obstacle> m_obstacles;

    std::mt19937 m_rng { std::random_device{}() };

    const std::string m_propBarrier = RESOURCE_ROOT "resources/props/road_block_a/road_block_a.obj";

    // helpers
    void maybeSpawnObstacle(glm::ivec2 tc);
    static float distXZ(const glm::vec3& a, const glm::vec3& b) {
        glm::vec2 da(a.x - b.x, a.z - b.z);
        return glm::length(da);
    }

    void depenetrateXZ(glm::vec3& posWS) {
        const float walleRadius = 0.45f;     
        const float skin        = 1e-3f;    
        for (int iter = 0; iter < 4; ++iter) {
            bool corrected = false;

            for (const auto& obs : m_obstacles) {
                glm::vec2 p(posWS.x - obs.posWS.x, posWS.z - obs.posWS.z);
                float d2 = glm::dot(p, p);
                float minR = walleRadius + obs.radius;
                float minR2 = minR * minR;

                if (d2 < minR2) {
                    float d = std::sqrt(std::max(d2, 1e-8f));
                    glm::vec2 n = (d > 1e-6f) ? (p / d) : glm::vec2(1.0f, 0.0f); // fallback normal
                    float push = (minR - d) + skin;

                    // push out along normal
                    posWS.x += n.x * push;
                    posWS.z += n.y * push;
                    corrected = true;
                }
            }
            if (!corrected) break;
        }
    }

    
};
void Application::spawnTileAt(glm::ivec2 tc)
{
    const std::uint64_t key = tileKey(tc);
    if (m_generatedTileKeys.find(key) != m_generatedTileKeys.end())
        return; // already exists

    // Build a CPU tile from bounds and push its GPUMesh into m_meshes
    glm::vec3 start = tileStartWS(tc);
    glm::vec3 end   = start + glm::vec3(m_tileWidth, 0.0f, m_tileDepth);

    Tile t(start, end);
    m_meshes.emplace_back(GPUMesh(t.generateMesh()));

    m_generatedTileKeys.insert(key);
    maybeSpawnObstacle(tc);

}

void Application::updateTileStreaming()
{
    glm::vec3 wallePos = glm::vec3(m_walleMatrix[3]);
    glm::ivec2 tc = worldToTileCoord(wallePos);

    spawnTileAt(tc);

    if (tc != m_currentTile) {
        m_currentTile = tc;
    }
}

void Application::maybeSpawnObstacle(glm::ivec2 tc)
{
    if (tc == glm::ivec2(0, 0)) return;

    std::uniform_real_distribution<float> p01(0.0f, 1.0f);
    if (p01(m_rng) > 0.5f) return;

    static const glm::vec2 slots[9] = {
        {0.50f,0.50f},
        {0.15f,0.15f}, {0.85f,0.15f}, {0.15f,0.85f}, {0.85f,0.85f},
        {0.50f,0.15f}, {0.50f,0.85f}, {0.15f,0.50f}, {0.85f,0.50f}
    };
    std::uniform_int_distribution<int> slotPick(0, 8);
    glm::vec2 uv = slots[slotPick(m_rng)];

    glm::vec3 posWS = positionInTileWS(tc, uv.x, uv.y);

    const float tileY = tileStartWS(tc).y;
    const float barrierHalfHeight = 0.95f; 
    posWS.y = tileY + barrierHalfHeight;

    std::uniform_real_distribution<float> yawDeg(0.0f, 360.0f);
    float yaw = glm::radians(yawDeg(m_rng));

    const std::string file = m_propBarrier;
    const glm::vec3   scale(1.0f);
    // --- visual mesh ---
    glm::mat4 M(1.0f);
    M = glm::translate(M, posWS);
    M = glm::rotate(M, yaw, glm::vec3(0,1,0));
    M = glm::scale(M, scale);
    auto propMeshes = GPUMesh::loadMeshGPU(M, file);
    for (auto& g : propMeshes) m_meshes.emplace_back(std::move(g));

    const float halfLen   = 3.00f; // half of barrier length along its long axis
    const float halfWidth = 0.95f; // ~half its width; this is the circle radius

    glm::vec3 dirF = glm::normalize(glm::vec3(std::cos(yaw), 0.0f, std::sin(yaw)));

    glm::vec3 endA = posWS - dirF * halfLen;
    glm::vec3 endB = posWS + dirF * halfLen;

    // register colliders (XZ only; y is ignored elsewhere)
    m_obstacles.push_back(Obstacle{ tc, endA, halfWidth });
    m_obstacles.push_back(Obstacle{ tc, posWS, halfWidth }); 
    m_obstacles.push_back(Obstacle{ tc, endB, halfWidth });
}



int main()
{
    Application app;
    app.update();

    return 0;
}
