#include <iostream>
#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>
#include <engine/types.hpp>
#include <engine/application.hpp>
#include <engine/ecs.hpp>
#include <engine/Tween.hpp>
#include <engine/Easing.hpp>
#include <random>
using namespace Engine;
using namespace Engine::Component;
#include <chrono>
class FireModelExplosion {
public:
    struct FireInstance {
        entity_id entityId;
        glm::vec3 velocity;
        float life;
        float maxLife;

        FireInstance(entity_id id, glm::vec3 vel, float l)
            : entityId(id), velocity(vel), life(l), maxLife(l) {}

        bool isAlive() const { return life > 0.0f; }
    };
    std::vector<FireInstance> fireParticles;
    Ref<Model> fireModel;
    Ref<ECS> ecsRef;
    bool hasExploded;
    bool isFountain; // Enable continuous spawning mode
    glm::vec3 fountainPosition; // Where to spawn new particles
    float spawnTimer; // Timer for continuous spawning
    float spawnInterval; // Seconds between spawn batches
    int particlesPerBatch; // How many particles to spawn per batch

    FireModelExplosion() : fireModel(nullptr), ecsRef(nullptr), hasExploded(false),
        isFountain(false), fountainPosition(0.0f), spawnTimer(0.0f),
        spawnInterval(0.05f), particlesPerBatch(3) {}

    void init(Ref<Model> model, Ref<ECS> ecs) {
        fireModel = model;
        ecsRef = ecs;
    }

    void explode(glm::vec3 position, int count = 100) {
        // Clean up old particles
        cleanup();
        isFountain = false; // Disable fountain mode

        hasExploded = true;

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> velDist(-8.0f, 8.0f); // Random XZ velocity
        std::uniform_real_distribution<float> upDist(5.0f, 15.0f); // Strong upward velocity
        std::uniform_real_distribution<float> lifeDist(1.5f, 4.0f); // Particle lifetime

        for (int i = 0; i < count; i++) {
            // Create random velocity in all directions (spherical explosion)
            glm::vec3 velocity(velDist(gen), upDist(gen), velDist(gen));

            // Instantiate fire.glb model at explosion position
            entity_id fireEntity = ecsRef->Instantiate(null, Component::Transform(), fireModel);
            auto fireTransform = ecsRef->GetTransformRef(fireEntity);
            fireTransform.SetPosition(position);
            fireTransform.SetScale(glm::vec3(0.5f, 0.5f, 0.5f)); // Adjust scale as needed

            // Store instance with its velocity
            fireParticles.emplace_back(fireEntity, velocity, lifeDist(gen));
        }

        Log::info("Fire explosion created with {} fire.glb instances at position ({}, {}, {})",
            count, position.x, position.y, position.z);
    }
    // Start fountain mode: continuously spawn particles in an upward cone
    void startFountain(glm::vec3 position, float interval = 0.01f, int particlesPerBatch = 3) {
        cleanup(); // Clear old particles
        isFountain = true;
        fountainPosition = position;
        spawnInterval = interval;
        particlesPerBatch = particlesPerBatch;
        spawnTimer = 0.0f;
        hasExploded = true;
    }
    // Stop fountain mode
    void stopFountain() {
        isFountain = false;
    }

    void update(float deltaTime) {
        const float gravity = -9.8f;

        // In fountain mode, spawn new particles continuously
        if (isFountain) {
            spawnTimer += deltaTime;
            while (spawnTimer >= spawnInterval) {
                spawnTimer -= spawnInterval;
                spawnFountainBatch();
            }
        }

        // Update all fire particles
        for (auto it = fireParticles.begin(); it != fireParticles.end();) {
            // Decrease life first
            it->life -= deltaTime;

            // Check if particle is dead before trying to access its transform
            if (!it->isAlive()) {
                ecsRef->DestroyEntity(it->entityId);
                it = fireParticles.erase(it);
                continue;
            }

            // Apply gravity
            it->velocity.y += gravity * deltaTime;

            // Get transform and update position (only if alive)
            try {
                auto fireTransform = ecsRef->GetTransformRef(it->entityId);
                glm::vec3 currentPos = fireTransform.GetPosition();
                currentPos += it->velocity * deltaTime;
                fireTransform.SetPosition(currentPos);
                ++it;
            }
            catch (...) {
                // If entity is invalid, remove from list
                it = fireParticles.erase(it);
            }
        }
    }
private:
    // Helper: spawn a batch of fountain particles
    void spawnFountainBatch() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> velDist(-3.0f, 3.0f); // Small random XZ spread
        std::uniform_real_distribution<float> upDist(8.0f, 12.0f); // Upward velocity for fountain
        std::uniform_real_distribution<float> lifeDist(2.0f, 3.5f); // Particle lifetime

        for (int i = 0; i < particlesPerBatch; i++) {
            // Create cone-shaped upward velocity (mostly up, some sideways spread)
            glm::vec3 velocity(velDist(gen), upDist(gen), velDist(gen));

            // Instantiate fire.glb model at fountain position
            entity_id fireEntity = ecsRef->Instantiate(null, Component::Transform(), fireModel);
            auto fireTransform = ecsRef->GetTransformRef(fireEntity);
            fireTransform.SetPosition(fountainPosition);
            fireTransform.SetScale(glm::vec3(0.5f, 0.5f, 0.5f));

            // Store instance with its velocity
            fireParticles.emplace_back(fireEntity, velocity, lifeDist(gen));
        }
    }
public:

    void cleanup() {
        // Destroy all existing fire entities safely
        for (auto& fp : fireParticles) {
            hasExploded = false;
            try {
                ecsRef->DestroyEntity(fp.entityId);
            }
            catch (...) {
                //Entity may already be destroyed; silently skip
            }
        }
        fireParticles.clear();
    }

    void reset() {
        cleanup();
        hasExploded = false;
    }
};
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
entity_id car = null, camera = null, big_H = null, small_H = null, grass = null, road = null, pummp = null,
truck = null, police = null, firetruck = null, city_parent = null, tree = null, car2 = null, fire = null, fire_truck1 = null, fire_truck2 = null;
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
            {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 8, 6, 6, 6, 6, 6, 8, 0, 1, 1, 0, 2, 2, 0, 4, 4, 4},
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
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 2 && bigModel4) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel4);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 1.5f), 0.75f, (tileSize / 1.5f) });
                    entity_id i = ecs->Instantiate(null, Component::Transform(), cross);
                    auto ref1 = ecs->GetTransformRef(i);
                    ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 4 && grassModel && trees) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), grassModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    entity_id i = ecs->Instantiate(e, Component::Transform(), trees);
                    auto ref1 = ecs->GetTransformRef(i);
                    // ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ tileSize / 2 , 20.0f, tileSize / 2 });
                    continue;
                }
                else if (tile == 0 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 6 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetRotation(glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)));
                    ref.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 8 && cross) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), cross);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 3 && pummpModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), pummpModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize * 3 / 2.0f), 0.74f, (tileSize * 3 / 2.0f) });
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
                    ref1.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
                    continue;
                }
                else if (tile == 7 && roadModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f, (tileSize / 2.0f) });
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
class Animator {
public:
    struct Keyframe {
        Transform target;
        float duration;
        Easing::Func easing;
        std::function<void()> onComplete; // Optional callback
        glm::vec3 lookAtTarget; // Target point to look at
        bool hasLookAt; // Whether this keyframe includes a look-at target

        Keyframe(const Transform& t, float d, Easing::Func e = Easing::Linear)
            : target(t), duration(d), easing(e), onComplete(nullptr),
            lookAtTarget(0.0f), hasLookAt(false) {}
    };

    struct TweenState {
        Transform startTransform;
        Transform endTransform;
        float duration;
        float elapsed;
        Easing::Func easing;
        bool isActive;
        glm::vec3 startLookAt;
        glm::vec3 endLookAt;
        bool hasLookAt;

        TweenState() : duration(0), elapsed(0), easing(Easing::Linear),
            isActive(false), startLookAt(0.0f), endLookAt(0.0f),
            hasLookAt(false) {}
    };

private:
    TweenState tweenState;
    std::vector<Keyframe> keyframeQueue;
    Transform currentTransform;
    glm::vec3 currentLookAt;
    bool isPlaying;

public:
    Animator() : isPlaying(false), currentLookAt(0.0f) {}

    // Add a single keyframe to the queue
    Animator& addKeyframe(const Transform& target, float duration, Easing::Func easing = Easing::Linear) {
        keyframeQueue.emplace_back(target, duration, easing);
        return *this;
    }

    // Add keyframe with callback
    Animator& addKeyframe(const Transform& target, float duration, Easing::Func easing, std::function<void()> callback) {
        Keyframe kf(target, duration, easing);
        kf.onComplete = callback;
        keyframeQueue.push_back(kf);
        return *this;
    }

    // NEW: Add keyframe with look-at target (for camera)
    Animator& addKeyframeWithLookAt(const Transform& target, const glm::vec3& lookAt,
        float duration, Easing::Func easing = Easing::Linear) {
        Keyframe kf(target, duration, easing);
        kf.lookAtTarget = lookAt;
        kf.hasLookAt = true;
        keyframeQueue.push_back(kf);
        return *this;
    }

    // NEW: Add keyframe with look-at and callback
    Animator& addKeyframeWithLookAt(const Transform& target, const glm::vec3& lookAt,
        float duration, Easing::Func easing,
        std::function<void()> callback) {
        Keyframe kf(target, duration, easing);
        kf.lookAtTarget = lookAt;
        kf.hasLookAt = true;
        kf.onComplete = callback;
        keyframeQueue.push_back(kf);
        return *this;
    }

    // Convenience method: add position-only keyframe
    Animator& moveTo(const glm::vec3& position, float duration, Easing::Func easing = Easing::Linear) {
        Transform target = currentTransform;
        target.position = position;
        return addKeyframe(target, duration, easing);
    }

    // NEW: Move to position while looking at target
    Animator& moveToAndLookAt(const glm::vec3& position, const glm::vec3& lookAt,
        float duration, Easing::Func easing = Easing::Linear) {
        Transform target = currentTransform;
        target.position = position;
        return addKeyframeWithLookAt(target, lookAt, duration, easing);
    }

    // Start playing the animation sequence
    void play(const Transform& startTransform, const glm::vec3& startLookAt = glm::vec3(0.0f)) {
        currentTransform = startTransform;
        currentLookAt = startLookAt;
        isPlaying = true;
        if (!keyframeQueue.empty()) {
            startNextKeyframe();
        }
    }

    // Clear all keyframes and stop
    void stop() {
        isPlaying = false;
        keyframeQueue.clear();
        tweenState.isActive = false;
    }

    // Update and return interpolated transform
    Transform update(const Transform& fallbackTransform, float deltaTime) {
        if (!isPlaying || keyframeQueue.empty()) {
            currentTransform = fallbackTransform;
            return fallbackTransform;
        }

        if (!tweenState.isActive) {
            return currentTransform;
        }

        tweenState.elapsed += deltaTime;
        float t = glm::clamp(tweenState.elapsed / tweenState.duration, 0.0f, 1.0f);

        Transform result = Engine::Tween::Interpolate(
            tweenState.startTransform,
            tweenState.endTransform,
            t,
            tweenState.easing
        );

        currentTransform = result;

        // Interpolate look-at target if present
        if (tweenState.hasLookAt) {
            currentLookAt = glm::mix(tweenState.startLookAt, tweenState.endLookAt,
                tweenState.easing(t));
        }

        // Check if current keyframe is complete
        if (tweenState.elapsed >= tweenState.duration) {
            tweenState.isActive = false;

            // Execute callback if exists
            if (!keyframeQueue.empty() && keyframeQueue[0].onComplete) {
                keyframeQueue[0].onComplete();
            }

            // Remove completed keyframe
            keyframeQueue.erase(keyframeQueue.begin());

            // Start next keyframe if available
            if (!keyframeQueue.empty()) {
                startNextKeyframe();
            }
            else {
                isPlaying = false; // Sequence complete
            }
        }

        return result;
    }

    // NEW: Get current look-at target
    glm::vec3 getCurrentLookAt() const {
        return currentLookAt;
    }

    // NEW: Check if current keyframe has look-at
    bool hasLookAt() const {
        return tweenState.hasLookAt;
    }

    bool isAnimating() const {
        return isPlaying && (tweenState.isActive || !keyframeQueue.empty());
    }

    bool hasFinished() const {
        return !isPlaying && keyframeQueue.empty();
    }

    size_t remainingKeyframes() const {
        return keyframeQueue.size();
    }

private:
    void startNextKeyframe() {
        if (keyframeQueue.empty()) return;

        const Keyframe& next = keyframeQueue[0];

        tweenState.startTransform = currentTransform;
        tweenState.endTransform = next.target;
        tweenState.duration = next.duration;
        tweenState.elapsed = 0.0f;
        tweenState.easing = next.easing;
        tweenState.isActive = true;

        // Setup look-at interpolation
        tweenState.hasLookAt = next.hasLookAt;
        if (next.hasLookAt) {
            tweenState.startLookAt = currentLookAt;
            tweenState.endLookAt = next.lookAtTarget;
        }
    }
};
// Smooth camera tracking variables
static glm::vec3 smoothCamPos = glm::vec3(20, 0.32, 23.75);
static float smoothCarRotation = 0.0f;
// Car state machine for demo movement
enum CarState {
    PARKED,
    MOVE_LEFT,
    FIRST_TURN,
    MOVING_FORWARD,
    TURNING,
    TURNING_RIGHT,
    STOPPED,
    STOP_SECOND,
    CRASH,
    FIRE_TRUCK_TURN,
    FIRE_TRUCK_MOVE_TO_PUMP,
    FIRE_TRUCK_STOP
};
enum cameraMode {
    CAMERA_FOLLOW,
    TRACKING,
    SPIN,
    GO_IN,
    GO_BACK,
    ASCEND,
    CAMERA_MOVE_INTO,
    TRACK_ESCAPING
};
static cameraMode camMode = GO_IN;
static CarState carState = MOVE_LEFT;
static float carYaw = 0.0f; // radians
static float carSpeed = 4.0f; // world units per second
auto spinAngle = 0.0f;
FireModelExplosion fireExplosion;//aaaaaaaaaaaaaaaa
Animator carAnimator;
Animator cameraAnimator;
Animator policeAnimator;
Animator car2Animator;
Animator fire1Anim;
Animator fire2Anim;
Animator cameraAnim;
extern "C" {
    void setupPoliceAndCar2Animation();
    void setupCar2CrashAnimation();
    void setupFireTrucksInitialMove();
    void setupFireTrucksTurnAnimation();
    void setupFireTrucksFinalMove();
    void setupCarAnimation();

    void setupCameraIntroAnimation() {
        auto cameraTransform = ecs->GetTransformRef(camera);
        auto carTransform = ecs->GetTransformRef(car);

        Transform camCurrent;
        camCurrent.position = cameraTransform.GetPosition(); // (20, 0.32, 23.75)
      
        camCurrent.scale = cameraTransform.GetScale();
        
        glm::vec3 carPos = carTransform.GetPosition(); // (20, 0, 22.5)
        glm::vec3 carForward = carTransform.Forward();

        cameraAnim
            // Move closer to car
            .addKeyframeWithLookAt(
                Transform{
                    glm::vec3(20.0f, 0.32f, 22.9f), // Move from 23.75 to 22.0
                    camCurrent.rotation,
                    camCurrent.scale
                },
                glm::vec3(carPos.x, 0.32, 0), // Look at car's forward direction
                5.0f,
                Easing::InSine
               
            )
            .addKeyframeWithLookAt(
                Transform{
                    glm::vec3(20.0f, 0.32f, 22.8f), // Move from 23.75 to 22.0
                    camCurrent.rotation,
                    camCurrent.scale
                },
                glm::vec3(carPos.x, 0.32, 0), // Look at car's forward direction
                1.0f,
                Easing::InSine

            ).addKeyframeWithLookAt(
                Transform{
                    glm::vec3(20.05f, 0.32f, 22.65f), // Move from 23.75 to 22.0
                    camCurrent.rotation,
                    camCurrent.scale
                },
                carForward, // Look at car's forward direction
                5.0f,
                Easing::InSine
            ).addKeyframeWithLookAt(
                Transform{
                    glm::vec3(20.05, 0.32f, 22.65f), // Move from 23.75 to 22.0
                    camCurrent.rotation,
                    camCurrent.scale
                },
                glm::vec3(-5,0,22.65), // Look at car's forward direction
                3.0f,
                Easing::InSine
                ).addKeyframeWithLookAt(
                    Transform{
                        glm::vec3(20.05, 0.32f, 22.65f), // Move from 23.75 to 22.0
                        camCurrent.rotation,
                        camCurrent.scale
                    },
                    glm::vec3(10, 0, 22.65), // Look at car's forward direction
                    3.0f,
                    Easing::InSine
                ).addKeyframeWithLookAt(
                    Transform{
                        glm::vec3(20, 0.35f, 23.75f), // Move from 23.75 to 22.0
                        camCurrent.rotation,
                        camCurrent.scale
                    },
                    glm::vec3(20,0.32,22.5), // Look at car's forward direction
                    2.0f,
                    Easing::Linear,
                    []() {
                        // After intro, start car animation and switch to tracking
                        setupCarAnimation();
                        camMode = TRACKING;
                    }
                );

        cameraAnim.play(camCurrent, carPos);
    }

    void setupCameraFollowAnimation() {
        auto cameraTransform = ecs->GetTransformRef(camera);
        auto carTransform = ecs->GetTransformRef(car);

        Transform camCurrent;
        camCurrent.position = cameraTransform.GetPosition();
        camCurrent.rotation = cameraTransform.GetRotation();
        camCurrent.scale = cameraTransform.GetScale();

        glm::vec3 carPos = carTransform.GetPosition();

        // Follow position behind car
        glm::vec3 followPos = glm::vec3(carPos.x, 0.35f, carPos.z + 1.25f);

        cameraAnim
            .moveToAndLookAt(followPos, carPos, 1.0f, Easing::InOutQuad);

        cameraAnim.play(camCurrent, carPos);
    }

    // Setup camera to ascend and look down at scene
    void setupCameraAscendAnimation() {
        auto cameraTransform = ecs->GetTransformRef(camera);
        auto carTransform = ecs->GetTransformRef(car);
        auto policeTransform = ecs->GetTransformRef(police);

        Transform camCurrent;
        camCurrent.position = cameraTransform.GetPosition();
        camCurrent.rotation = cameraTransform.GetRotation();
        camCurrent.scale = cameraTransform.GetScale();

        glm::vec3 carPos = carTransform.GetPosition();
        glm::vec3 policePos = policeTransform.GetPosition();

        cameraAnim
            // Ascend to bird's eye view
            .addKeyframeWithLookAt(
                Transform{
                    glm::vec3(carPos.x, 10.0f, carPos.z),
                    camCurrent.rotation,
                    camCurrent.scale
                },
                policePos,
                3.0f,
                Easing::InOutQuad
            );

        cameraAnim.play(camCurrent, carPos);
    }

    // Setup dramatic spinning camera around crash site

   
    void scene_update(float deltaTime) {
        fireExplosion.update(deltaTime);

        auto carTransform = ecs->GetTransformRef(car);
        auto policeT = ecs->GetTransformRef(police);
        auto car2T = ecs->GetTransformRef(car2);
        auto FT1 = ecs->GetTransformRef(fire_truck1);
        auto FT2 = ecs->GetTransformRef(fire_truck2);
        auto cameraTransform = ecs->GetTransformRef(camera);
        

        // Update all animators
        Transform carCurrent = { carTransform.GetPosition(), carTransform.GetRotation(), carTransform.GetScale() };
        Transform carAnimated = carAnimator.update(carCurrent, deltaTime);
        carTransform.SetPosition(carAnimated.position);
        carTransform.SetRotation(carAnimated.rotation);

        Transform policeCurrent = { policeT.GetPosition(), policeT.GetRotation(), policeT.GetScale() };
        Transform policeAnimated = policeAnimator.update(policeCurrent, deltaTime);
        policeT.SetPosition(policeAnimated.position);
        policeT.SetRotation(policeAnimated.rotation);

        Transform car2Current = { car2T.GetPosition(), car2T.GetRotation(), car2T.GetScale() };
        Transform car2Animated = car2Animator.update(car2Current, deltaTime);
        car2T.SetPosition(car2Animated.position);
        car2T.SetRotation(car2Animated.rotation);

        Transform fire1Current = { FT1.GetPosition(), FT1.GetRotation(), FT1.GetScale() };
        Transform fire1Animated = fire1Anim.update(fire1Current, deltaTime);
        FT1.SetPosition(fire1Animated.position);
        FT1.SetRotation(fire1Animated.rotation);

        Transform fire2Current = { FT2.GetPosition(), FT2.GetRotation(), FT2.GetScale() };
        Transform fire2Animated = fire2Anim.update(fire2Current, deltaTime);
        FT2.SetPosition(fire2Animated.position);
        FT2.SetRotation(fire2Animated.rotation);

        // NEW: Update camera with animator
        Transform camCurrent = { cameraTransform.GetPosition(), cameraTransform.GetRotation(), cameraTransform.GetScale() };
        Transform camAnimated = cameraAnim.update(camCurrent, deltaTime);
        cameraTransform.SetPosition(camAnimated.position);

        // Apply look-at if animator has it
        if (cameraAnim.hasLookAt()) {
            glm::vec3 lookAtTarget = cameraAnim.getCurrentLookAt();
            camComp->LookAt(camAnimated.position, lookAtTarget);
        }

        // Handle dynamic tracking modes that need continuous updates
        glm::vec3 carPos = carTransform.GetPosition();
        glm::vec3 car2Pos = car2T.GetPosition();

        switch (camMode) {
        case TRACKING: {
            // Continuous tracking behind car
            if (!cameraAnim.isAnimating()) {
                glm::vec3 trackingPos = glm::vec3(carPos.x, 0.35f, carPos.z + 1.25);
                cameraTransform.SetPosition(trackingPos);
                camComp->LookAt(trackingPos, carPos);
            }
            break;
        }
        case SPIN: {
            spinAngle += deltaTime * 0.32f;

            const glm::vec3 spinCenter(5.0f, 0.0f, -18.0f);
            const float radius = 9.0f;
            static float height = 4.0f;

            height += deltaTime * 1.0f;
            if (height > 10.0f) height = 10.0f;

            glm::vec3 offset(
                cos(spinAngle) * radius,
                height,
                sin(spinAngle) * radius
            );

            glm::vec3 spinCamPos = spinCenter + offset;

            auto cameraTransform = ecs->GetTransformRef(camera);
            cameraTransform.SetPosition(spinCamPos);

            camComp->LookAt(spinCamPos, spinCenter + glm::vec3(0.0f, 3.5f, 0.0f));
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
        case CAMERA_FOLLOW: {
            // Smooth follow mode
            if (!cameraAnim.isAnimating()) {
                float distance = 0.12f;
                float height = 0.32f;
                glm::vec3 localOffset = glm::vec3(
                    sin(smoothCarRotation) * distance,
                    height,
                    cos(smoothCarRotation) * distance
                );
                glm::vec3 targetCamPos = carPos + localOffset;
                smoothCamPos = glm::mix(smoothCamPos, targetCamPos, 0.1f);
                camComp->LookAt(targetCamPos, carTransform.Forward() + carPos);
            }
            break;
        }

        case ASCEND: {
            glm::vec3 trackingPos = glm::vec3(carPos.x, 10.0f, carPos.z);
            cameraTransform.SetPosition(trackingPos);
            camComp->LookAt(trackingPos, policeT.GetPosition());
        }
        }
        
    }

    // 1. CAR ANIMATION - The main sequence starter
    void setupCarAnimation() {
        auto carTransform = ecs->GetTransformRef(car);
        Transform current;
        current.position = carTransform.GetPosition();
        current.rotation = carTransform.GetRotation();
        current.scale = carTransform.GetScale();
        carAnimator
            // MOVE_LEFT
            .addKeyframe(
                Transform{
                    glm::vec3(-11.5f, current.position.y, current.position.z),
                    glm::angleAxis(1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::vec3(0.5f)
                },
                6.0f,
                Easing::InOutQuad,
                []() { camMode = CAMERA_FOLLOW; } // Switch to follow camera
            )
            // FIRST_TURN
            .addKeyframe(
                Transform{
                    glm::vec3(-12.0f, current.position.y, 20.0f),
                    glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::vec3(0.5f)
                },
                2.25f,
                Easing::Linear

            )
            // MOVING_FORWARD
            .addKeyframe(
                Transform{
                    glm::vec3(-11.5f, current.position.y, -16.5f),
                    glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::vec3(0.5f)
                },
                15.0f,
                Easing::InOutQuad
            )
            // TURNING
            .addKeyframe(
                Transform{
                    glm::vec3(-10.0f, current.position.y, -17.0f),
                    glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::vec3(0.5f)
                },
                2.25f,
                Easing::Linear
            )
            // TURNING_RIGHT (moving to final stop position)
            .addKeyframe(
                Transform{
                    glm::vec3(2.0f, current.position.y, -17.5f),
                    glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::vec3(0.5f)
                },
                6.0f,
                Easing::InOutQuad,
                []() {
                    // STOPPED - Car stops, trigger police and car2
                    camMode = ASCEND;
                    setupPoliceAndCar2Animation();
                    setupCameraAscendAnimation();
                }
        );
        carAnimator.play(current);
    }
    // 2. POLICE AND CAR2 - Run simultaneously when car stops
    void setupPoliceAndCar2Animation() {
        // Setup police animation
        auto policeT = ecs->GetTransformRef(police);
        Transform policeCurrent;
        policeCurrent.position = policeT.GetPosition();
        policeCurrent.rotation = policeT.GetRotation();
        policeCurrent.scale = policeT.GetScale();
        policeAnimator
            .addKeyframe(
                Transform{
                    glm::vec3(8.0f, 0.05f, -13.0f),
                    policeCurrent.rotation,
                    policeCurrent.scale
                },
                4.0f,
                Easing::Linear
            );
        policeAnimator.play(policeCurrent);
        // Setup car2 animation (moves to behind police)
        auto car2T = ecs->GetTransformRef(car2);
        Transform car2Current;
        car2Current.position = car2T.GetPosition();
        car2Current.rotation = car2T.GetRotation();
        car2Current.scale = car2T.GetScale();
        car2Animator
            .addKeyframe(
                Transform{
                    glm::vec3(8.0f, 0.1f, -15.0f),
                    car2Current.rotation,
                    car2Current.scale
                },
                4.0f,
                Easing::Linear,
                []() {
                    // STOP_SECOND - Car2 stops, now it escapes
                    camMode = TRACK_ESCAPING;
                    setupCar2CrashAnimation();
                }
        );
        car2Animator.play(car2Current);
    }
    // 3. CAR2 CRASH - Escape and crash into truck
    void setupCar2CrashAnimation() {
        auto car2T = ecs->GetTransformRef(car2);
        Transform car2Current;
        car2Current.position = car2T.GetPosition();
        car2Current.rotation = car2T.GetRotation();
        car2Current.scale = car2T.GetScale();
        car2Animator
            .addKeyframe(
                Transform{
                    glm::vec3(5.0f, 0.1f, -18.5f),
                    glm::angleAxis(1.5708f / 2, glm::vec3(0, 1, 0)),
                    car2Current.scale
                },
                1.0f,
                Easing::Linear,
                []() {
                    // CRASH - Start fire explosion and fire trucks
                    camMode = SPIN;
                    
                    fireExplosion.startFountain(glm::vec3(5.0f, 0.5f, -19.0f), 0.006f, 50);
                    setupFireTrucksInitialMove();
                }
        );
        car2Animator.play(car2Current);
    }
    // 4. FIRE TRUCKS - Initial move toward crash site
    void setupFireTrucksInitialMove() {
        // Fire Truck 1
        auto FT1 = ecs->GetTransformRef(fire_truck1);
        Transform ft1Current;
        ft1Current.position = FT1.GetPosition();
        ft1Current.rotation = FT1.GetRotation();
        ft1Current.scale = FT1.GetScale();
        fire1Anim
            .addKeyframe(
                Transform{
                    glm::vec3(-4.0f, 0.0f, -17.5f),
                    ft1Current.rotation,
                    ft1Current.scale
                },
                6.0f,
                Easing::Linear,
                []() {
                    // Both trucks arrived, start turning
                    setupFireTrucksTurnAnimation();
                }
        );
        fire1Anim.play(ft1Current);
        // Fire Truck 2
        auto FT2 = ecs->GetTransformRef(fire_truck2);
        Transform ft2Current;
        ft2Current.position = FT2.GetPosition();
        ft2Current.rotation = FT2.GetRotation();
        ft2Current.scale = FT2.GetScale();
        fire2Anim
            .addKeyframe(
                Transform{
                    glm::vec3(11.0f, 0.0f, -15.0f),
                    ft2Current.rotation,
                    ft2Current.scale
                },
                5.0f,
                Easing::Linear
            );
        fire2Anim.play(ft2Current);
    }
    // 5. FIRE TRUCKS - Turn toward pump/crash site
    void setupFireTrucksTurnAnimation() {
        // Fire Truck 1 - Turn
        auto FT1 = ecs->GetTransformRef(fire_truck1);
        Transform ft1Current;
        ft1Current.position = FT1.GetPosition();
        ft1Current.rotation = FT1.GetRotation();
        ft1Current.scale = FT1.GetScale();
        fire1Anim
            .addKeyframe(
                Transform{
                    glm::vec3(-2.0f, 0.0f, -16.5f),
                    glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)), // Face forward (0 rotation)
                    ft1Current.scale
                },
                6.0f,
                Easing::Linear,
                []() {
                    // Turn complete, move to pump
                    setupFireTrucksFinalMove();
                }
        );
        fire1Anim.play(ft1Current);
        // Fire Truck 2 - Turn
        auto FT2 = ecs->GetTransformRef(fire_truck2);
        Transform ft2Current;
        ft2Current.position = FT2.GetPosition();
        ft2Current.rotation = FT2.GetRotation();
        ft2Current.scale = FT2.GetScale();
        fire2Anim
            .addKeyframe(
                Transform{
                    glm::vec3(9.5f, 0.0f, -16.5f),
                    glm::angleAxis(glm::radians(-225.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    ft2Current.scale
                },
                5.0f,
                Easing::Linear
            );
        fire2Anim.play(ft2Current);
    }
    // 6. FIRE TRUCKS - Final move to pump
    void setupFireTrucksFinalMove() {
        // Fire Truck 1 - Move to pump
        auto FT1 = ecs->GetTransformRef(fire_truck1);
        Transform ft1Current;
        ft1Current.position = FT1.GetPosition();
        ft1Current.rotation = FT1.GetRotation();
        ft1Current.scale = FT1.GetScale();
        fire1Anim
            .addKeyframe(
                Transform{
                    glm::vec3(4.0f, 0.0f, -16.5f),
                    glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)),
                    ft1Current.scale
                },
                6.0f,
                Easing::Linear
                // FIRE_TRUCK_STOP - Animation complete, trucks at pump
            );
        fire1Anim.play(ft1Current);
        // Fire Truck 2 - Move to position
        auto FT2 = ecs->GetTransformRef(fire_truck2);
        Transform ft2Current;
        ft2Current.position = FT2.GetPosition();
        ft2Current.rotation = FT2.GetRotation();
        ft2Current.scale = FT2.GetScale();
        fire2Anim
            .addKeyframe(
                Transform{
                    glm::vec3(8.0f, 0.0f, -18.0f),
                    glm::angleAxis(glm::radians(-225.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    ft2Current.scale
                },
                5.0f,
                Easing::Linear
            );
        fire2Anim.play(ft2Current);
    }
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
        auto cameraT= ecs->GetTransformRef(camera);
        cameraT.SetPosition({ 20,0.32,23.75 });
        // Load and instantiate our 3D model
        Ref<Model> model = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/car_expo.glb"));
        car = ecs->Instantiate(null, Component::Transform(), model);
        auto carT = ecs->GetTransformRef(car);
        carT.SetPosition({ 20.0f,0.0f,22.5f });
        carT.SetRotation(glm::angleAxis(1.5708f, glm::vec3({ 0,1,0 })));
        carT.SetScale(glm::vec3(0.5, 0.5, 0.5));
        
        Ref<Model> model1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/tank.glb"));
        truck = ecs->Instantiate(null, Component::Transform(), model1);
        auto truckT = ecs->GetTransformRef(truck);
        truckT.SetPosition({ 5.0f, 0.0f, -19.0f });
        truckT.SetScale({ 0.25f,0.25f,0.25f });
        Ref<Model> model_fire = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/fire.glb"));
        fire = ecs->Instantiate(null, Component::Transform(), model_fire);
        Ref<Model> modelpolice = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Police Car.glb"));
        police = ecs->Instantiate(null, Component::Transform(), modelpolice);
        auto policeT = ecs->GetTransformRef(police);
        policeT.SetPosition({ 8.0f, 0.05f, 9.0f });
        policeT.SetScale({ 0.3f,0.3f,0.3f });
        policeT.SetRotation(glm::angleAxis(2 * 1.5708f, glm::vec3({ 0,1,0 })));
        Ref<Model> model_car2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/red_car.glb"));
        car2 = ecs->Instantiate(null, Component::Transform(), model_car2);
        auto car2T = ecs->GetTransformRef(car2);
        car2T.SetPosition({ 8.0f,0.1f,7.0f });
        car2T.SetScale({ 0.5f,0.5f,0.5f });
        Ref<Model> model_firetruck1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Fire Truck.glb"));
        fire_truck1 = ecs->Instantiate(null, Component::Transform(), model_firetruck1);
        auto Ft1 = ecs->GetTransformRef(fire_truck1);
        Ft1.SetPosition({ -4.0f, 0.0f, -31.0f }); // ← was 0.0f → keep 0.0f (already good)
        Ft1.SetScale({ 1.5f,1.5f,1.5f });
        Ft1.SetRotation(glm::angleAxis(-1.5708f, glm::vec3({ 0,1,0 })));
        Ref<Model> model_firetruck2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Fire Truck.glb"));
        fire_truck2 = ecs->Instantiate(null, Component::Transform(), model_firetruck2);
        auto Ft2 = ecs->GetTransformRef(fire_truck2);
        Ft2.SetPosition({ 20.0f, 0.0f, -15.0f }); // ← change from 0.0f → still 0.0f (or was 0.0f already)
        Ft2.SetScale({ 1.5f,1.5f,1.5f });
        Ft2.SetRotation(glm::angleAxis(-1.5708f * 2, glm::vec3({ 0,1,0 })));
     
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
   
        fireExplosion.init(model_fire, ecs);
        city.shader = shader;
        city.initializeModels();
        city.generate();
        setupCameraIntroAnimation();
        
        // Initialize particle system with an unlit shader from engine assets
    }
    void scene_render() {
        city.shader->Enable();
        glm::mat4 viewMatrix = camComp->viewMatrix;
    }
    void scene_shutdown() {
    }
    void scene_update_fixed(float deltaTime) {
        return;
    }
}
