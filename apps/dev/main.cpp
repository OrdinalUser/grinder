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

entity_id entity_model = null, camera = null, ring = null, sun = null;
Camera* camComp = nullptr;
Ref<ECS> ecs;
Ref<VFS> vfs;

void scene_init(scene_data_t scene_data) {
    // Scene preamble
    Application& app = Application::Get();
    ecs = app.GetECS();
    vfs = app.GetVFS();
    Ref<ResourceSystem> rs = app.GetResourceSystem();
    auto renderer = app.GetRenderer();
    auto module_name = string(scene_data.module_name);

    renderer->LoadSkybox(vfs->Resolve(module_name, "assets/skybox_clouds_adjusted"));

    // Setup camera
    camera = ecs->CreateEntity3D(null, Component::Transform(), "Main Camera");
    auto& cam = ecs->AddComponent<Camera>(camera, Camera::Perspective());
    cam.isMain = true;
    camComp = &cam;
    // Light camera_light = Light::Spot(12.5, 17.5, 50, vec3{ 1 }, 1);
    // ecs->AddComponent<Light>(camera, camera_light);

    // Load and instantiate our 3D model
    LoadCfg::Model modelConf;
    modelConf.normalize = true;

    Ref<Model> model = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/Police Car.glb"));
    entity_model = ecs->Instantiate(null, Component::Transform(), model);

    Component::Transform t{};
    t.position = { 10, 0, 0 }; ecs->Instantiate(null, t, model);
    t.position = { -10, 0, 0 }; ecs->Instantiate(null, t, model);

    //// Add sun
    sun = ecs->CreateEntity3D(null, Transform(), "Sun");
    Light sun_light = Light::Directional(Color(255, 0, 0).to_vec4(), 1.0f, {-0.4, -1.0, -0.4});
    // ecs->AddComponent<Light>(sun, sun_light);

    //// Create a ring of lights around our model
    Ref<Model> model_light = rs->load<Model>(vfs->GetResourcePath(module_name, "assets/cube.glb"));
    ring = ecs->CreateEntity3D(null, Transform(), "Light Ring");
    // Add lights into the ring
    constexpr float light_ring_radius = 30.0f;
    constexpr size_t light_count = 16;
    constexpr vec3 light_color{ 1.0f };
    constexpr float light_intensity = 0.4f;
    constexpr float light_range = 10.0f;

    for (size_t i = 0; i < light_count; i++) {
        float angle_deg = 360.0f / (static_cast<float>(light_count) / i);
        vec2 pos = {
            Math::cos(glm::radians(angle_deg)) * light_ring_radius - (rand() % 150 / 100.0f),
            Math::sin(glm::radians(angle_deg)) * light_ring_radius - (rand() % 150 / 100.0f)
        };
        Transform t{};
        t.position = vec3(pos.x, (rand() % 200 / 100.0f) - 1.0f, pos.y);
        t.scale = vec3{ 0.25f };
        entity_id e = ecs->Instantiate(ring, t, model_light);
        Light light = Light::Point(
            light_range, light_color, light_intensity
        );
        ecs->AddComponent<Light>(e, light);
    }
}

void scene_update_fixed(float deltaTime) {
    static float angle = 0.0f;
    constexpr float ROTATION_SPEED = 0.025f;
    angle += ROTATION_SPEED * deltaTime;

    // Rotate the light ring
    auto ring_transform = ecs->GetTransformRef(ring);
    ring_transform.RotateAround({ 0, 1, 0 }, glm::radians(angle));
    ring_transform.SetScale(glm::max(glm::abs(vec3(glm::cos(angle*10.0), 1, glm::cos(angle*10.0))), 0.1f));
}

void scene_update(float deltaTime) {
    // Rotate camera around origin
    static float angle = 0.0f;
    constexpr float ROTATION_SPEED = 0.25f;
    angle += ROTATION_SPEED * deltaTime;

    constexpr float radius = 25.0f;
    auto trans = ecs->GetTransformRef(camera);
    float camX = radius * cos(angle);
    float camZ = radius * sin(angle);
    trans.SetPosition(vec3(camX, 1, camZ));
    camComp->LookAt(trans.GetPosition());
}

void scene_render() {
    return;
}

void scene_shutdown() {
    ecs->DestroyEntity(camera);
    ecs->DestroyEntity(sun);
    ecs->DestroyEntity(entity_model, true);
    ecs->DestroyEntity(ring, true);
}

//// Many entities test
//
//#include <engine/scene_api.hpp>
//#include <engine/exception.hpp>
//#include <engine/log.hpp>
//#include <engine/vfs.hpp>
//#include <engine/resource.hpp>
//
//#include <engine/types.hpp>
//#include <engine/application.hpp>
//#include <engine/ecs.hpp>
//
//#include <glad/glad.h>
//#include <vector>
//#include <random>
//
//using namespace Engine;
//using namespace Engine::Component;
//
//// Initial debug thingy
//static void log_entity(entity_id id, const std::string& label = "") {
//    Ref<ECS> ecs = Application::Get().GetECS();
//    auto tref = ecs->GetTransformRef(id);
//    auto& h = ecs->GetComponent<Hierarchy>(id);
//    const Transform& t = tref.GetTransform();
//
//    Log::info("{}: {}, depth=({})", label, id, h.depth);
//    Log::info("-- pos=({}, {}, {}), rot=({}, {}, {}, {}), scale=({}, {}, {})",
//        t.position.x, t.position.y, t.position.z,
//        t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
//        t.scale.x, t.scale.y, t.scale.z
//    );
//
//    Log::info("-- children:");
//    if (h.first_child != null) {
//        for (entity_id cid = h.first_child; cid != null;) {
//            auto& hc = ecs->GetComponent<Hierarchy>(cid);
//            Log::info("--- child={} (depth={})", cid, hc.depth);
//            log_entity(cid);
//            cid = hc.next_sibling;
//        }
//    }
//}
//
//// --- Simulation Constants ---
//constexpr u32 NUM_ENTITIES = 2000;
//constexpr u32 MAX_DEPTH = 8;
//constexpr u32 NUM_UPDATING_ENTITIES = 200; // Number of entities to randomly rotate each frame
//constexpr float ROTATION_SPEED = 150.0f; // Degrees per second
//
//// --- Global Scene Variables ---
//Ref<Shader> shader = nullptr;
//GLuint VAO = 0;
//GLuint VBO = 0;
//
//std::vector<entity_id> all_entities;
//std::vector<entity_id> entities_to_update_this_frame;
//
//// A persistent random number generator for performance
//std::mt19937 random_generator;
//
//// Helper to draw an entity as a colored point
//static void DrawEntity(entity_id id, const vec3& color) {
//    Ref<ECS> ecs = Application::Get().GetECS();
//    const mat4& model = ecs->GetComponent<Transform>(id).modelMatrix;
//    int modelLoc = glGetUniformLocation(shader->program, "modelMatrix");
//    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
//    int colorLoc = glGetUniformLocation(shader->program, "objectColor");
//    glUniform3fv(colorLoc, 1, &color[0]);
//    glDrawArrays(GL_POINTS, 0, 1);
//}
//
//extern "C" {
//    void scene_init(scene_data_t scene_data) {
//        Application& app = Application::Get();
//        Ref<ECS> ecs = app.GetECS();
//
//        // --- 1. Setup Randomness ---
//        std::random_device rd;
//        random_generator.seed(rd());
//
//        Log::info("Generating scene with {} entities, max depth of {}...", NUM_ENTITIES, MAX_DEPTH);
//
//        // --- 2. Generate the Chaotic Hierarchy ---
//        all_entities.reserve(NUM_ENTITIES);
//
//        // The first entity is always a root node
//        entity_id root = ecs->CreateEntity3D();
//        all_entities.push_back(root);
//
//        for (u32 i = 1; i < NUM_ENTITIES; ++i) {
//            entity_id parent_id = null;
//
//            // Find a random, valid parent that won't exceed the max depth
//            while (true) {
//                std::uniform_int_distribution<u32> parent_dist(0, all_entities.size() - 1);
//                entity_id potential_parent = all_entities[parent_dist(random_generator)];
//
//                if (ecs->GetComponent<Hierarchy>(potential_parent).depth < MAX_DEPTH) {
//                    parent_id = potential_parent;
//                    break;
//                }
//            }
//
//
//            entity_id new_entity = ecs->CreateEntity3D(parent_id);
//            all_entities.push_back(new_entity);
//
//            if (i % 100 == 0) {
//                Component::Light l; l.diffuse = Color(255, 255, 0, 255);
//                ecs->AddComponent(new_entity, l);
//            }
//
//            // Give it a random position relative to its parent to spread things out
//            std::uniform_real_distribution<float> pos_dist(-15.0f, 15.0f);
//            ecs->GetTransformRef(new_entity).SetPosition({ pos_dist(random_generator), pos_dist(random_generator), pos_dist(random_generator) });
//        }
//
//        entities_to_update_this_frame.resize(NUM_UPDATING_ENTITIES);
//        Log::info("Scene generation complete.");
//
//        // log_entity(root, "root");
//
//        // --- 3. Load Shader and Setup OpenGL ---
//
//        Ref<VFS> vfs = app.GetVFS();
//        Ref<ResourceSystem> rs = app.GetResourceSystem();
//        auto module_name = string(scene_data.module_name);
//        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/point"));
//
//        float pointVertex[] = { 0.0f, 0.0f, 0.0f };
//        glGenVertexArrays(1, &VAO);
//        glGenBuffers(1, &VBO);
//        glBindVertexArray(VAO);
//        glBindBuffer(GL_ARRAY_BUFFER, VBO);
//        glBufferData(GL_ARRAY_BUFFER, sizeof(pointVertex), pointVertex, GL_STATIC_DRAW);
//        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
//        glEnableVertexAttribArray(0);
//        glBindBuffer(GL_ARRAY_BUFFER, 0);
//        glBindVertexArray(0);
//        glEnable(GL_PROGRAM_POINT_SIZE);
//        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
//    }
//
//    void scene_update(float deltaTime) {
//        Ref<ECS> ecs = Application::Get().GetECS();
//        if (all_entities.size() == 0) return;
//
//        // --- 1. Select a new random subset of entities to update this frame ---
//        std::uniform_int_distribution<u32> entity_dist(0, all_entities.size() - 1);
//        for (u32 i = 0; i < NUM_UPDATING_ENTITIES; ++i) {
//            entities_to_update_this_frame[i] = all_entities[entity_dist(random_generator)];
//        }
//
//        // --- 2. Apply a rotation to only that subset ---
//        for (entity_id id : entities_to_update_this_frame) {
//            auto transformRef = ecs->GetTransformRef(id);
//            float angle = glm::radians(ROTATION_SPEED * deltaTime);
//            vec3 axis = { 0.0f, 0.0f, 1.0f };
//            quat rotationDelta = glm::angleAxis(angle, axis);
//            quat newRotation = rotationDelta * transformRef.GetRotation();
//            transformRef.SetRotation(newRotation);
//        }
//
//        // --- 3. Kill random entity
//        entity_id last = all_entities[all_entities.back()];
//        all_entities.pop_back();
//        ecs->DestroyEntity(last);
//    }
//
//    void scene_render() {
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        glUseProgram(shader->program);
//        glBindVertexArray(VAO);
//
//        // --- Setup a camera that can see the whole nebula ---
//        auto& window = Application::Get().GetWindow();
//        float aspectRatio = window.GetAspectRatio();
//        float orthoSize = 60.0f; // Greatly increased size to see the cloud
//        mat4 projection = glm::ortho(-orthoSize * aspectRatio, orthoSize * aspectRatio, -orthoSize, orthoSize, -100.0f, 100.0f);
//        mat4 view = glm::lookAt(vec3(0.0f, 0.0f, 50.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
//
//        glUniformMatrix4fv(glGetUniformLocation(shader->program, "projection"), 1, GL_FALSE, &projection[0][0]);
//        glUniformMatrix4fv(glGetUniformLocation(shader->program, "view"), 1, GL_FALSE, &view[0][0]);
//
//        // --- Draw all entities ---
//        for (entity_id id : all_entities) {
//            DrawEntity(id, { 0.8f, 0.9f, 1.0f }); // A nice nebula-blue color
//        }
//
//        glBindVertexArray(0);
//        glUseProgram(0);
//    }
//
//    void scene_shutdown() {
//        glDeleteVertexArrays(1, &VAO);
//        glDeleteBuffers(1, &VBO);
//        VAO = 0;
//        VBO = 0;
//    }
//}

//// Simple scene test
//
//#include <engine/scene_api.hpp>
//#include <engine/exception.hpp>
//#include <engine/log.hpp>
//#include <engine/vfs.hpp>
//#include <engine/resource.hpp>
//
//#include <engine/types.hpp>
//#include <engine/application.hpp>
//#include <engine/ecs.hpp>
//
//#include <glad/glad.h>
//#include <vector>
//
//using namespace Engine;
//using namespace Engine::Component;
//
//// --- Data Structures for a Cleaner Scene ---
//struct OrbitalPivot {
//    entity_id id;
//    float revolution_speed; // The speed at which this pivot (and its children) orbit the parent.
//};
//
//struct CelestialBody {
//    entity_id id;
//    float rotation_speed;   // The speed of the body's own axial spin.
//    vec3 color;
//};
//
//// --- Global Scene Variables ---
//Ref<Shader> shader = nullptr;
//GLuint VAO = 0;
//GLuint VBO = 0;
//std::vector<OrbitalPivot>   orbital_pivots;
//std::vector<CelestialBody>  celestial_bodies;
//
//// Helper function to apply rotation to an entity
//static void ApplyRotation(Ref<ECS>& ecs, entity_id id, float speed, float deltaTime) {
//    auto transformRef = ecs->GetTransformRef(id);
//    float angle = glm::radians(speed * deltaTime);
//    vec3 axis = { 0.0f, 0.0f, 1.0f }; // Z-axis for top-down view
//    quat rotationDelta = glm::angleAxis(angle, axis);
//    quat newRotation = rotationDelta * transformRef.GetRotation();
//    transformRef.SetRotation(newRotation);
//}
//
//// Helper function to draw an entity as a colored point
//static void DrawEntity(entity_id id, const vec3& color) {
//    Ref<ECS> ecs = Application::Get().GetECS();
//    const mat4& model = ecs->GetComponent<Transform>(id).modelMatrix;
//    int modelLoc = glGetUniformLocation(shader->program, "modelMatrix");
//    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
//    int colorLoc = glGetUniformLocation(shader->program, "objectColor");
//    glUniform3fv(colorLoc, 1, &color[0]);
//    glDrawArrays(GL_POINTS, 0, 1);
//}
//
//extern "C" {
//    void scene_init(scene_data_t scene_data) {
//        Application& app = Application::Get();
//        Ref<ECS> ecs = app.GetECS();
//
//        // --- 1. Define Solar System Data ---
//        // Revolution speed: How fast the planet orbits the sun.
//        // Rotation speed: How fast the planet spins on its own axis.
//        const vec3 COLOR_SUN = { 1.0f, 1.0f, 0.0f };
//        const vec3 COLOR_PLANET = { 0.7f, 0.7f, 0.7f };
//        const vec3 COLOR_EARTH = { 0.2f, 0.4f, 1.0f };
//        const vec3 COLOR_GAS = { 0.9f, 0.6f, 0.4f };
//        const vec3 COLOR_MARS = { 0.9f, 0.3f, 0.1f };
//        const vec3 COLOR_ICE = { 0.6f, 0.8f, 0.9f };
//        const vec3 COLOR_MOON = { 0.8f, 0.8f, 0.8f };
//
//        // --- 2. Create All Entities using the Pivot Pattern ---
//        entity_id sun = ecs->CreateEntity3D();
//        celestial_bodies.push_back({ sun, -5.0f, COLOR_SUN });
//
//        auto create_planet = [&](entity_id parent, float orbit_radius, float revolution_speed, float rotation_speed, const vec3& color) {
//            entity_id pivot = ecs->CreateEntity3D(parent);
//            orbital_pivots.push_back({ pivot, revolution_speed });
//
//            entity_id planet = ecs->CreateEntity3D(pivot);
//            ecs->GetTransformRef(planet).SetPosition({ orbit_radius, 0.0f, 0.0f });
//            celestial_bodies.push_back({ planet, rotation_speed, color });
//            return planet; // Return the planet's ID in case it has moons
//            };
//
//        entity_id mercury = create_planet(sun, 0.8f, -88.0f, -20.0f, COLOR_PLANET);
//        entity_id venus = create_planet(sun, 1.2f, -62.0f, 15.0f, COLOR_GAS); // Retrograde axial spin
//        entity_id earth = create_planet(sun, 1.7f, -50.0f, -150.0f, COLOR_EARTH);
//        entity_id mars = create_planet(sun, 2.2f, -40.0f, -120.0f, COLOR_MARS);
//        entity_id jupiter = create_planet(sun, 3.5f, -25.0f, -80.0f, COLOR_GAS);
//        entity_id saturn = create_planet(sun, 4.8f, -20.0f, -70.0f, COLOR_GAS);
//        entity_id uranus = create_planet(sun, 5.9f, -15.0f, 45.0f, COLOR_ICE); // Retrograde axial spin
//        entity_id neptune = create_planet(sun, 6.8f, -10.0f, -55.0f, COLOR_ICE);
//
//        // Add moons, making them children of their respective planets
//        entity_id luna = create_planet(earth, 0.25f, -200.0f, -50.0f, COLOR_MOON);
//        entity_id phobos = create_planet(mars, 0.15f, -400.0f, -200.0f, COLOR_MOON);
//        entity_id deimos = create_planet(mars, 0.25f, -300.0f, -180.0f, COLOR_MOON);
//        entity_id titan = create_planet(saturn, 0.5f, -150.0f, -100.0f, COLOR_MOON);
//
//        // --- 3. Load Shader and Setup OpenGL ---
//        // ... (This part remains the same)
//        Ref<VFS> vfs = app.GetVFS();
//        Ref<ResourceSystem> rs = app.GetResourceSystem();
//        auto module_name = string(scene_data.module_name);
//        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/point"));
//
//        float pointVertex[] = { 0.0f, 0.0f, 0.0f };
//        glGenVertexArrays(1, &VAO);
//        glGenBuffers(1, &VBO);
//        glBindVertexArray(VAO);
//        glBindBuffer(GL_ARRAY_BUFFER, VBO);
//        glBufferData(GL_ARRAY_BUFFER, sizeof(pointVertex), pointVertex, GL_STATIC_DRAW);
//        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
//        glEnableVertexAttribArray(0);
//        glBindBuffer(GL_ARRAY_BUFFER, 0);
//        glBindVertexArray(0);
//        glEnable(GL_PROGRAM_POINT_SIZE);
//        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
//    }
//
//    void scene_update(float deltaTime) {
//        Ref<ECS> ecs = Application::Get().GetECS();
//        // Update all the orbital pivots
//        for (const auto& pivot : orbital_pivots) {
//            ApplyRotation(ecs, pivot.id, pivot.revolution_speed, deltaTime);
//        }
//        // Update all the visible bodies' axial rotations
//        for (const auto& body : celestial_bodies) {
//            ApplyRotation(ecs, body.id, body.rotation_speed, deltaTime);
//        }
//    }
//
//    void scene_render() {
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        glUseProgram(shader->program);
//        glBindVertexArray(VAO);
//
//        auto& window = Application::Get().GetWindow();
//        float aspectRatio = window.GetAspectRatio();
//        float orthoSize = 7.5f; // Increased size to see Neptune's orbit
//        mat4 projection = glm::ortho(-orthoSize * aspectRatio, orthoSize * aspectRatio, -orthoSize, orthoSize, -10.0f, 10.0f);
//        mat4 view = glm::lookAt(vec3(0.0f, 0.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
//
//        glUniformMatrix4fv(glGetUniformLocation(shader->program, "projection"), 1, GL_FALSE, &projection[0][0]);
//        glUniformMatrix4fv(glGetUniformLocation(shader->program, "view"), 1, GL_FALSE, &view[0][0]);
//
//        // Loop through and draw all the visible bodies. The pivots are invisible.
//        for (const auto& body : celestial_bodies) {
//            DrawEntity(body.id, body.color);
//        }
//
//        glBindVertexArray(0);
//        glUseProgram(0);
//    }
//
//    void scene_shutdown() {
//        // You were absolutely right to point out this needs to be done.
//        // Explicitly clean up the OpenGL objects created in scene_init.
//        glDeleteVertexArrays(1, &VAO);
//        glDeleteBuffers(1, &VBO);
//        VAO = 0;
//        VBO = 0;
//    }
//}
