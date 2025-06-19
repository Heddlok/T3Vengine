#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <memory>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"
#include "Material.h"

namespace Config {
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr char APP_NAME[]   = "IWEngine";
}

static std::string readFile(const std::filesystem::path& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + filepath.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Camera {
    glm::vec3 pos{0,0,3};
    float yaw = -90.0f, pitch = 0.0f;
    float speed = 5.0f, sensitivity = 0.1f;

    glm::mat4 getView() const {
        glm::vec3 front{
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        };
        return glm::lookAt(pos, pos + glm::normalize(front), {0,1,0});
    }

    void processKeyboard(const Uint8* keys, float dt) {
        glm::vec3 front{
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            0.0f,
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        };
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, {0,1,0}));
        if (keys[SDL_SCANCODE_W]) pos += front * speed * dt;
        if (keys[SDL_SCANCODE_S]) pos -= front * speed * dt;
        if (keys[SDL_SCANCODE_A]) pos -= right * speed * dt;
        if (keys[SDL_SCANCODE_D]) pos += right * speed * dt;
    }

    void processMouse(int dx, int dy) {
        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
};

class EngineApp {
public:
    void run() {
        initWindow();
        initGL();
        mainLoop();
        cleanup();
    }

private:
    SDL_Window*           window    = nullptr;
    SDL_GLContext         glContext = nullptr;
    Camera                camera;
    glm::mat4             projection;
    GLuint                program   = 0;
    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Material> material;

    void initWindow() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        window = SDL_CreateWindow(
            Config::APP_NAME,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );
        if (!window) throw std::runtime_error("SDL_CreateWindow failed");
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) throw std::runtime_error("SDL_GL_CreateContext failed");
        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    void initGL() {
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) throw std::runtime_error("glewInit failed");
        glGetError(); // clear

        glEnable(GL_DEPTH_TEST);
        float aspect = float(Config::WINDOW_WIDTH) / Config::WINDOW_HEIGHT;
        projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

        // Compile shaders
        auto vertSrc = readFile(std::filesystem::path(SHADER_DIR) / "vert.glsl");
        auto fragSrc = readFile(std::filesystem::path(SHADER_DIR) / "frag.glsl");
        auto compileShader = [&](GLenum type, const std::string& src){
            GLuint s = glCreateShader(type);
            const char* ptr = src.c_str();
            glShaderSource(s, 1, &ptr, nullptr);
            glCompileShader(s);
            GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char buf[512]; glGetShaderInfoLog(s, 512, nullptr, buf);
                throw std::runtime_error("Shader compile error: " + std::string(buf));
            }
            return s;
        };
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        { GLint ok; glGetProgramiv(program, GL_LINK_STATUS, &ok);
          if (!ok) {
              char buf[512]; glGetProgramInfoLog(program, 512, nullptr, buf);
              throw std::runtime_error("Program link error: " + std::string(buf));
          }
        }
        glDeleteShader(vs); glDeleteShader(fs);

        // Load mesh
        mesh = std::make_unique<Mesh>(std::string(ASSET_DIR) + "/model.obj");
        std::cout << "Loaded mesh with " << mesh->vertexCount << " vertices.\n";

        // Create material: albedo, normal, specular, shininess
        material = std::make_unique<Material>(
            "texture.png",        // albedo
            "normal_map.png",      // normal map
            "specular_map.png",    // specular map
            64.0f                   // shininess
        );

        glViewport(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
    }

    void mainLoop() {
        bool running = true;
        SDL_Event e;
        Uint64 now = SDL_GetPerformanceCounter();

        while (running) {
            Uint64 last = now;
            now = SDL_GetPerformanceCounter();
            float dt = float(now - last) / float(SDL_GetPerformanceFrequency());

            while(SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                else if (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_RESIZED)
                    glViewport(0, 0, e.window.data1, e.window.data2);
                else if (e.type==SDL_MOUSEMOTION)
                    camera.processMouse(e.motion.xrel,e.motion.yrel);
            }
            camera.processKeyboard(SDL_GetKeyboardState(nullptr), dt);

            glm::mat4 view = camera.getView();
            static float angle = 0.0f;
            angle += dt * glm::radians(45.0f);
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0));
            glm::mat4 mvp   = projection * view * model;

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);

            // Pass uniforms
            glUniformMatrix4fv(glGetUniformLocation(program,"uMVP"),1,GL_FALSE,glm::value_ptr(mvp));
            glUniformMatrix4fv(glGetUniformLocation(program,"uModel"),1,GL_FALSE,glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(program,"uView"),1,GL_FALSE,glm::value_ptr(view));
            glUniform3fv(glGetUniformLocation(program,"uCameraPos"),1,glm::value_ptr(camera.pos));
            glUniform3fv(glGetUniformLocation(program,"uLightDir"),1,glm::value_ptr(glm::normalize(glm::vec3(1,1,0))));

            // Bind material and draw
            material->bind(program);
            mesh->draw();

            SDL_GL_SwapWindow(window);
        }
    }

    void cleanup() {
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main(){
    try { EngineApp{}.run(); }
    catch(const std::exception& ex){
        std::cerr<<"Fatal: "<<ex.what()<<std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
