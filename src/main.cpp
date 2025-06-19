#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Config {
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr char APP_NAME[]   = "IWEngine";
}

// Utility to read a file into a string
static std::string readFile(const std::filesystem::path& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open file: " + filepath.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// First-person camera (WASD + mouse look)
struct Camera {
    glm::vec3 pos{0,0,3};
    float yaw = -90.0f, pitch = 0.0f;
    float speed = 5.0f, sensitivity = 0.1f;

    glm::mat4 getView() const {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::lookAt(pos, pos + glm::normalize(front), {0,1,0});
    }

    void processKeyboard(const Uint8* keys, float dt) {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = 0.0f;
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
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
    SDL_Window*   window    = nullptr;
    SDL_GLContext glContext = nullptr;
    Camera        camera;
    glm::mat4     projection;
    GLuint        program = 0;
    GLuint        VAO = 0, VBO = 0, EBO = 0, texture = 0;

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
        if (!window)
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));

        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
            throw std::runtime_error("SDL_GL_CreateContext failed: " + std::string(SDL_GetError()));

        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1);  // VSync
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    void initGL() {
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK)
            throw std::runtime_error("glewInit failed");
        glGetError();  // clear stray error

        glEnable(GL_DEPTH_TEST);

        float aspect = float(Config::WINDOW_WIDTH) / Config::WINDOW_HEIGHT;
        projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

        // Load shaders
        auto vertSrc = readFile(std::filesystem::path(SHADER_DIR) / "vert.glsl");
        auto fragSrc = readFile(std::filesystem::path(SHADER_DIR) / "frag.glsl");
        auto compileShader = [&](GLenum type, const std::string& src) {
            GLuint s = glCreateShader(type);
            const char* ptr = src.c_str();
            glShaderSource(s, 1, &ptr, nullptr);
            glCompileShader(s);
            GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char buf[512];
                glGetShaderInfoLog(s, 512, nullptr, buf);
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
        {
            GLint ok; glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (!ok) {
                char buf[512];
                glGetProgramInfoLog(program, 512, nullptr, buf);
                throw std::runtime_error("Program link error: " + std::string(buf));
            }
        }
        glDeleteShader(vs);
        glDeleteShader(fs);

        // Quad: 4 verts (pos + uv) and 6 indices
        float verts[] = {
            // pos            // uv
            -1.0f,-1.0f,0.0f,  0.0f,0.0f,
             1.0f,-1.0f,0.0f,  1.0f,0.0f,
             1.0f, 1.0f,0.0f,  1.0f,1.0f,
            -1.0f, 1.0f,0.0f,  0.0f,1.0f
        };
        unsigned int idx[] = { 0,1,2, 0,2,3 };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);

          glBindBuffer(GL_ARRAY_BUFFER, VBO);
          glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
          glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

          // position
          glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
          glEnableVertexAttribArray(0);
          // UV
          glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
          glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        // Load texture or fallback checkerboard
        int w, h, n;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load((std::filesystem::path(SHADER_DIR) / "texture.png").c_str(), &w, &h, &n, 0);
        static unsigned char checker[2*2*3] = {
            255,255,255,   0,0,0,
              0,0,0,     255,255,255
        };
        if (!data) {
            w = h = 2; n = 3;
            data = checker;
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLenum internalFmt = (n == 4 ? GL_RGBA8 : GL_RGB8);
        GLenum fmt = (n == 4 ? GL_RGBA : GL_RGB);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (data != checker) {
            stbi_image_free(data);
        }

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

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED)
                    glViewport(0, 0, e.window.data1, e.window.data2);
                else if (e.type == SDL_MOUSEMOTION)
                    camera.processMouse(e.motion.xrel, e.motion.yrel);
            }

            camera.processKeyboard(SDL_GetKeyboardState(nullptr), dt);

            glm::mat4 view  = camera.getView();
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 mvp   = projection * view * model;

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
            glUniformMatrix4fv(glGetUniformLocation(program, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);

            // Draw the quad via indices (6 vertices)
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

            SDL_GL_SwapWindow(window);
        }
    }

    void cleanup() {
        if (texture)   glDeleteTextures(1, &texture);
        if (EBO)       glDeleteBuffers(1, &EBO);
        if (VBO)       glDeleteBuffers(1, &VBO);
        if (VAO)       glDeleteVertexArrays(1, &VAO);
        if (program)   glDeleteProgram(program);
        if (glContext) SDL_GL_DeleteContext(glContext);
        if (window)    SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main() {
    try {
        EngineApp{}.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
