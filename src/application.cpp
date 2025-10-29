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
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }


    // --- Camera modes ---
    enum class CamMode { BirdsEye = 0, Follow = 1, Trackball = 2 };
    CamMode m_camMode = CamMode::BirdsEye;
    
    // Follow-cam parameters (object-space offset that’s transformed by m_modelMatrix)
    glm::vec3 m_followOffsetOS { 0.0f, 0.8f, 2.0f }; // behind & slightly above

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

    // env mapping feature
    std::vector<GPUMesh> m_mirrorMeshes;  // the dome mirror
    CubemapTexture m_envMap;
    Shader m_envShader;
    glm::mat4 m_mirrorModel {1.0f};
    DynamicEnvCapture m_dynamicEnv; 

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

            // --- Lamp path advance ---
            double now = glfwGetTime();
            float dt = float(now - m_prevTime);
            m_prevTime = now;

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
                glm::vec3 objWorld = glm::vec3(m_modelMatrix * glm::vec4(0,0,0,1));
                glm::vec3 camWorld = glm::vec3(m_modelMatrix * glm::vec4(m_followOffsetOS, 1.0f));
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

                // Per-pass uniforms
                glUniform3fv(m_defaultShader.getUniformLocation("lightPos"), 1, glm::value_ptr(m_lampPos));
                glUniform3fv(m_defaultShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_lampColor));

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
                    if (!mesh.texturePath.empty()) {
                        auto it = textureCache.find(mesh.texturePath);
                        if (it != textureCache.end()) {
                            it->second.bind(GL_TEXTURE0);
                            glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                            glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                            glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                            boundTexture = true;
                        }
                    }
                    if (!boundTexture) {
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial ? GL_TRUE : GL_FALSE);
                    }

                    mesh.draw(m_defaultShader);
                }
            }

            drawMirror(P, V);

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

        // --- Example Tile Generation ---
        glm::vec3 startPoint(-5.0f, 0.0f, -5.0f);
        glm::vec3 endPoint(5.0f, 0.0f, 5.0f);
        Tile tile(startPoint, endPoint);
        m_meshes.push_back(GPUMesh(tile.generateMesh()));

        // -- Example static object with speciffic postion generation ---
        glm::mat4 identity = glm::mat4(1.0);
        identity = glm::translate(identity, tile.positionInTile(0.5, 1.0));
        std::vector<GPUMesh> mm = GPUMesh::loadMeshGPU(identity, RESOURCE_ROOT "resources/car.obj");

        for (GPUMesh& gpumesh : mm) {
            m_meshes.emplace_back(std::move(gpumesh));
        }





        // mirror obj
        glm::vec3 carPos = tile.positionInTile(0.5f, 1.0f);
        glm::vec3 sceneCtr = tile.positionInTile(0.5f, 0.5f);
        glm::vec3 offsetFromCar = glm::vec3(2.0f, 0.1f, -2.7f);
        glm::vec3 desiredPos = carPos + offsetFromCar;

        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, desiredPos);
        M = glm::rotate(M, glm::radians(230.0f), glm::vec3(0, 1, 0)); // face –Z if needed
        M = glm::scale(M, glm::vec3(0.85f)); // adjust to your scene scale
        m_mirrorModel = M;

        m_mirrorMeshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/mirror/convex_mirror.obj");

        // --- Create Textures ---
        for (GPUMesh& mesh : m_meshes) {
            if (mesh.hasTextureCoords() && !mesh.texturePath.empty()) {
                const std::string path = mesh.texturePath;

                // 1. Check if texture is already in cache
                if (textureCache.find(path) == textureCache.end()) {
                    // 2. Not found: Load it and insert into cache
                    std::cout << "Loading unique texture: " << path << mesh.m_numIndices << std::endl;
                    textureCache.emplace(path, Texture(path));
                }

            }
        }

    }

    void imgui() {
        // Use ImGui for easy input/output of ints, floats, strings, etc...
        ImGui::Begin("Views");
        ImGui::Checkbox("Use material if no texture", &m_useMaterial);
        ImGui::SliderFloat("BirdsEye half-size", &birdsEyeHalfSize, 0.5f, 10.0f); //don't update anything yet
        ImGui::SliderFloat("BirdsEye height", &birdsEyeHeight, 1.0f, 20.0f); //don't update anything yet

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


        ImGui::End();
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
        glUniformMatrix4fv(m_envShader.getUniformLocation("viewMatrix"), 1, GL_FALSE, glm::value_ptr(V));
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
        const glm::mat4& M = m_modelMatrix;
        const glm::mat4  MVP = P * V * M;
        const glm::mat3  NMM = glm::inverseTranspose(glm::mat3(M));

        m_defaultShader.bind();
        glUniform3fv(m_defaultShader.getUniformLocation("lightPos"), 1, glm::value_ptr(m_lampPos));
        glUniform3fv(m_defaultShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_lampColor));
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(MVP));
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(M));
        glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(NMM));

        for (GPUMesh& mesh : m_meshes) {
            bool boundTexture = false;
            if (!mesh.texturePath.empty()) {
                auto it = textureCache.find(mesh.texturePath);
                if (it != textureCache.end()) {
                    it->second.bind(GL_TEXTURE0);
                    glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                    boundTexture = true;
                }
            }
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
        

        glm::vec3 moveDir(0.0f);

        if (m_moveFwd)  moveDir += fwd;
        if (m_moveBack) moveDir -= fwd;

        // Normalize movement
        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir) * m_moveSpeed;
            m_walleMatrix = glm::translate(m_walleMatrix, moveDir);
        }

        // --- Rotation ---
        // Rotate around the Y-axis (up axis)
        if (m_rotateLeft)
            m_walleMatrix = glm::rotate(m_walleMatrix, glm::radians(m_rotationSpeed), glm::vec3(0, 1, 0));
        if (m_rotateRight)
            m_walleMatrix = glm::rotate(m_walleMatrix, -glm::radians(m_rotationSpeed), glm::vec3(0, 1, 0));

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

    bool m_moveFwd = false;
    bool m_moveBack = false;
    bool m_rotateLeft = false;
    bool m_rotateRight = false;
    float m_moveSpeed = 0.1f;
    float m_rotationSpeed = 0.5f;

    glm::vec3 fwd = glm::vec3(m_walleMatrix * glm::vec4(1, 0, 0, 0));
    
};

int main()
{
    Application app;
    app.update();

    return 0;
}
