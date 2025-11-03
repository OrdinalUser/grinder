// Car demo

#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>

#include <engine/types.hpp>
#include <engine/application.hpp>
#include <engine/ecs.hpp>

using namespace Engine;
using namespace Engine::Component;

entity_id car = null, camera = null;
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
    glm::mat4 viewMatrix{ 1.0f };

public:
    // Public attributes that define position, color ..
    glm::vec3 position{ 0,0,0 };
    glm::vec3 rotation{ 0,0,0 };
    glm::vec3 scale{ 1,1,1 };
    glm::vec3 color{ 1,0,0 };


    // Initialize object data buffers
    Cube() {
    };
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
        shader->SetUniform("ViewMatrix",mat);
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

    void updateViewMatrix(glm::vec3 viewRotation, glm::vec3 targetPosition = glm::vec3(0, 0, 0), float carRotationZ = 0.0f) {
        float distance = 15.0f;   // distance behind car
        float height = 8.0f;    // height above car

        // Smooth rotation interpolation
        static float smoothRotation = carRotationZ;
        smoothRotation = glm::mix(smoothRotation, carRotationZ, 0.1f);

        // Camera offset in car's local space (behind the car)
        glm::vec3 localOffset = glm::vec3(
            sin(smoothRotation) * distance,
            height,
            cos(smoothRotation) * distance
        );

        glm::vec3 cameraPos = targetPosition + localOffset;
        glm::vec3 up = { 0.0f, 1.0f, 0.0f };

        // Smooth position follow
        static glm::vec3 smoothCam = cameraPos;
        smoothCam = glm::mix(smoothCam, cameraPos, 0.1f);

        // Smooth target follow
        static glm::vec3 smoothTarget = targetPosition;
        smoothTarget = glm::mix(smoothTarget, targetPosition, 0.1f);

        viewMatrix = glm::lookAt(smoothCam, smoothTarget, up);
    }


    // Draw polygons

    void render() {
        // Update GPU uniforms
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
    City() {}

    void generate() {
        // R = 0 (Road), S = 2 (Small Building), B = 1 (Big Building), G = 4 (Grass), P = 3 (Gas Pump), T = 5 (Trees)
        std::vector<std::vector<int>> cityMap = {
            {1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {4,4,4,4,4,0,0,4,4,4,0,0,4,4,4,4,4,0,4,4,4,4,4,4,0,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,31,31,31,32,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,4,0,4,31,31,31,32,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,32,32,32,32,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,32,32,32,32,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,1,1,1,4,0,4,4,1,1,1,4,0,4,4,4,4,0,0,7,7,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,4,4,4,4,0,4,4,4,4,4,4,0,4,4,4,4,0,0,7,7,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,0,4,4,4,0,4,4,0,4,4,4,0,4,4,4,4,0,4,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,4,0,4,4,4,0,4,4,0,4,4,4,0,4,4,4,4,0,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,0,4,4,4,0,4,4,0,4,4,4,0,4,4,4,4,0,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,5,4,5,4,5,4,5,5,4,5,4,5,4,5,5,4,5,4,5,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,0,4,4,4,0,4,4,0,4,4,4,0,4,4,4,4,0,4,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,5,4,5,4,5,4,5,5,4,5,4,5,4,5,5,4,5,4,5,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,1,1,1,4,1,1,1,1,1,1,4,1,1,1,1,4,1,1,1,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,4,0,4,4,4,0,4,4,0,4,4,4,0,4,4,4,4,0,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,2,2,2,0,0,0,2,2,2,0,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,2,2,2,4,0,4,2,2,2,0,0,5,4,5,4,5,4,5,5,4,5,4,5,4,5,5,4,5,4,5,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,0,0,4,4,4},
            {0,1,1,1,4,4,2,2,2,4,0,0,1,1,1,0,4,2,2,4,1,1,1,4,0,4,4,2,2,2,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,1,1,1,4,4,2,2,2,4,0,0,1,1,1,0,4,2,2,4,1,1,1,4,0,4,4,2,2,2,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4},
            {0,1,1,1,4,4,2,2,2,4,0,0,1,1,1,0,4,4,4,4,1,1,1,4,0,4,4,2,2,2,4,4,0,0,1,1,1,4,4,4,0,0,4,4,4}
        };

        float tileSize = 2.0f;
        int count = 0;
        
        int rows = cityMap.size();
        int cols = cityMap[0].size();

        for (int z = 0; z < rows; z++) {
            for (int x = 0; x < cols; x++) {
                int tile = cityMap[z][x];
                auto obj = std::make_unique<Cube>();
                obj->shader = shader;
                obj->init();
                // Center the grid around origin (0,0)
                float worldX = (x - cols / 2.0f) * tileSize;
                float worldZ = (z - rows / 2.0f) * tileSize;

                if (tile == 1) {
                    // Big Building
                    obj->position = { worldX, 5.0f, worldZ };  // y = half height to sit on ground
                    obj->scale = { tileSize / 2.0f, 5.0f, tileSize / 2.0f };
                    obj->color = { 0.6,0.6,0.6 };//{colorDist(gen), colorDist(gen) * 0.8f, colorDist(gen) * 0.9f};
                }
                else if (tile == 2) {
                    // Small house
                    obj->position = { worldX, 2.0f, worldZ };  // y = half height
                    obj->scale = { tileSize / 2.0f, 2.0f, tileSize / 2.0f };
                    obj->color = { 0.1,0.3,0.9 };//{colorDist(gen), colorDist(gen) * 0.8f, colorDist(gen) * 0.9f};
                }
                else if (tile == 5) {
                    obj->position = { worldX, 0.1f, worldZ };
                    obj->scale = { tileSize / 2.0f,  1.0f, tileSize / 2.0f };
                    obj->color = { 0.8,0.6,0.4 };
                }
                else if (tile == 31) {
                    // Gas pump
                    obj->position = { worldX, 0.75f, worldZ };  // y = half height
                    obj->scale = { tileSize / 2.0f, 0.75f, tileSize / 2.0f };
                    obj->color = { 1.0f, 0.8f, 0.0f };
                }
                else if (tile == 32) {
                    auto obj2 = std::make_unique<Cube>();
                    obj2->shader = shader;
                    obj2->init();
                    obj->position = { worldX, 1.5f, worldZ };
                    obj->scale = { tileSize / 2.0f, 0.05f, tileSize / 2.0f };
                    obj->color = { 1.0f, 0.8f, 0.0f };
                    obj2->position = { worldX, 0.01f, worldZ };  // at ground level
                    obj2->scale = { tileSize / 2.0f, 0.01f, tileSize / 2.0f };
                    obj2->color = { 0.3f, 0.3f, 0.3f };
                    obj2->updateModelMatrix();
                    objects.push_back(std::move(obj2));
                }
                else if (tile == 4) {
                    // Grass
                    obj->position = { worldX, 0.025f, worldZ };  // slightly above ground
                    obj->scale = { tileSize / 2.0f, 0.025f, tileSize / 2.0f };
                    obj->color = { 0.0f, 0.8f, 0.0f };
                }
                else if (tile == 0) {
                    // Road
                    obj->position = { worldX, 0.01f, worldZ };  // at ground level
                    obj->scale = { tileSize / 2.0f, 0.01f, tileSize / 2.0f };
                    obj->color = { 0.3f, 0.3f, 0.3f };

                }
                else if (tile == 7) {
                    obj->position = { worldX, 0.01f, worldZ };  // at ground level
                    obj->scale = { tileSize / 2.0f, 0.01f, tileSize / 2.0f };
                    obj->color = { 0.3f, 0.3f, 0.3f };
                   // std::cout << worldX << " " << worldZ << " " << std::endl;

                }
                else continue;
                
                
                obj->updateModelMatrix();
                objects.push_back(std::move(obj));
                count++;
            }
        }

       // std::cout << "✅ Simple city generated — no merging, literal tiles.\n" << count << std::endl;
    }




    void updateView(glm::vec3 viewRot, glm::vec3 targetPos, float carRotationZ) {
        for (auto& o : objects)
            o->updateViewMatrix(viewRot, targetPos, carRotationZ);
    }


    void render() {
        for (auto& o : objects)
            o->render();
    }
};

Ref<Shader> shader = nullptr;
glm::vec3 viewRotation{ 0, 0, 0 };
Cube cube;
City city;
extern "C" {
    
    void scene_init(scene_data_t scene_data) {
        // Scene preamble
        
        Application& app = Application::Get();
        ecs = app.GetECS();
        vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();
        auto module_name = string(scene_data.module_name);

        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/color"));

        // Setup camera
        camera = ecs->CreateEntity3D(null, Component::Transform(), "Main Camera");
        auto& cam = ecs->AddComponent<Camera>(camera, Camera::Perspective());
        cam.isMain = true;
        camComp = &cam;
        
        // Load and instantiate our 3D model
        Ref<Model> model = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/red_car.glb"));
        car = ecs->Instantiate(null, Component::Transform(), model);

        city.shader = shader;
        city.generate();
        //city.position={ 1,0,1 };
        //city.init();
    }

    void scene_update(float deltaTime) {
        // Rotate camera around origin
        static float angle = 0.0f;
        constexpr float ROTATION_SPEED = 0.05f;
        angle += ROTATION_SPEED * deltaTime;

        constexpr float radius = 2.5f;
        auto trans = ecs->GetTransformRef(camera);
        float camX = radius * cos(angle);
        float camZ = radius * sin(angle);
        trans.SetPosition(vec3(camX, 1.0f, camZ));
        camComp->LookAt(trans.GetPosition());

        // Rotate car wheels
        constexpr float WHEEL_ROTATION_SPEED = 5.0f;
        auto ecs = Application::Get().GetECS();
        for (entity_id child : ChildrenRange(ecs.get(), car)) {
            auto transformRef = ecs->GetTransformRef(child);
            transformRef.RotateAround({ 0.0f, 1.0f, 0.0f }, glm::radians(WHEEL_ROTATION_SPEED * deltaTime));

        }
        auto ref = ecs->GetTransformRef(car);
        ref.SetPosition({ 10,0,10 });
    
    }

    void scene_render() {
        city.shader->Enable();
        auto ident = glm::mat4(1.0f);
        //city.updateModelMatrix();
        city.updateView(viewRotation, cube.position, cube.rotation.z);
        city.render();
    }

    void scene_shutdown() {

    }
}
