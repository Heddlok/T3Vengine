#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <memory>
#include <vector>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"
#include "Map.h"

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

constexpr float WALL_HEIGHT = 3.0f; // Set wall height here!

struct Camera {
    glm::vec3 pos{0, 0, 3};
    float yaw = -90.0f, pitch = 0.0f;
    float speed = 3.5f, sensitivity = 0.12f;

    static bool aabbVsMap(const Map& map, float x, float z, float radius) {
        int minX = int(std::floor(x - radius));
        int maxX = int(std::floor(x + radius));
        int minZ = int(std::floor(z - radius));
        int maxZ = int(std::floor(z + radius));
        std::cout << "Checking player at (" << x << ", " << z << ") against grid X: " << minX << "-" << maxX << ", Z: " << minZ << "-" << maxZ << std::endl;
        for (int gx = minX; gx <= maxX; ++gx) {
            for (int gz = minZ; gz <= maxZ; ++gz) {
                int mapRow = map.grid.size() - 1 - gz;
                if (mapRow < 0 || mapRow >= (int)map.grid.size()) continue;
                if (gx < 0 || gx >= (int)map.grid[mapRow].size()) continue;
                std::cout << "  Checking cell (" << gx << ", " << mapRow << "): '" << map.grid[mapRow][gx] << "'" << std::endl;
                if (map.isWall(gx, mapRow)) {
                    std::cout << "BLOCK: gx=" << gx << " gz=" << mapRow
                              << " (map.grid[" << mapRow << "][" << gx << "]: '" << map.grid[mapRow][gx]
                              << "') for player at x=" << x << " z=" << z << std::endl;
                    return true;
                }
            }
        }
        return false;
    }

    glm::mat4 getView() const {
        glm::vec3 front{
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
        };
        return glm::lookAt(pos, pos + glm::normalize(front), {0,1,0});
    }

    void processKeyboard(const Uint8* keys, float dt, const Map& map) {
        const float PLAYER_RADIUS = 0.45f;

        glm::vec3 front{
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            0.0f,
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
        };
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, {0,1,0}));

        glm::vec3 move = glm::vec3(0.0f);
        if (keys[SDL_SCANCODE_W]) move += front;
        if (keys[SDL_SCANCODE_S]) move -= front;
        if (keys[SDL_SCANCODE_A]) move -= right;
        if (keys[SDL_SCANCODE_D]) move += right;

        if (glm::length(move) > 0.0f)
            move = glm::normalize(move) * speed * dt;

        glm::vec3 tryPos = pos + move;

        if (!aabbVsMap(map, tryPos.x, tryPos.z, PLAYER_RADIUS)) {
            pos = tryPos;
            std::cout << "Player moved to x=" << pos.x << " z=" << pos.z << std::endl;
        } else {
            glm::vec3 xOnly = pos; xOnly.x += move.x;
            if (!aabbVsMap(map, xOnly.x, xOnly.z, PLAYER_RADIUS)) {
                pos.x = xOnly.x;
                std::cout << "Player moved X to x=" << pos.x << " z=" << pos.z << std::endl;
            }
            glm::vec3 zOnly = pos; zOnly.z += move.z;
            if (!aabbVsMap(map, zOnly.x, zOnly.z, PLAYER_RADIUS)) {
                pos.z = zOnly.z;
                std::cout << "Player moved Z to x=" << pos.x << " z=" << pos.z << std::endl;
            }
        }
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
    std::unique_ptr<Mesh> mesh;
    GLuint program = 0;
    Map map;
    std::vector<glm::vec3> wallPositions;
    Camera camera;

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

        glViewport(0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        // SHADERS: Lighting as before
        const char* vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            out vec3 FragPos;
            out vec3 Normal;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
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
                float ambientStrength = 0.18;
                vec3 ambient = ambientStrength * uLightColor;
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * uLightColor;
                float specularStrength = 0.5;
                vec3 viewDir = normalize(uViewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                vec3 specular = specularStrength * spec * uLightColor;
                vec3 result = (ambient + diffuse + specular) * uObjectColor;
                FragColor = vec4(result, 1.0);
            }
        )";

        auto compileShader = [](GLenum type, const char* src){
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
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

        mesh = std::make_unique<Mesh>(std::string(ASSET_DIR) + "/model.obj");
        std::cout << "Loaded mesh with " << mesh->vertexCount << " vertices.\n";

        // --- Load the map! ---
        if (!map.load("maps/map.txt")) {
            throw std::runtime_error("Failed to load maps/map.txt!");
        }
        std::cout << "Loaded map with " << map.grid.size() << " rows.\n";

        // --- Extra debug for the region of interest ---
        int z_interest = 1;
        int x_interest = 32;
        int row_interest = map.grid.size() - 1 - z_interest;
        std::cout << "Region of interest: x=" << x_interest << ", z=" << z_interest << "\n";
        std::cout << "Corresponds to map[" << row_interest << "][" << x_interest << "]\n";
        std::cout << "map[" << row_interest << "][" << x_interest << "] = '" << map.grid[row_interest][x_interest] << "'\n";
        std::cout << "Full row " << row_interest << ": " << map.grid[row_interest] << "\n";
        std::cout << "All wall columns in row " << row_interest << ": ";
        for (int x = 0; x < map.grid[row_interest].size(); ++x)
            if (map.grid[row_interest][x] == '#')
                std::cout << x << " ";
        std::cout << "\n";

        // --- Gather all wall tile positions ---
        wallPositions.clear();
        for (int y = 0; y < (int)map.grid.size(); ++y) {
            for (int x = 0; x < (int)map.grid[y].size(); ++x) {
                if (map.grid[y][x] == '#') {
                    int worldZ = map.grid.size() - 1 - y;
                    wallPositions.emplace_back(x, 0, worldZ);
                    std::cout << "Drawing wall at (" << x << ", " << worldZ
                              << ") because map[" << y << "][" << x << "] = '"
                              << map.grid[y][x] << "'\n";
                }
            }
        }

        // --- Start camera at player spawn (flip row for Z) ---
        if (map.playerSpawn.x >= 0 && map.playerSpawn.y >= 0)
            camera.pos = glm::vec3(map.playerSpawn.x + 0.5f, 1.0f, map.grid.size() - 1 - map.playerSpawn.y + 0.5f);
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
                else if (e.type == SDL_MOUSEMOTION)
                    camera.processMouse(e.motion.xrel, e.motion.yrel);
            }
            camera.processKeyboard(SDL_GetKeyboardState(nullptr), dt, map);

            // Print player position each frame for debug
            std::cout << "Player at x=" << camera.pos.x << ", z=" << camera.pos.z << std::endl;

            glm::mat4 view = camera.getView();
            float aspect = float(Config::WINDOW_WIDTH) / Config::WINDOW_HEIGHT;
            glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);

            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));

            // Lighting uniforms
            glm::vec3 lightPos = glm::vec3(2, 4, 2);
            glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
            glm::vec3 objectColor = glm::vec3(0.5f, 0.5f, 0.5f); // gray walls
            glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, glm::value_ptr(lightPos));
            glUniform3fv(glGetUniformLocation(program, "uLightColor"), 1, glm::value_ptr(lightColor));
            glUniform3fv(glGetUniformLocation(program, "uObjectColor"), 1, glm::value_ptr(objectColor));
            glUniform3fv(glGetUniformLocation(program, "uViewPos"), 1, glm::value_ptr(camera.pos));

            // --- Draw all walls as cubes (with tall height) ---
            for (const auto& pos : wallPositions) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, pos);
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

int main(){
    try { EngineApp{}.run(); }
    catch(const std::exception& ex){
        std::cerr<<"Fatal: "<<ex.what()<<std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
