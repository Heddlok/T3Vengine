#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

#include "Mesh.h"
#include "Map.h"

namespace Config {
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr char APP_NAME[]   = "IWEngine";
}

constexpr float WALL_HEIGHT   = 3.0f;
constexpr float PLAYER_RADIUS = 0.45f;

struct Camera {
    glm::vec3 pos;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Camera()
        : pos(0.0f, 1.0f, 0.0f),
          yaw(0.0f),
          pitch(0.0f),
          speed(3.5f),
          sensitivity(0.12f) {}

    static bool aabbVsMap(const Map& map, float x, float z, float radius) {
        int minX = int(std::floor(x - radius));
        int maxX = int(std::floor(x + radius));
        int minZ = int(std::floor(z - radius));
        int maxZ = int(std::floor(z + radius));
        for (int gx = minX; gx <= maxX; ++gx) {
            for (int gz = minZ; gz <= maxZ; ++gz) {
                int row = map.grid.size() - 1 - gz;
                if (row < 0 || row >= (int)map.grid.size()) continue;
                if (gx < 0 || gx >= (int)map.grid[row].size()) continue;
                if (!map.isWall(gx, row)) continue;
                float cx = std::clamp(x, float(gx), float(gx + 1));
                float cz = std::clamp(z, float(gz), float(gz + 1));
                float dx = x - cx;
                float dz = z - cz;
                float dist2 = dx*dx + dz*dz;
                float rad2  = radius * radius;
                std::cout << "[DEBUG] Collision cell("<<gx<<","<<gz<<") dist2="<<dist2<<" rad2="<<rad2<<std::endl;
                if (dist2 < rad2) return true;
            }
        }
        return false;
    }

    glm::mat4 getView() const {
        glm::vec3 front;
        front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front.y = std::sin(glm::radians(pitch));
        front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        return glm::lookAt(pos, pos + glm::normalize(front), glm::vec3(0,1,0));
    }

    void processKeyboard(const Uint8* keys, float dt, const Map& map) {
        glm::vec3 front;
        front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front.y = 0.0f;
        front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0,1,0)));

        glm::vec3 move(0.0f);
        if (keys[SDL_SCANCODE_W]) move += front;
        if (keys[SDL_SCANCODE_S]) move -= front;
        if (keys[SDL_SCANCODE_A]) move -= right;
        if (keys[SDL_SCANCODE_D]) move += right;
        if (glm::length(move) > 0.0f) move = glm::normalize(move) * speed * dt;

        glm::vec3 tryPos = pos + move;
        if (!aabbVsMap(map, tryPos.x, tryPos.z, PLAYER_RADIUS)) {
            pos = tryPos;
        } else {
            glm::vec3 p = pos;
            p.x += move.x;
            if (!aabbVsMap(map, p.x, p.z, PLAYER_RADIUS)) pos.x = p.x;
            p = pos;
            p.z += move.z;
            if (!aabbVsMap(map, p.x, p.z, PLAYER_RADIUS)) pos.z = p.z;
        }
    }

    void processMouse(int dx, int dy) {
        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
    }
};

class EngineApp {
public:
    void run() { initWindow(); initGL(); mainLoop(); cleanup(); }

private:
    SDL_Window*           window    = nullptr;
    SDL_GLContext         glContext = nullptr;
    std::unique_ptr<Mesh> mesh;
    GLuint                program   = 0;
    Map                   map;
    std::vector<glm::vec3> wallPositions;
    Camera                camera;

    void initWindow() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) throw std::runtime_error("SDL_Init failed");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        window = SDL_CreateWindow(Config::APP_NAME,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window) throw std::runtime_error("SDL_CreateWindow failed");
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) throw std::runtime_error("SDL_GL_CreateContext failed");
        SDL_GL_SetSwapInterval(1);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(shader, 512, nullptr, log);
            throw std::runtime_error(log);
        }
        return shader;
    }

    void initGL() {
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) throw std::runtime_error("glewInit failed");
        glGetError();
        glViewport(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        const char* vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            out vec3 FragPos;
            out vec3 Normal;
            void main() {
                FragPos = vec3(uModel * vec4(aPos, 1.0));
                Normal = mat3(transpose(inverse(uModel))) * aNormal;
                gl_Position = uProjection * uView * vec4(FragPos, 1.0);
            }
        )";
        const char* fragSrc = R"(
            #version 330 core
            in vec3 FragPos;
            in vec3 Normal;
            out vec4 FragColor;
            uniform vec3 uLightPos;
            uniform vec3 uViewPos;
            uniform vec3 uLightColor;
            uniform vec3 uObjectColor;
            void main() {
                vec3 ambient = 0.18 * uLightColor;
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * uLightColor;
                vec3 viewDir = normalize(uViewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                vec3 specular = 0.5 * spec * uLightColor;
                vec3 color = (ambient + diffuse + specular) * uObjectColor;
                FragColor = vec4(color, 1.0);
            }
        )";
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        mesh = std::make_unique<Mesh>(std::string(ASSET_DIR) + "/model.obj");
        if (!map.load("maps/map.txt")) throw std::runtime_error("map load failed");

        wallPositions.clear();
        for (int y = 0; y < (int)map.grid.size(); ++y) {
            for (int x = 0; x < (int)map.grid[y].size(); ++x) {
                if (map.grid[y][x] == '#') {
                    wallPositions.emplace_back(
                        x + 0.5f,
                        0.0f,
                        (map.grid.size() - 1 - y) + 0.5f
                    );
                }
            }
        }
        std::cout << "[DEBUG] walls=" << wallPositions.size() << std::endl;
        if (map.playerSpawn.x >= 0) {
            camera.pos = glm::vec3(
                map.playerSpawn.x + 0.5f,
                1.0f,
                (map.grid.size() - 1 - map.playerSpawn.y) + 0.5f
            );
            camera.yaw = 0.0f;
        }
    }

    void mainLoop() {
        SDL_Event e;
        Uint64 last = SDL_GetPerformanceCounter();
        while (true) {
            Uint64 now = SDL_GetPerformanceCounter();
            float dt = float(now - last) / float(SDL_GetPerformanceFrequency());
            last = now;

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) return;
                if (e.type == SDL_MOUSEMOTION)
                    camera.processMouse(e.motion.xrel, e.motion.yrel);
            }
            camera.processKeyboard(SDL_GetKeyboardState(nullptr), dt, map);

            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);

            glm::mat4 view = camera.getView();
            glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                float(Config::WINDOW_WIDTH) / Config::WINDOW_HEIGHT, 0.1f, 100.0f);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(program, "uViewPos"), 1, glm::value_ptr(camera.pos));
            glUniform3f(glGetUniformLocation(program, "uLightPos"), 2.0f, 4.0f, 2.0f);
            glUniform3f(glGetUniformLocation(program, "uLightColor"), 1.0f, 1.0f, 1.0f);
            glUniform3f(glGetUniformLocation(program, "uObjectColor"), 0.5f, 0.5f, 0.5f);

            // Draw floor
            glm::mat4 floorM = glm::translate(glm::mat4(1.0f),
                glm::vec3(map.grid[0].size() * 0.5f, 0.0f, map.grid.size() * 0.5f));
            floorM = glm::scale(floorM,
                glm::vec3((float)map.grid[0].size(), 1.0f, (float)map.grid.size()));
            glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, glm::value_ptr(floorM));
            mesh->draw();

            // Draw walls
            for (const auto& p : wallPositions) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), p);
                model = glm::scale(model, glm::vec3(1.0f, WALL_HEIGHT, 1.0f));
                glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
                mesh->draw();
            }

            SDL_GL_SwapWindow(window);
        }
    }

    void cleanup() {
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main() {
    try { EngineApp{}.run(); }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
