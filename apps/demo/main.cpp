// Car demo

#include <iostream>
#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>

#include <engine/types.hpp>
#include <engine/application.hpp>
#include <engine/ecs.hpp>
#include <random>

using namespace Engine;
using namespace Engine::Component;
#include <chrono>

// Add these classes BEFORE the City class

class Particle {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float life;
    float maxLife;
    float size;
    
    Particle(glm::vec3 pos, glm::vec3 vel, glm::vec3 col, float l, float s)
        : position(pos), velocity(vel), color(col), life(l), maxLife(l), size(s) {}
    
    bool isAlive() const { return life > 0.0f; }
    
    void update(float deltaTime) {
        const float gravity = -9.8f;
        velocity.y += gravity * deltaTime;
        position += velocity * deltaTime;
        life -= deltaTime;
    }
};

class ParticleSystem {
public:
    std::vector<Particle> particles;
    Ref<Shader> shader;
    GLuint vao, vbo, ibo;
    
    ParticleSystem() : shader(nullptr), vao(0), vbo(0), ibo(0) {}
    
    void init(Ref<Shader> particleShader) {
        shader = particleShader;
        
        // Create a simple cube for particles
        std::vector<glm::vec3> vertices = {
            {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
            {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
        };
        
        std::vector<GLuint> indices = {
            0,1,2, 2,3,0,  4,5,6, 6,7,4,  0,3,7, 7,4,0,
            1,5,6, 6,2,1,  3,2,6, 6,7,3,  0,4,5, 5,1,0
        };
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
        
        glBindVertexArray(vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
    }
    
    ~ParticleSystem() {
        if (ibo) glDeleteBuffers(1, &ibo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
    }
    
    void explode(glm::vec3 position, int count = 150) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> velDist(-8.0f, 8.0f);
        std::uniform_real_distribution<float> upDist(5.0f, 15.0f);
        std::uniform_real_distribution<float> lifeDist(1.5f, 4.0f);
        std::uniform_real_distribution<float> sizeDist(0.15f, 0.4f);
        std::uniform_real_distribution<float> colorDist(0.0f, 0.3f);
        
        for (int i = 0; i < count; i++) {
            glm::vec3 velocity(velDist(gen), upDist(gen), velDist(gen));
            // Orange color with variations (bright orange to red-orange)
            glm::vec3 color(1.0f, 0.4f + colorDist(gen), 0.0f + colorDist(gen) * 0.2f);
            particles.emplace_back(position, velocity, color, lifeDist(gen), sizeDist(gen));
        }
    }
    
    void update(float deltaTime) {
        for (auto it = particles.begin(); it != particles.end();) {
            it->update(deltaTime);
            if (!it->isAlive()) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix = glm::mat4(1.0f)) {
        if (particles.empty() || !shader) return;
        shader->Enable();
        glBindVertexArray(vao);

        // Enable alpha blending for particles
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Use engine convention: uProjView = projection * view
        glm::mat4 projView = projectionMatrix * viewMatrix;
        shader->SetUniform("uProjView", projView);

        for (const auto& p : particles) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), p.position);
            model = glm::scale(model, glm::vec3(p.size));

            shader->SetUniform("uModel", model);

            // Fade color based on remaining life
            float alpha = p.life / p.maxLife;
            glm::vec3 fadedColor = p.color * alpha;
            shader->SetUniform("uMaterial.diffuseColor", fadedColor);
            shader->SetUniform("uMaterial.opacity", alpha);

            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        glDisable(GL_BLEND);
    }
};

// Add these global variables after the existing globals (after City city;)
ParticleSystem particleSystem;
bool explosionTriggered = false;
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

entity_id car = null, camera = null, big_H = null, small_H = null, grass = null, road = null, pummp = null,
truck = null, police = null, firetruck = null, city_parent = null, tree = null,car2=null;
Camera* camComp = nullptr;
Ref<ECS> ecs;
Ref<VFS> vfs;

class City {
public:
    
    Ref<Shader> shader = nullptr;
    // Optional 3D model buildings (loaded from assets)
    Ref<Model> bigModel1 = nullptr;
    Ref<Model> bigModel2 = nullptr;
    Ref<Model> bigModel3 = nullptr;
    Ref<Model> bigModel4 = nullptr;
    Ref<Model> trees = nullptr;
    Ref<Model> smallModel = nullptr;
    Ref<Model> grassModel = nullptr;
    Ref<Model> roadModel = nullptr;
    Ref<Model> cross = nullptr;
    Ref<Model> pummpModel = nullptr;
    std::vector<Ref<Model>> bigModels;

    // Manually adding items to the array
    void initializeModels() {
        bigModels.push_back(bigModel1);
        bigModels.push_back(bigModel2);
        bigModels.push_back(bigModel3);
        bigModels.push_back(bigModel4);

    }
    Ref<Model> randomChoice(const std::vector<Ref<Model>>& models) {
        if (models.empty()) {
            return nullptr;
        }
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, models.size() - 1);
        int randomIndex = dis(gen);
        return models[randomIndex];
    }
    City() {}

    void generate() {
        // R = 0 (Road), S = 2 (Small Building), B = 1 (Big Building), G = 4 (Grass), P = 3 (Gas Pump), T = 5 (Trees)
        std::vector<std::vector<int>> cityMap = {
            {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
            {4, 8, 6, 6, 6, 8, 6, 6, 6, 8, 6, 6, 6, 8, 6, 6, 6, 6, 6, 6, 8, 6, 6, 8, 6, 6, 8, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 4, 0, 2, 2, 2, 2, 2, 2, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 4, 4, 4, 4, 4, 2, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 8, 5, 5, 5, 5, 4, 0, 1, 1, 0, 2, 2, 0, 8, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 4, 5, 0, 0, 0, 4, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 4, 5, 0, 3, 0, 4, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 4, 5, 0, 0, 0, 4, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 2, 0, 4, 5, 5, 8, 8, 4, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 8, 6, 6, 6, 6, 6, 8, 8, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 4, 4, 4, 4, 0, 8, 6, 6, 8, 6, 6, 8, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 1, 4, 1, 0, 2, 2, 0, 8, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 6, 6, 6, 8, 6, 6, 8, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 5, 5, 5, 5, 5, 5, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 5, 5, 5, 5, 5, 5, 5, 1, 0, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 5, 4, 4, 4, 4, 4, 5, 1, 0, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 5, 5, 5, 5, 5, 5, 5, 1, 0, 5, 5, 5, 5, 5, 5, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 6, 6, 6, 6, 6, 6, 8, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 8, 6, 8, 6, 8, 6, 8, 6, 8, 1, 1, 1, 1, 1, 1, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 1, 4, 4, 4, 4, 1, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 1, 4, 1, 4, 1, 4, 1, 4, 0, 1, 4, 4, 4, 4, 1, 0, 4, 4, 4},
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 1, 1, 1, 1, 1, 0, 8, 4, 4},
            {8, 8, 6, 6, 6, 8, 6, 8, 6, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 6, 6, 6, 6, 6, 6, 8, 4, 4, 4},
            {0, 4, 4, 4, 4, 0, 4, 0, 4, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 4, 4, 4},
            {0, 4, 4, 4, 4, 8, 6, 1, 6, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 4, 4, 4, 1, 0, 4, 4, 4},
            {0, 4, 4, 4, 4, 0, 4, 0, 1, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 1, 1, 1, 1, 1, 1, 0, 4, 4, 4},
            {8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 4, 4, 4},
            {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}
        };

        float tileSize = 2.0f;
        int count = 0;

        int rows = cityMap.size();
        int cols = cityMap[0].size();

        for (int z = 0; z < rows; z++) {
            for (int x = 0; x < cols; x++) {
                int tile = cityMap[z][x];

                // Center the grid around origin (0,0)
                float worldX = (x - cols / 2.0f) * tileSize;
                float worldZ = (z - rows / 2.0f) * tileSize;

                // If tile is -1, it's already occupied by a multi-tile building; skip
                if (tile == -1) continue;

                // If we have 3x3 building models loaded, place them and mark surrounding tiles as occupied (-1)
                if (tile == 1 && bigModel1) {
                    // Instantiate big building model centered on this tile, occupying 3x3
                    Ref<Model> bigModel = randomChoice(bigModels);

                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel1);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(2, 3);
                    // Optionally scale the model to roughly cover 3x3 tiles
                    ref.SetScale({ tileSize / 1.0,dis(gen),tileSize / 1.0 });
                    entity_id i = ecs->Instantiate(null, Component::Transform(), cross);
                    auto ref1 = ecs->GetTransformRef(i);
                    ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });

                    continue;
                }
                else if (tile == 2 && bigModel4) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel4);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 1.5f), 0.75f,  (tileSize / 1.5f) });
                    entity_id i = ecs->Instantiate(null, Component::Transform(), cross);
                    auto ref1 = ecs->GetTransformRef(i);
                    ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });

                    continue;
                }
                else if (tile == 4 && grassModel && trees) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), grassModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    entity_id i = ecs->Instantiate(e, Component::Transform(), trees);
                    auto ref1 = ecs->GetTransformRef(i);
                    // ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ tileSize / 2 , 20.0f,  tileSize / 2 });


                    continue;
                }
                else if (tile == 0 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 6 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetRotation(glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)));
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 8 && cross) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), cross);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 3 && pummpModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), pummpModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize * 3 / 2.0f), 0.74f,  (tileSize * 3 / 2.0f) });
                    continue;
                }
                else if (tile == 5 && bigModel3) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel3);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 1.0f, (tileSize / 2.0f) });
                    entity_id i = ecs->Instantiate(null, Component::Transform(), cross);
                    auto ref1 = ecs->GetTransformRef(i);
                    ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 7 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });

                    std::cout << worldX << "|" << worldZ << std::endl;
                    continue;
                    Log::info("{} {}", worldX, worldZ);
                }
               
            }
        }
    }

    
};

Ref<Shader> shader = nullptr;
City city;

// Smooth camera tracking variables
static glm::vec3 smoothCamPos = glm::vec3(0, 0, 0);
static float smoothCarRotation = 0.0f;
// Car state machine for demo movement
enum CarState {
    MOVE_LEFT,
    FIRST_TURN,
    MOVING_FORWARD,
    TURNING,
    TURNING_RIGHT,
    STOPPED,
    STOP_SECOND,
    CRASH,
};
enum cameraMode {
    CAMERA_FOLLOW,
    TRACKING,
    SPIN,
    ASCEND,
    TRACK_ESCAPING

};
static cameraMode camMode = TRACKING;
static CarState carState = MOVE_LEFT;
static float carYaw = 0.0f; // radians
static float carSpeed = 7.0f; // world units per second
auto spinAngle = 0.0f;

extern "C" {
    void scene_init(scene_data_t scene_data) {

        // Scene preamble
        Application& app = Application::Get();
        ecs = app.GetECS();
        vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();
        auto module_name = string(scene_data.module_name);

        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/color"));
        city_parent = ecs->CreateEntity3D(null, Component::Transform(), "CityParent");
        // Setup camera
        camera = ecs->CreateEntity3D(null, Component::Transform(), "Main Camera");
        auto& cam = ecs->AddComponent<Camera>(camera, Camera::Perspective());
        cam.isMain = true;
        camComp = &cam;

        // Load and instantiate our 3D model
        Ref<Model> model = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/car_expo.glb"));
        car = ecs->Instantiate(null, Component::Transform(), model);
        auto carT = ecs->GetTransformRef(car);
        carT.SetPosition({ 20,0.0f,22.5 });
        carT.SetRotation(glm::angleAxis(1.5708f, glm::vec3({ 0,1,0 })));
        carT.SetScale(glm::vec3(0.5, 0.5, 0.5));

        Ref<Model> model1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/tank.glb"));
        truck = ecs->Instantiate(null, Component::Transform(), model1);
        auto truckT = ecs->GetTransformRef(truck);
        truckT.SetPosition({ 5.0f, 0.0f, -19.0f });
        truckT.SetScale({ 0.25f,0.25f,0.25f });

        Ref<Model> modelpolice = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Police Car.glb"));
        police = ecs->Instantiate(null, Component::Transform(), modelpolice);
        auto policeT = ecs->GetTransformRef(police);
        policeT.SetPosition({ 8.0f, 0.05f, 9.0f });
        policeT.SetScale({ 0.3f,0.3f,0.3f });
        policeT.SetRotation(glm::angleAxis(2*1.5708f, glm::vec3({ 0,1,0 })));

        Ref<Model> model_car2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/red_car.glb"));
        car2 = ecs->Instantiate(null, Component::Transform(), model_car2);
        auto car2T = ecs->GetTransformRef(car2);
        car2T.SetPosition({ 8.0f,0.1f,7.0f });
        car2T.SetScale({ 0.5f,0.5f,0.5f });

        //policeT.SetRotation(glm::angleAxis(1.5708f, glm::vec3({ 0,1,0 })));
    // Load building models (if present) and assign to city

        Ref<Model> cityModel = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H1.glb"), LoadCfg::Model{ .static_mesh = true });
        city.bigModel1 = cityModel;

        Ref<Model> cityModel1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H2.glb"), LoadCfg::Model{ .static_mesh = true });

        city.bigModel2 = cityModel1;

        Ref<Model> cityModel2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H3.glb"), LoadCfg::Model{ .static_mesh = true });

        city.bigModel3 = cityModel2;

        Ref<Model> cityModel3 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H4.glb"), LoadCfg::Model{ .static_mesh = true });

        city.bigModel4 = cityModel3;

        Ref<Model> grass = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/grass.glb"));
        city.grassModel = grass;

        Ref<Model> road = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/road.glb"));
        city.roadModel = road;

        Ref<Model> road1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/cross.glb"));
        city.cross = road1;

        Ref<Model> small_H = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/small_b.glb"), LoadCfg::Model{ .static_mesh = true });
        city.smallModel = small_H;

        Ref<Model> pummp = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/gas_pump.glb"), LoadCfg::Model{ .static_mesh = true });
        city.pummpModel = pummp;
        Ref<Model> tree = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/tree.glb"), LoadCfg::Model{ .static_mesh = true });
        city.trees = tree;
        // Create parent entity for all city objects

        city.shader = shader;
        city.initializeModels();
        city.generate();
        // Initialize particle system with an unlit shader from engine assets
        try {
            Ref<Shader> particleShader = rs->load<Shader>(vfs->GetEngineResourcePath("assets/shaders/unlit"));
            particleSystem.init(particleShader);
        }
        catch (...) {
            Log::warn("Failed to load particle shader; particles will be disabled");
        }
    }


    void scene_update(float deltaTime) {
        // Get car transform
        //auto deltaTime = (float)glfwGetTime();
        auto carTransform = ecs->GetTransformRef(car);
        glm::vec3 carPos = carTransform.GetPosition();
        glm::quat carRot = carTransform.GetRotation();

        auto car2T = ecs->GetTransformRef(car2);
        glm::vec3 car2Pos = car2T.GetPosition();

        auto policeT = ecs->GetTransformRef(police);
        glm::vec3 polpos = policeT.GetPosition();
        // Extract car's Y-axis rotation (yaw)
        glm::vec3 carEuler = glm::eulerAngles(carRot);
        float carYaw = carEuler.y;
        float carYaw2 = 0.0f;
        float y = 0.0f;
        switch (camMode) {
        case CAMERA_FOLLOW: {
            // Smooth rotation interpolation
            smoothCarRotation = glm::mix(smoothCarRotation, carYaw, 0.1f);

            // Camera follows behind the car
            float distance = 0.0f;  // distance behind car
            float height = 0.32f;     // height above car

            // Calculate camera offset in car's local space
            glm::vec3 localOffset = glm::vec3(
                sin(smoothCarRotation) * distance,
                height,
                cos(smoothCarRotation) * distance
            );

            glm::vec3 targetCamPos = carPos + localOffset;

            // Smooth camera position follow
            smoothCamPos = glm::mix(smoothCamPos, targetCamPos, 0.1f);

            // Update camera entity transform
            auto cameraTransform = ecs->GetTransformRef(camera);
            //cameraTransform.SetPosition(smoothCamPos);
           // cameraTransform.MoveTowards(targetCamPos,1.0f);
           // cameraTransform.RotateAround(carPos, 1.0f);

            // Make camera look at car
            camComp->LookAt(targetCamPos, carTransform.Forward() + carPos);
            break;
        }
        case TRACKING: {
            // Fixed tracking position
            glm::vec3 trackingPos = glm::vec3(-13.0f, 10.0f, 22.0f);
            auto cameraTransform = ecs->GetTransformRef(camera);
            cameraTransform.SetPosition(trackingPos);

            // Make camera look at car
            camComp->LookAt(trackingPos, carPos);
            break;
        }
        case ASCEND: {
            auto cameraTransform = ecs->GetTransformRef(camera);
            glm::vec3 camPos = cameraTransform.GetPosition();
            if (camPos.y <= 10.0f)
                camPos.y += 0.1f;
            cameraTransform.SetPosition(glm::vec3{carPos.x,camPos.y,carPos.z});
            camComp->LookAt(glm::vec3{ carPos.x,camPos.y,carPos.z }, polpos);
            break;
        }
        case SPIN: {

            // Update spin angle for continuous rotation
            spinAngle += deltaTime * 0.5f; // Adjust speed by changing multiplier

            float radius = 12.0f;  // Distance from car
            float height = 8.0f;   // Height above car

            // Calculate camera position in circular orbit
            glm::vec3 offset = glm::vec3(
                cos(spinAngle) * radius,
                height,
                sin(spinAngle) * radius
            );

            glm::vec3 spinCamPos = carPos + offset;

            // Update camera transform
            auto cameraTransform = ecs->GetTransformRef(camera);
            cameraTransform.SetPosition(spinCamPos);

            // Keep camera looking at car
            camComp->LookAt(spinCamPos, glm::vec3{ 8,0,-18 });
            break;
        }
        case TRACK_ESCAPING: {
           // Camera follows behind the car
            float distance = 0.0f;  // distance behind car
            float height = 0.32f;     // height above car

            glm::vec3 localOffset = glm::vec3(
                sin(smoothCarRotation) * distance,
                height,
                cos(smoothCarRotation) * distance
            );

            glm::vec3 targetCamPos = carPos + localOffset;
            auto cameraTransform = ecs->GetTransformRef(camera);
         
            camComp->LookAt(targetCamPos, car2Pos);
            break;

        }
        }
        // Rotate car wheels
        constexpr float WHEEL_ROTATION_SPEED = 5.0f;

        glm::vec3 pos = carTransform.GetPosition();
        // polpos already declared above and used by camera logic
        // Car movement state machine
        

            switch (carState) {
            case MOVE_LEFT:
                pos.x -= carSpeed * deltaTime;
                if (pos.x < -11.5f) {
                    pos.x = -11.5f;
                    carState = FIRST_TURN;
                }
                break;
            case FIRST_TURN:
                carYaw -= 0.02f;
                if (carYaw < 0.0f) {
                    carYaw = 0.0f;
                    carState = MOVING_FORWARD;
                }
                break;
            case MOVING_FORWARD:
                camMode = CAMERA_FOLLOW;
                pos.z -= carSpeed * deltaTime;
                carYaw = 0.0f;
                if (pos.z <= -16.5f) {
                    carState = TURNING;
                    pos.z = -16.5f; // Snap to exact position
                }
                break;
            case TURNING:
                carYaw -= 0.02f;
                if (carYaw < -1.5708f) {
                    carState = TURNING_RIGHT;
                    carYaw = -1.5708f; // -90 degrees
                }
                break;
            case TURNING_RIGHT:
                pos.x += carSpeed * deltaTime;
                if (pos.x >= 2.0f) {
                    carState = STOPPED;
                    pos.x = 2.0f;
                }
                break;
            case STOPPED:
                polpos.z -= carSpeed * deltaTime;
                carYaw = -1.5708f;
                car2Pos.z-=carSpeed*deltaTime;
                camMode = ASCEND;
                if (car2Pos.z < -15) {
                    polpos.z = -15;
                    carState = STOP_SECOND;
                    camMode = TRACK_ESCAPING;

                }
                break;
            case STOP_SECOND:
                carYaw2 = 1.5708f / 2;
                car2Pos.x -= carSpeed * deltaTime;
                car2Pos.z -= carSpeed * deltaTime;
                if (car2Pos.z < -18.5) {
                   camMode = SPIN;
                    

                    carState = CRASH;
                }
                break;
            case CRASH:
                carYaw2 = 1.5708f / 2;
            //std::cout << "boom" << std::endl;
                if (!explosionTriggered) {
                    auto truckT = ecs->GetTransformRef(truck);
                    glm::vec3 truckPos = truckT.GetPosition();
                    particleSystem.explode(truckPos, 150);
                    explosionTriggered = true;
                }
                break;

            }
        

        // Apply transforms after state machine so updates run every frame
        car2T.SetPosition(car2Pos);
        car2T.SetRotation(glm::angleAxis(carYaw2, glm::vec3(0.0f, 1.0f, 0.0f)));
        policeT.SetPosition(polpos);
        carTransform.SetPosition(pos);
        carTransform.SetRotation(glm::angleAxis(carYaw, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Update particle system each frame so particles move/life decreases
        particleSystem.update(deltaTime);

        // Debug: print positions and state so we can see movement values in console
        
    }

    void scene_render() {
        city.shader->Enable();

        // Get the camera's view matrix from the ECS camera component
        glm::mat4 viewMatrix = camComp->viewMatrix;

        // Render city with the camera's view matrix
        //city.render(viewMatrix);
        // Use the camera's projection matrix so particles use the same projection as the renderer
        particleSystem.render(viewMatrix, camComp->projectionMatrix);
    }

    void scene_shutdown() {

    }

    void scene_update_fixed(float deltaTime) {
        return;
    }
}