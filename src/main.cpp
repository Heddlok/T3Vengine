#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Config {
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr char APP_NAME[]   = "IWEngine";
}

// Utility to read an entire text file into a std::string
static std::string readFile(const std::filesystem::path& filepath) {
    std::ifstream in(filepath, std::ios::in | std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open shader file: " + filepath.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

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
    GLuint        program   = 0;
    GLuint        VAO = 0, VBO = 0;

    void initWindow() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
            throw std::runtime_error(std::string("SDL init failed: ") + SDL_GetError());

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
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
            throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());

        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1);
    }

    void initGL() {
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK)
            throw std::runtime_error("Failed to initialize GLEW");
        glGetError(); // clear

        // Load shader sources
        const std::filesystem::path shaderDir{SHADER_DIR};
        std::string vertCode = readFile(shaderDir / "vert.glsl");
        std::string fragCode = readFile(shaderDir / "frag.glsl");

        auto compileShader = [&](GLenum type, const std::string& src) {
            GLuint shader = glCreateShader(type);
            const char* cstr = src.c_str();
            glShaderSource(shader, 1, &cstr, nullptr);
            glCompileShader(shader);
            GLint ok;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char buf[512];
                glGetShaderInfoLog(shader, 512, nullptr, buf);
                throw std::runtime_error(std::string("Shader compile error: ") + buf);
            }
            return shader;
        };

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertCode);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragCode);

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char buf[512];
            glGetProgramInfoLog(program, 512, nullptr, buf);
            throw std::runtime_error(std::string("Program link error: ") + buf);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);

        float vertices[] = {
             0.0f,  0.5f,
            -0.5f, -0.5f,
             0.5f, -0.5f
        };
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
          glBindBuffer(GL_ARRAY_BUFFER, VBO);
          glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
          glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
          glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        glViewport(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
    }

    void mainLoop() {
        bool running = true;
        SDL_Event e;
        while (running) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    glViewport(0, 0, e.window.data1, e.window.data2);
                }
            }

            // Compute rotating MVP
            float t = SDL_GetTicks() / 1000.0f;
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), t, glm::vec3(0, 0, 1));
            glm::mat4 view  = glm::mat4(1.0f);
            glm::mat4 proj  = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
            glm::mat4 mvp   = proj * view * model;

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(program);
            GLint loc = glGetUniformLocation(program, "uMVP");
            glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mvp));
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            SDL_GL_SwapWindow(window);
        }
    }

    void cleanup() {
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (program) glDeleteProgram(program);
        if (glContext) SDL_GL_DeleteContext(glContext);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main() {
    try {
        EngineApp{}.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
