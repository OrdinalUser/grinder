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
    bool isFountain;               // Enable continuous spawning mode
    glm::vec3 fountainPosition;    // Where to spawn new particles
    float spawnTimer;              // Timer for continuous spawning
    float spawnInterval;           // Seconds between spawn batches
    int particlesPerBatch;         // How many particles to spawn per batch
    
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
        isFountain = false;  // Disable fountain mode
        
        hasExploded = true;
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> velDist(-8.0f, 8.0f);   // Random XZ velocity
        std::uniform_real_distribution<float> upDist(5.0f, 15.0f);    // Strong upward velocity
        std::uniform_real_distribution<float> lifeDist(1.5f, 4.0f);   // Particle lifetime
        
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
        cleanup();  // Clear old particles
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
        std::uniform_real_distribution<float> velDist(-3.0f, 3.0f);   // Small random XZ spread
        std::uniform_real_distribution<float> upDist(8.0f, 12.0f);    // Upward velocity for fountain
        std::uniform_real_distribution<float> lifeDist(2.0f, 3.5f);   // Particle lifetime
        
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
// Add these classes BEFORE the City class
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

entity_id car = null, camera = null, big_H = null, small_H = null, grass = null, road = null, pummp = null,
truck = null, police = null, firetruck = null, city_parent = null, tree = null,car2=null,fire=null,fire_truck1 = null, fire_truck2=null;
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

// Car animation/tween helper
class CarAnimator {
public:
    struct TweenState {
        Transform startTransform;
        Transform endTransform;
        float duration;
        float elapsed;
        Easing::Func easing;
        bool isActive;
        
        TweenState() : duration(0), elapsed(0), easing(Easing::Linear), isActive(false) {}
    };
    
    TweenState tweenState;
    
    void animateTo(const Transform& currentTransform, const Transform& target, float duration, Easing::Func easing = Easing::OutCubic) {
        // Vždy použi aktuálnu pozíciu ako štart
        tweenState.startTransform = tweenState.isActive ? tweenState.endTransform : currentTransform;
        tweenState.endTransform = target;
        tweenState.duration = duration;
        tweenState.elapsed = 0.0f;
        tweenState.easing = easing;
        tweenState.isActive = true;
    }
    
    Transform update(const Transform& currentTransform, float deltaTime) {
        if (!tweenState.isActive) {
            startTransform = currentTransform;  // ← Aktualizuj startTransform
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

        if (tweenState.elapsed >= tweenState.duration) {
            tweenState.isActive = false;
            startTransform = tweenState.endTransform;  // ← Ulož poslednú pozíciu
        }

        return result;
    }
    
    bool isAnimating() const { return tweenState.isActive; }
    
private:
    Transform startTransform;
};

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
    FIRE_TRUCK_TURN,
    FIRE_TRUCK_MOVE_TO_PUMP,
    FIRE_TRUCK_STOP
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
static float carSpeed = 4.0f; // world units per second
auto spinAngle = 0.0f;
FireModelExplosion fireExplosion;//aaaaaaaaaaaaaaaa
CarAnimator carAnimator;

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
        policeT.SetRotation(glm::angleAxis(2*1.5708f, glm::vec3({ 0,1,0 })));

        Ref<Model> model_car2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/red_car.glb"));
        car2 = ecs->Instantiate(null, Component::Transform(), model_car2);
        auto car2T = ecs->GetTransformRef(car2);
        car2T.SetPosition({ 8.0f,0.1f,7.0f });
        car2T.SetScale({ 0.5f,0.5f,0.5f });

        Ref<Model> model_firetruck1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Fire Truck.glb"));
        fire_truck1 = ecs->Instantiate(null, Component::Transform(), model_firetruck1);
        auto Ft1 = ecs->GetTransformRef(fire_truck1);
        Ft1.SetPosition({ -4.0f,0.5f,-31.0f });
        Ft1.SetScale({ 1.5f,1.5f,1.5f });
        Ft1.SetRotation(glm::angleAxis(-1.5708f, glm::vec3({ 0,1,0 })));

        Ref<Model> model_firetruck2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Fire Truck.glb"));
        fire_truck2 = ecs->Instantiate(null, Component::Transform(), model_firetruck2);
        auto Ft2 = ecs->GetTransformRef(fire_truck2);
        Ft2.SetPosition({ 20.0f,0.5f,-15.0f });
        Ft2.SetScale({ 1.5f,1.5f,1.5f });
        Ft2.SetRotation(glm::angleAxis(-1.5708f*2, glm::vec3({ 0,1,0 })));
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
        
        fireExplosion.init(model_fire, ecs);
        city.shader = shader;
        city.initializeModels();
        city.generate();

        // Initialize particle system with an unlit shader from engine assets

    }


    void scene_update(float deltaTime) {
        
        fireExplosion.update(deltaTime);

        auto carTransform = ecs->GetTransformRef(car);
        glm::vec3 carPos = carTransform.GetPosition();

        auto FT1 = ecs->GetTransformRef(fire_truck1);
        auto FT2 = ecs->GetTransformRef(fire_truck2);

        auto car2T = ecs->GetTransformRef(car2);
        glm::vec3 car2Pos = car2T.GetPosition();

        auto policeT = ecs->GetTransformRef(police);
        glm::vec3 polpos = policeT.GetPosition();

        // --- Persistent state for fire trucks ---
        static glm::vec3 F1pos = glm::vec3(-4.0f, 0.5f, -31.0f);
        static glm::vec3 F2pos = glm::vec3(20.0f, 0.5f, -15.0f);
        static float F1yaw = -1.5708f;        // facing +Z (north)
        static float F2yaw = -3.14159f;        // facing -Z (south) = 180°

        static float carYaw2 = 0.0f;

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
            auto cameraTransform = ecs->GetTransformRef(camera);
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
        }
        // Rotate car wheels
        constexpr float WHEEL_ROTATION_SPEED = 5.0f;

        glm::vec3 pos = carTransform.GetPosition();
        
        // polpos already declared above and used by camera logic
        // Car movement state machine with tweened animations
        

            // Nájdi túto časť v scene_update a nahraď ju:
      //  std::cout << pos.x << "||" << pos.z << std::endl;
switch (carState) {
    case MOVE_LEFT:
        if (!carAnimator.isAnimating()) {
            Transform current;
            current.position = carTransform.GetPosition();
            current.rotation = carTransform.GetRotation();
            current.scale = carTransform.GetScale();

            Transform target;
            target.position = glm::vec3(-11.5f, pos.y, pos.z);
            target.rotation = glm::angleAxis(1.5708f, glm::vec3(0.0f, 1.0f, 0.0f));
            target.scale = glm::vec3(0.5f, 0.5f, 0.5f);

            carAnimator.animateTo(current, target, 4.0f, Easing::InOutQuad);  // ← Pridaj current
            carState = FIRST_TURN;
        }
        
        break;
    case FIRST_TURN:
        if (!carAnimator.isAnimating()) {
            // Rotation animation
            Transform current;
            current.position = carTransform.GetPosition();
            current.rotation = carTransform.GetRotation();
            current.scale = carTransform.GetScale();
            Transform target;
            target.position = glm::vec3(-12.0f, pos.y, 20);
            target.rotation = glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            target.scale = glm::vec3(0.5f, 0.5f, 0.5f);
            // ZMENENÉ: z 0.5f na 1.5f (pomalšia rotácia)
            carAnimator.animateTo(current,target, 1.5f, Easing::InOutQuad);
            carState = MOVING_FORWARD;
            }
        break;
    case MOVING_FORWARD:
        camMode = CAMERA_FOLLOW;
        if (!carAnimator.isAnimating()) {
            Transform target;
            Transform current;
            current.position = carTransform.GetPosition();
            current.rotation = carTransform.GetRotation();
            current.scale = carTransform.GetScale();
            target.position = glm::vec3(pos.x, pos.y, -16.5f);
            target.rotation = glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            target.scale = glm::vec3(0.5f, 0.5f, 0.5f);
            // ZMENENÉ: z 2.0f na 5.0f (pomalší pohyb dopredu)
            carAnimator.animateTo(current, target, 10.0f, Easing::InOutQuad);
            carState = TURNING;
        }
        break;
    case TURNING:
        if (!carAnimator.isAnimating()) {
            // Rotate 90 degrees
            Transform target;
            Transform current;
            current.position = carTransform.GetPosition();
            current.rotation = carTransform.GetRotation();
            current.scale = carTransform.GetScale();
            target.position = glm::vec3(-10.0f, pos.y, -17.0f);
            target.rotation = glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f));
            target.scale = glm::vec3(0.5f, 0.5f, 0.5f);
            // ZMENENÉ: z 0.5f na 1.5f (pomalšia rotácia)
            carAnimator.animateTo(current, target, 1.5f, Easing::InOutQuad);
            carState = TURNING_RIGHT;
        }
        break;
    case TURNING_RIGHT:
        if (!carAnimator.isAnimating()) {
            Transform target;
            Transform current;
            current.position = carTransform.GetPosition();
            current.rotation = carTransform.GetRotation();
            current.scale = carTransform.GetScale();
            target.position = glm::vec3(2.0f, pos.y, pos.z);
            target.rotation = glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f));
            target.scale = glm::vec3(0.5f, 0.5f, 0.5f);
            // ZMENENÉ: z 1.5f na 4.0f (pomalší pohyb doprava)
            carAnimator.animateTo(current, target, 4.0f, Easing::InOutQuad);
            
        }
        if (pos.x == 2) {
            carState = STOPPED;
        }
        break;
    case STOPPED:
        // ZMENENÉ: zmenšená rýchlosť polície a car2 (z 2*carSpeed na 1.5*carSpeed)
        polpos.z -= 1.5f * carSpeed * deltaTime;
        car2Pos.z -= 1.5f * carSpeed * deltaTime;
        camMode = ASCEND;
        if (car2Pos.z < -15) {
            polpos.z = -15;
            carState = STOP_SECOND;
            camMode = TRACK_ESCAPING;
        }
        break;
    case STOP_SECOND:
        carYaw2 = 1.5708f / 2;
        // ZMENENÉ: zmenšená rýchlosť (z carSpeed na 0.7*carSpeed)
        car2Pos.x -= 0.7f * carSpeed * deltaTime;
        car2Pos.z -= 0.7f * carSpeed * deltaTime;
        if (car2Pos.z < -18.5) {
            camMode = SPIN;
            carState = CRASH;
        }
        break;
    case CRASH:
        
        if (F1pos.z < -17.0f) {
            F1pos.z += carSpeed * 1.2f * deltaTime;
            if (F1pos.z > -17.0f) F1pos.z = -17.0f;
        }

        // FT2 comes from right (x = 20 → 10), then will turn later
        if (F2pos.x > 10.0f) {
            F2pos.x -= carSpeed * 1.2f * deltaTime;
            if (F2pos.x < 10.0f) F2pos.x = 10.0f;
        }

        // Start fire
        if (!fireExplosion.hasExploded) {
            fireExplosion.startFountain(glm::vec3(5.0f, 0.5f, -19.0f), 0.006f, 50);
        }

        // Both arrived → start turning
        if (F1pos.z >= -17.0f && F2pos.x <= 10.0f) {
            carState = FIRE_TRUCK_TURN;
        }
        break;

    case FIRE_TRUCK_TURN:
    {
        const float turnSpeed = 1.6f;

        // FT1 (left truck): turn right → from -90° to 0°
        if (F1yaw < 0.0f) {
            F1yaw += turnSpeed * deltaTime;
            if (F1yaw > 0.0f) F1yaw = 0.0f;
        }

        // FT2 (right truck): turn left + down → from 180° to 135° (-135° or -3π/4)
        float targetYaw = glm::radians(-225.0f); // -135 degrees
        if (F2yaw > targetYaw) {
            F2yaw -= turnSpeed * deltaTime;
            if (F2yaw < targetYaw) F2yaw = targetYaw;
        }

        if (F1yaw >= 0.0f && F2yaw <= targetYaw) {
            F1yaw = 0.0f;
            F2yaw = targetYaw;
            carState = FIRE_TRUCK_MOVE_TO_PUMP;
        }
        break;
    }

    case FIRE_TRUCK_MOVE_TO_PUMP:
    {
        const float moveSpeed = carSpeed * 0.9f;

        // FT1 moves right to x = 2
        if (F1pos.x < 2.0f) {
            F1pos.x += moveSpeed * deltaTime;
            if (F1pos.x > 2.0f) F1pos.x = 2.0f;
        }

        // FT2 moves diagonally left + down to (8, 0.5, -15)
        glm::vec3 target2(6.0f, 0.5f, -17.0f);
        glm::vec3 dir = target2 - F2pos;
        if (glm::length(dir) > 0.1f) {
            dir = glm::normalize(dir);
            F2pos += dir * moveSpeed * deltaTime;
        }
        else {
            F2pos = target2;
        }

        if (F1pos.x >= 2.0f && glm::distance(F2pos, target2) < 0.5f) {
            F1pos = glm::vec3(2.0f, 0.5f, -17.0f);
            F2pos = target2;
            F1yaw = 0.0f;
            F2yaw = -2.35619f;
            carState = FIRE_TRUCK_STOP;
            camMode = SPIN;
            spinAngle = 0.0f;
        }
        break;
    }

    case FIRE_TRUCK_STOP:
        // Final locked positions – heroic stance in front of the burning tanker
        // Final pose
        F1pos = glm::vec3(2.0f, 0.5f, -17.0f);
        F2pos = glm::vec3(6.0f, 0.5f, -17.0f);
        F1yaw = 0.0f;
        F2yaw = glm::radians(-225.0f); // -135°
        break;
        break;
}
        
    Transform carCurrent = { carTransform.GetPosition(), carTransform.GetRotation(), carTransform.GetScale() };
    Transform animated = carAnimator.update(carCurrent, deltaTime);
    carTransform.SetPosition(animated.position);
    carTransform.SetRotation(animated.rotation);

    FT1.SetPosition(F1pos);
    FT1.SetRotation(glm::angleAxis(F1yaw, glm::vec3(0, 1, 0)));

    FT2.SetPosition(F2pos);
    FT2.SetRotation(glm::angleAxis(F2yaw, glm::vec3(0, 1, 0)));

    car2T.SetPosition(car2Pos);
    car2T.SetRotation(glm::angleAxis(carYaw2, glm::vec3(0, 1, 0)));
    policeT.SetPosition(polpos);
    }

    void scene_render() {
        city.shader->Enable();

        // Get the camera's view matrix from the ECS camera component
        glm::mat4 viewMatrix = camComp->viewMatrix;

        // Render city with the camera's view matrix
        // Use the camera's projection matrix so particles use the same projection as the renderer
   
    }

    void scene_shutdown() {

    }

    void scene_update_fixed(float deltaTime) {
        return;
    }
}