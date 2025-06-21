#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iterator>

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
#include "CollisionGrid.h"
#include "Material.h"

namespace Config {
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr char APP_NAME[]   = "IWEngine";
}

constexpr float WALL_HEIGHT   = 3.0f;
constexpr float PLAYER_RADIUS = 0.45f;

static std::string readFile(const std::string& path) {
    std::ifstream in{path};
    if (!in) throw std::runtime_error("Failed to open file: " + path);
    return std::string{std::istreambuf_iterator<char>(in), {}};
}

struct Camera {
    glm::vec3 pos;
    float yaw, pitch, speed, sensitivity;

    Camera()
      : pos(0.0f, 1.0f, 0.0f),
        yaw(-90.0f), pitch(0.0f),
        speed(3.5f), sensitivity(0.12f)
    {}

    glm::mat4 getView() const {
        glm::vec3 front{
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        };
        return glm::lookAt(pos, pos + glm::normalize(front), glm::vec3(0,1,0));
    }

    void processKeyboard(const Uint8* keys, float dt, const CollisionGrid& grid) {
        glm::vec3 front{
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            0.0f,
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        };
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0,1,0)));

        glm::vec3 move(0.0f);
        if (keys[SDL_SCANCODE_W]) move += front;
        if (keys[SDL_SCANCODE_S]) move -= front;
        if (keys[SDL_SCANCODE_A]) move -= right;
        if (keys[SDL_SCANCODE_D]) move += right;
        if (glm::length(move) > 0.0f)
            move = glm::normalize(move) * speed * dt;

        glm::vec3 tryPos = pos + move;
        glm::vec3 sphPos{tryPos.x, pos.y, tryPos.z};
        if (!grid.collides(sphPos, PLAYER_RADIUS)) {
            pos = tryPos;
        } else {
            sphPos = {pos.x + move.x, pos.y, pos.z};
            if (!grid.collides(sphPos, PLAYER_RADIUS)) pos.x = sphPos.x;
            sphPos = {pos.x, pos.y, pos.z + move.z};
            if (!grid.collides(sphPos, PLAYER_RADIUS)) pos.z = sphPos.z;
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
    void run() {
        initWindow();
        initGL();
        mainLoop();
        cleanup();
    }

private:
    SDL_Window*               window       = nullptr;
    SDL_GLContext             glContext    = nullptr;
    std::unique_ptr<Mesh>     mesh;
    std::unique_ptr<Material> wallMaterial;
    GLuint                    program      = 0;
    Map                       map;
    std::vector<glm::vec3>    wallPositions;
    CollisionGrid             collisionGrid;
    Camera                    camera;
    GLint                     uModelLoc    = -1;
    GLint                     uUseInstLoc  = -1;
    GLint                     uAmbientLoc  = -1;
    GLint                     uLightDirLoc = -1;

    void initWindow() {
        std::cout << "Working directory: " << std::filesystem::current_path() << std::endl;
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
        SDL_ShowWindow(window);

        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
            throw std::runtime_error("SDL_GL_CreateContext failed: " + std::string(SDL_GetError()));

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
        if (glewInit() != GLEW_OK)
            throw std::runtime_error("glewInit failed");
        glGetError();

        glViewport(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        // load shaders
        std::string vertCode = readFile("../shader_sources/vert.glsl");
        std::string fragCode = readFile("../shader_sources/frag.glsl");
        GLuint vs = compileShader(GL_VERTEX_SHADER,   vertCode.c_str());
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        glUseProgram(program);

        // bind albedo sampler to unit 0
        GLint uAlbedoLoc = glGetUniformLocation(program, "uAlbedo");
        glUniform1i(uAlbedoLoc, 0);

        uModelLoc    = glGetUniformLocation(program, "uModel");
        uUseInstLoc  = glGetUniformLocation(program, "uUseInstancing");
        uAmbientLoc  = glGetUniformLocation(program, "uAmbientColor");
        uLightDirLoc = glGetUniformLocation(program, "uLightDir");

        // static uniforms
        glUniform3f(uAmbientLoc,  0.13f, 0.13f, 0.13f);
        glUniform3f(uLightDirLoc, 1.0f, -1.0f,  0.0f);

        // load mesh & material
        mesh         = std::make_unique<Mesh>(std::string(ASSET_DIR) + "/model.obj");
        wallMaterial = std::make_unique<Material>("", "", "", 32.0f);

        if (!map.load("maps/map.txt"))
            throw std::runtime_error("map load failed");

        // build walls
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

        // instance data
        std::vector<glm::mat4> inst;
        inst.reserve(wallPositions.size());
        for (auto &p : wallPositions) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), p);
            m = glm::scale(m, glm::vec3(1.0f, WALL_HEIGHT, 1.0f));
            inst.push_back(m);
        }
        mesh->setInstanceBuffer(inst);
        collisionGrid.build(wallPositions, WALL_HEIGHT);

        // spawn camera
        if (map.playerSpawn.x >= 0 && map.playerSpawn.y >= 0) {
            camera.pos = glm::vec3(
                map.playerSpawn.x + 0.5f,
                1.0f,
                (float)(map.grid.size() - 1 - map.playerSpawn.y) + 0.5f
            );
            float centerX = float(map.grid[0].size()) * 0.5f;
            float centerZ = float(map.grid.size())     * 0.5f;
            float dx = centerX - camera.pos.x;
            float dz = centerZ - camera.pos.z;
            camera.yaw   = glm::degrees(std::atan2(dz, dx));
            camera.pitch = -20.0f;
        }
    }

    void mainLoop() {
        SDL_Event e;
        Uint64 last = SDL_GetPerformanceCounter();

        while (true) {
            // solid fill
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // refresh ambient
            glUniform3f(uAmbientLoc, 0.13f, 0.13f, 0.13f);

            Uint64 now = SDL_GetPerformanceCounter();
            float dt = float(now - last) / float(SDL_GetPerformanceFrequency());
            last = now;

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) return;
                if (e.type == SDL_MOUSEMOTION)
                    camera.processMouse(e.motion.xrel, e.motion.yrel);
            }
            camera.processKeyboard(SDL_GetKeyboardState(nullptr), dt, collisionGrid);

            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);

            // camera & lighting uniforms
            glm::mat4 view = camera.getView();
            glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                              float(Config::WINDOW_WIDTH) / Config::WINDOW_HEIGHT,
                                              0.1f, 100.0f);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"),       1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv  (glGetUniformLocation(program, "uViewPos"),       1, glm::value_ptr(camera.pos));
            glUniform3f   (glGetUniformLocation(program, "uLightColor"),     1.0f, 1.0f, 1.0f);
            glUniform3f   (glGetUniformLocation(program, "uObjectColor"),    0.5f, 0.5f, 0.5f);

            // draw floor
            glm::mat4 floorM = glm::translate(glm::mat4(1.0f),
                glm::vec3(map.grid[0].size() * 0.5f, 0.0f, map.grid.size() * 0.5f));
            floorM = glm::scale(floorM,
                glm::vec3((float)map.grid[0].size(), 1.0f, (float)map.grid.size()));
            glUniform1i(uUseInstLoc, 0);
            wallMaterial->bind(program);
            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(floorM));
            mesh->drawPlain();

            // draw walls instanced
            glUniform1i(uUseInstLoc, 1);
            wallMaterial->bind(program);
            mesh->drawInstanced(static_cast<GLsizei>(wallPositions.size()));

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
    try {
        EngineApp{}.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
