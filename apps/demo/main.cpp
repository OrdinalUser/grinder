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

entity_id car = null, camera = null, big_H = null, small_H = null, grass = null, road = null, pummp = null,
truck = null, police = null, firetruck = null , city_parent= null, tree=null;
Camera* camComp = nullptr;
Ref<ECS> ecs;
Ref<VFS> vfs;
class Cube {
public:
    // Cube vertices (corner points)
    std::vector<glm::vec3> vertices = {
        {-1,   -1,   -1},   // 0 - bottom left front
        {1, -1,   -1},   // 1 - bottom right front
        {1, 1, -1},   // 2 - top right front
        {-1,   1, -1},   // 3 - top left front
        {-1,   -1,   1}, // 4 - bottom left back
        {1, -1,   1}, // 5 - bottom right back
        {1, 1, 1}, // 6 - top right back
        {-1,   1, 1}  // 7 - top left back
    };

    // Structure representing a triangular face
    struct Face {
        GLuint v0, v1, v2;
    };

    // Cube faces (each face has 2 triangles)
    std::vector<Face> indices = {
        // Front face (z = 0)
        {0, 1, 2}, {2, 3, 0},

        // Back face (z = 0.1)
        {4, 5, 6}, {6, 7, 4},

        // Left face (x = 0)
        {0, 3, 7}, {7, 4, 0},

        // Right face (x = 0.1)
        {1, 5, 6}, {6, 2, 1},

        // Top face (y = 0.1)
        {3, 2, 6}, {6, 7, 3},

        // Bottom face (y = 0)
        {0, 4, 5}, {5, 1, 0}
    };
    // Program to associate with the object
    Ref<Shader> shader;

    // These will hold the data and object buffers
    GLuint vao, vbo, cbo, ibo;
    glm::mat4 modelMatrix{ 1.0f };

public:
    // Public attributes that define position, color ..
    glm::vec3 position{ 0,0,0 };
    glm::vec3 rotation{ 0,0,0 };
    glm::vec3 scale{ 1,1,1 };
    glm::vec3 color{ 1,0,0 };


    // Initialize object data buffers
    Cube() : shader(nullptr) {
    }
    // Clean up
    ~Cube() {
        // Delete data from OpenGL
        glDeleteBuffers(1, &ibo);
        glDeleteBuffers(1, &cbo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    void init() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // Positions
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        // Indices (flatten faces into raw indices)
        std::vector<GLuint> flatIndices;
        flatIndices.reserve(indices.size() * 3);
        for (const auto& f : indices) {
            flatIndices.push_back(f.v0);
            flatIndices.push_back(f.v1);
            flatIndices.push_back(f.v2);
        }

        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, flatIndices.size() * sizeof(GLuint), flatIndices.data(), GL_STATIC_DRAW);

        // Projection matrix setup
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
        shader->Enable();
        shader->SetUniform("ProjectionMatrix", projection);

        // Default view and model
        auto mat = glm::mat4(1.0f);
        shader->SetUniform("ViewMatrix", mat);
        shader->SetUniform("ModelMatrix", mat);
    }

    // Set the object transformation matrix
    void updateModelMatrix(glm::mat4 parent_matrix = glm::mat4(1.0f)) {
        // Compute transformation by scaling, rotating and then translating the shape

        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3{ 0,1,0 });
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

         modelMatrix = parent_matrix * T * R * S;

    }

    // Draw polygons
    void render( glm::mat4& viewMatrix ) {
        // Update GPU uniforms
        shader->Enable();
        shader->SetUniform("ModelMatrix", modelMatrix);
        shader->SetUniform("ViewMatrix", viewMatrix);
        shader->SetUniform("OverallColor", color);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)(indices.size() * 3), GL_UNSIGNED_INT, 0);
    }
};
class City {
public:
    std::vector<std::unique_ptr<Cube>> objects;
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
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 4, 4, 4, 4, 8, 8, 6, 6, 8, 6, 6, 8, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 1, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 4, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 1, 4, 1, 0, 2, 2, 0, 8, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 6, 6, 6, 8, 6, 6, 8, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 5, 5, 5, 5, 5, 5, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 5, 5, 5, 5, 5, 5, 5, 1, 0, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
    {2, 0, 2, 4, 2, 0, 2, 4, 1, 0, 1, 5, 4, 4, 4, 4, 4, 5, 1, 7, 5, 4, 4, 4, 4, 5, 0, 4, 4, 4},
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
                if (tile == 1 && bigModel1 ) {
                    // Instantiate big building model centered on this tile, occupying 3x3
                    Ref<Model> bigModel = randomChoice(bigModels);

                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel1);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(2,3);
                    // Optionally scale the model to roughly cover 3x3 tiles
                    ref.SetScale({ tileSize/1.1,dis(gen),tileSize / 1.1});
                    
                    
                    continue;
                }
                else if (tile == 2 && bigModel4) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel4);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.75f,  (tileSize / 2.0f) });

                   
                    continue;
                }else if (tile ==4 && grassModel && trees) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), grassModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 0.05f,  (tileSize / 2.0f) });
                    entity_id i = ecs->Instantiate(e, Component::Transform(), trees);
                    auto ref1 = ecs->GetTransformRef(i);
                   // ref1.SetPosition({ worldX, 0.0f, worldZ });
                    ref1.SetScale({ tileSize/2 , 20.0f,  tileSize/2 });


                    continue;
                }
                else if (tile == 0 && roadModel){
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({  (tileSize / 8.0f), 0.05f,  (tileSize / 8.0f) });
                    continue;
                }
                else if (tile == 6 && roadModel){
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), roadModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetRotation(glm::angleAxis(-1.5708f, glm::vec3(0.0f, 1.0f, 0.0f)));
                    ref.SetScale({  (tileSize / 8.0f), 0.05f,  (tileSize / 8.0f) });
                    continue;
                }
                else if (tile == 8 && cross){
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), cross);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({  (tileSize / 8.0f), 0.05f,  (tileSize / 8.0f) });
                    continue;
                }
                else if (tile == 3 && pummpModel) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), pummpModel);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize*3 / 2.0f), 0.74f,  (tileSize*3 / 2.0f) });
                    continue;
                }
                else if (tile == 5 && bigModel3) {
                    // Instantiate small building model centered on this tile, occupying 3x3
                    entity_id e = ecs->Instantiate(city_parent, Component::Transform(), bigModel3);
                    auto ref = ecs->GetTransformRef(e);
                    ref.SetPosition({ worldX, 0.0f, worldZ });
                    ref.SetScale({ (tileSize / 2.0f), 1.0f, (tileSize / 2.0f) });
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
                // Fallback: create cube for other tiles
                auto obj = std::make_unique<Cube>();
                obj->shader = shader;
                obj->init();
                if (tile == 5) {
                    obj->position = { worldX, 0.1f, worldZ };
                    obj->scale = { tileSize / 2.0f,  1.0f, tileSize / 2.0f };
                    obj->color = { 0.8,0.6,0.4 };
                }
                else if (tile == 3) {
                    // Gas pump
                    obj->position = { worldX, 0.75f, worldZ };
                    obj->scale = { tileSize / 2.0f, 0.75f, tileSize / 2.0f };
                    obj->color = { 1.0f, 0.8f, 0.0f };
                }
                else if (tile == 1) {
                    // Big Building
                    obj->position = { worldX, 0.0f, worldZ };  // y = half height to sit on ground
                    obj->scale = { tileSize / 2.0f, tileSize / 2.0f, tileSize / 2.0f };
                    obj->color = { 0.6,0.6,0.6 };//{colorDist(gen), colorDist(gen) * 0.8f, colorDist(gen) * 0.9f};
                }
                else if (tile == 2) {
                    // Small house
                    obj->position = { worldX, 2.0f, worldZ };  // y = half height
                    obj->scale = { tileSize / 2.0f, 2.0f, tileSize / 2.0f };
                    obj->color = { 0.1,0.3,0.9 };//{colorDist(gen), colorDist(gen) * 0.8f, colorDist(gen) * 0.9f};
                }
                else if (tile == 4) {
                    // Grass
                    obj->position = { worldX, 0.025f, worldZ };
                    obj->scale = { tileSize / 2.0f, 0.025f, tileSize / 2.0f };
                    obj->color = { 0.0f, 0.8f, 0.0f };
                }
                else if (tile == 0) {
                    // Road
                    obj->position = { worldX, 0.01f, worldZ };
                    obj->scale = { tileSize / 2.0f, 0.01f, tileSize / 2.0f };
                    obj->color = { 0.3f, 0.3f, 0.3f };
                }
                else if (tile == 7) {
                    obj->position = { worldX, 0.01f, worldZ };
                    obj->scale = { tileSize / 2.0f, 0.01f, tileSize / 2.0f };
                    obj->color = { 0.3f, 0.3f, 0.3f };
                    std::cout << worldX << "|" << worldZ << std::endl;
                }
                else continue;
                
                obj->updateModelMatrix();
                objects.push_back(std::move(obj));
                count++;
            }
        }
    }

    void render( glm::mat4& viewMatrix) {
        for (auto& o : objects)
            o->render(viewMatrix);
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
    STOPPED
};
enum cameraMode {
    CAMERA_FOLLOW,
    TRACKING,
    SPIN
};
static cameraMode camMode = TRACKING;
static CarState carState = MOVE_LEFT;
static float carYaw = 0.0f; // radians
static float carSpeed = 4.0f; // world units per second
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
        carT.SetRotation(glm::angleAxis(1.5708f,glm::vec3({0,1,0})));
        carT.SetScale(glm::vec3(0.5, 0.5, 0.5));

        Ref<Model> model1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/tank.glb"));
        truck = ecs->Instantiate(null, Component::Transform(), model1);
        auto truckT = ecs->GetTransformRef(truck);
        truckT.SetPosition({ 5.0f, 0.0f, -19.0f });
        truckT.SetScale({ 0.25f,0.25f,0.25f });
        
        Ref<Model> modelpolice = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Police Car.glb"));
        police = ecs->Instantiate(null, Component::Transform(), modelpolice);
        auto policeT = ecs->GetTransformRef(police);
        policeT.SetPosition({ 8.0f, 0.0f, 9.0f });
        policeT.SetScale({ 0.25f,0.25f,0.25f });
        //policeT.SetRotation(glm::angleAxis(1.5708f, glm::vec3({ 0,1,0 })));
    // Load building models (if present) and assign to city
   
    Ref<Model> cityModel = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H1.glb"), LoadCfg::Model{.static_mesh=true});
    city.bigModel1 = cityModel;
    
    Ref<Model> cityModel1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H2.glb"), LoadCfg::Model{.static_mesh=true});
    
    city.bigModel2 = cityModel1;

    Ref<Model> cityModel2 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H3.glb"), LoadCfg::Model{.static_mesh=true});
    
    city.bigModel3 = cityModel2;

    Ref<Model> cityModel3 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/big_H4.glb"), LoadCfg::Model{.static_mesh=true});
    
    city.bigModel4 = cityModel3;

        Ref<Model> grass = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/grass.glb"));
    city.grassModel = grass;

        Ref<Model> road = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/road.glb"));
        city.roadModel = road;

        Ref<Model> road1 = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/cross.glb"));
        city.cross = road1;

        Ref<Model> small_H = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/small_b.glb"), LoadCfg::Model{.static_mesh=true});
    city.smallModel = small_H;

        Ref<Model> pummp = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/gas_pump.glb"), LoadCfg::Model{.static_mesh=true});
        city.pummpModel = pummp;
        Ref<Model> tree = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/tree.glb"), LoadCfg::Model{ .static_mesh = true });
        city.trees = tree;
        // Create parent entity for all city objects
       
    city.shader = shader;
    city.initializeModels();
    city.generate();
}
    

    void scene_update(float deltaTime) {
        // Get car transform
        auto carTransform = ecs->GetTransformRef(car);
        glm::vec3 carPos = carTransform.GetPosition();
        glm::quat carRot = carTransform.GetRotation();
        auto policeT = ecs->GetTransformRef(police);
        glm::vec3 polpos = policeT.GetPosition();
        // Extract car's Y-axis rotation (yaw)
        glm::vec3 carEuler = glm::eulerAngles(carRot);
        float carYaw = carEuler.y;
        switch (camMode) {
            case CAMERA_FOLLOW: {
                // Smooth rotation interpolation
                smoothCarRotation = glm::mix(smoothCarRotation, carYaw, 0.1f);

                // Camera follows behind the car
                float distance = 0.01f;  // distance behind car
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
           camComp->LookAt(smoothCamPos, carTransform.Forward()+carPos);
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
                camComp->LookAt(spinCamPos, glm::vec3{8,0,-18});
                break;
            }
        }
        // Rotate car wheels
        constexpr float WHEEL_ROTATION_SPEED = 5.0f;
 

        // Car movement state machine
        {
            glm::vec3 pos = carTransform.GetPosition();
            glm::vec3 polpos = policeT.GetPosition();
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
                    //camMode = SPIN;
                }
                break;
            case STOPPED:
                polpos.z-= 10.0f * deltaTime;
                carYaw = -1.5708f;
                break;
            }
            policeT.SetPosition(polpos);
            carTransform.SetPosition(pos);
            carTransform.SetRotation(glm::angleAxis(carYaw, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
    }

    void scene_render() {
        city.shader->Enable();
        
        // Get the camera's view matrix from the ECS camera component
        glm::mat4 viewMatrix = camComp->viewMatrix;
        
        // Render city with the camera's view matrix
        city.render(viewMatrix);
    }

    void scene_shutdown() {

    }

    void scene_update_fixed(float deltaTime) {
        return;
    }
}