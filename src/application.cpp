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
        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/wall-e/wall-e_scaled.obj");
        
        // --- Example Tile Generation ---
        glm::vec3 startPoint(-5.0f, 0.0f, -5.0f);
        glm::vec3 endPoint(5.0f, 0.0f, 5.0f);
        Tile tile(startPoint, endPoint);
        m_meshes.push_back(GPUMesh(tile.generateMesh()));
        
        // -- Example static object with speciffic postion generation ---
        glm::mat4 identity = glm::mat4(1.0);
        //identity = glm::translate(identity, startPoint);
        //identity = glm::rotate(identity, glm::radians(60.f), glm::vec3(1.0, 0.0, 0.0));
        identity = glm::translate(identity, tile.positionInTile(0.5, 1.0));
        std::vector<GPUMesh> mm = GPUMesh::loadMeshGPU(identity, RESOURCE_ROOT "resources/car.obj"); 

        for (GPUMesh& gpumesh : mm) {
            m_meshes.emplace_back(std::move(gpumesh));
        }
        // mirror obj
        glm::vec3 carPos = tile.positionInTile(0.5f, 1.0f);
        glm::vec3 sceneCtr = tile.positionInTile(0.5f, 0.5f);
        glm::vec3 offsetFromCar = glm::vec3(0.2f, 0.1f, -0.1f);
        glm::vec3 desiredPos    = carPos + offsetFromCar;

        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, desiredPos);
        M = glm::rotate(M, glm::radians(110.0f), glm::vec3(0,1,0)); // face –Z if needed
        M = glm::scale(M, glm::vec3(0.85f)); // adjust to your scene scale
        m_mirrorModel = M;

        // OBJ exported from the GLB:
        std::vector<GPUMesh> mirror = GPUMesh::loadMeshGPU(M, RESOURCE_ROOT "resources/mirror/convex_mirror.obj");
        for (auto& g : mirror) m_mirrorMeshes.emplace_back(std::move(g));

        
        // --- Create Textures ---
        for (GPUMesh& mesh : m_meshes) {
            if (mesh.hasTextureCoords() && !mesh.texturePath.empty()) {
                const std::string path = mesh.texturePath;

                // 1. Check if texture is already in cache
                if (textureCache.find(path) == textureCache.end()) {
                    // 2. Not found: Load it and insert into cache
                    std::cout << "Loading unique texture: " << path << mesh.m_numIndices<< std::endl;
                    textureCache.emplace(path, Texture(path));
                }

            }
        }
        

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

    // UI / control
    bool  m_activeFreeCam = true;          // which camera gets input

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

        int dummyInteger = 0; // demo UI
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            // --- Lamp path advance ---
            double now = glfwGetTime();
            float dt = float(now - m_prevTime);
            m_prevTime = now;

            if (!m_pauseLamp) {
                m_pathU += m_lampSpeed * dt; // segments per second
            }
            m_lampPos = m_bezierPath.evalGlobal(m_pathU);
            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Views");
            ImGui::InputInt("This is an integer input", &dummyInteger); // Use ImGui::DragInt or ImGui::DragFloat for larger range of numbers.
            ImGui::InputInt("This is an integer input", &dummyInteger); // Use ImGui::DragInt or ImGui::DragFloat for larger range of numbers.
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::SliderFloat("BirdsEye half-size", &birdsEyeHalfSize, 0.5f, 10.0f); //don't update anything yet
            ImGui::SliderFloat("BirdsEye height",    &birdsEyeHeight,   1.0f, 20.0f); //don't update anything yet

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

            // Draw one view (with lamp lighting + texture cache)
            {
                const glm::mat4& M  = m_modelMatrix;
                const glm::mat4 mvpMatrix = P * V * M;
                
                const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(M));

                m_defaultShader.bind();

                // lamp uniforms (your lighting)
                glUniform3fv(m_defaultShader.getUniformLocation("lightPos"),   1, glm::value_ptr(m_lampPos));
                glUniform3fv(m_defaultShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_lampColor));

                // view/proj/model matrices (same for all meshes in this pass)
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"),       1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"),     1, GL_FALSE, glm::value_ptr(M));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));

                for (GPUMesh& mesh : m_meshes) {
                    // Choose texture from cache if the mesh declares a texture path (partner's logic)
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
                        // no cached texture → use material color or normal debug
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial ? GL_TRUE : GL_FALSE);
                    }

                    mesh.draw(m_defaultShader);
                }
            };
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
            m_envMap.bind(GL_TEXTURE0);
            glUniform1i(m_envShader.getUniformLocation("envMap"), 0);

            for (GPUMesh& mesh : m_mirrorMeshes)
                mesh.draw(m_envShader);
            // TODO - MAYBE REMOVE
            glEnable(GL_CULL_FACE);

            // Optional curve overlay (same P,V)
            m_bezierPath.drawCurve({0,0,fb.x,fb.y}, P, V, glm::vec3(0.9f, 0.2f, 0.1f));

            m_window.swapBuffers();
        }
    }

    void onKeyPressed(int key, int mods) {
        if (!m_activeFreeCam) return;

        const float move = 0.08f;
        glm::vec3 right = glm::normalize(glm::cross(m_freeCam.fwd, m_freeCam.up));
        if (key == GLFW_KEY_W) m_freeCam.pos += move * m_freeCam.fwd;
        if (key == GLFW_KEY_S) m_freeCam.pos -= move * m_freeCam.fwd;
        if (key == GLFW_KEY_A) m_freeCam.pos -= move * right;
        if (key == GLFW_KEY_D) m_freeCam.pos += move * right;
        if (key == GLFW_KEY_SPACE) m_freeCam.pos += move * m_freeCam.up;
        if (key == GLFW_KEY_C)     m_freeCam.pos -= move * m_freeCam.up;

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
    

};

int main()
{
    Application app;
    app.update();

    return 0;
}
