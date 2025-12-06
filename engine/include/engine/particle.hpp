#pragma once

#include <engine/api.hpp>
#include <engine/types.hpp>
#include <engine/resource.hpp>
#include <engine/renderer.hpp>

namespace Engine {

    namespace Particle {
        enum class SpawnMethod {
            RANDOM,
            RAIN,
            FOUNTAIN
        };

        enum class LifetimeMethod {
            NONE,
            RESPAWN
        };
    }

    template <typename T>
    class ParticleSystem {
    public:

        struct InstanceData {
            Component::Transform transform;
            bool alive = true;
        };

        ParticleSystem(size_t maxParticleCount,
            BBox bounds,
            const Component::Drawable3D drawable,
            Particle::SpawnMethod spawn,
            Particle::LifetimeMethod lifetime)
            : m_Bounds(bounds)
            , m_Drawable(drawable)
            , m_Spawn(spawn)
            , m_Lifetime(lifetime)
        {
            m_Particles.resize(maxParticleCount);
            memset(m_Particles.data(), 0, m_Particles.size() * sizeof(T));
            m_Instances.resize(maxParticleCount);
            memset(m_Instances.data(), 0, m_Instances.size() * sizeof(InstanceData));
        }

        ~ParticleSystem() {
            // Release memory resources
            m_Particles.clear();
            m_Instances.clear();
        }

        // Spawn N new particles using the configured spawn method.
        void Spawn(size_t count) {
            size_t spawned = 0;

            // Sequential spawn (NOT parallelizable due to finding dead slots)
            for (size_t i = 0; i < m_Particles.size() && spawned < count; i++) {
                if (!m_Instances[i].alive) {
                    SpawnParticle(i);
                    spawned++;
                }
            }
        }

        // User-defined update function per particle.
        template <typename Func>
        void Update(float dt, Func&& fn) {
            const size_t n = m_Particles.size();

            // Limit OpenMP to 4 threads for this parallel region
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < (int)n; i++) {
                if (!m_Instances[i].alive)
                    continue;

                fn(dt, m_Particles[i], m_Instances[i]);
            }

            // Second pass: Collect dead particles and batch respawn
            if (m_Lifetime == Particle::LifetimeMethod::RESPAWN) {
                std::vector<size_t> deadIndices;
                deadIndices.reserve(n / 10); // Pre-allocate for ~10% death rate estimate

                // Sequential collection of dead particles
                for (size_t i = 0; i < n; i++) {
                    if (!m_Instances[i].alive) {
                        deadIndices.push_back(i);
                    }
                }

                // Batch respawn all dead particles
                for (size_t idx : deadIndices) {
                    SpawnParticle(idx);
                }
            }
        }

        void Draw(std::shared_ptr<Renderer> renderer) {
            for (size_t i = 0; i < m_Particles.size(); i++) {
                if (!m_Instances[i].alive) continue;

                renderer->QueueDrawable3D(
                    &m_Instances[i].transform,
                    &m_Drawable
                );
            }
        }

    private:
        // Actual spawn implementation based on mode.
        void SpawnParticle(size_t idx) {
            InstanceData& inst = m_Instances[idx];
            inst.alive = true;

            switch (m_Spawn) {
            case Particle::SpawnMethod::RANDOM: {
                inst.transform.position = RandomPointInBounds(m_Bounds);
                inst.transform.rotation = glm::quat(1, 0, 0, 0);
                inst.transform.scale = glm::vec3(1.0f);
            } break;

            case Particle::SpawnMethod::RAIN: {
                inst.transform.position = {
                    RandomFloat(m_Bounds.min.x, m_Bounds.max.x),
                    m_Bounds.max.y-1.0f,
                    RandomFloat(m_Bounds.min.z, m_Bounds.max.z)
                };
                inst.transform.rotation = glm::quat(1, 0, 0, 0);
                inst.transform.scale = glm::vec3(1.0f);
            } break;

            case Particle::SpawnMethod::FOUNTAIN: {
                inst.transform.position = glm::vec3(
                    (m_Bounds.min.x + m_Bounds.max.x) * 0.5f,
                    m_Bounds.min.y,
                    (m_Bounds.min.z + m_Bounds.max.z) * 0.5f
                );
                inst.transform.rotation = glm::quat(1, 0, 0, 0);
                inst.transform.scale = glm::vec3(1.0f);
            } break;
            }

            // Reset user payload T
            m_Particles[idx] = T{};
        }

        glm::vec3 RandomPointInBounds(const BBox& b) {
            return {
                RandomFloat(b.min.x, b.max.x),
                RandomFloat(b.min.y, b.max.y),
                RandomFloat(b.min.z, b.max.z)
            };
        }

        float RandomFloat(float a, float b) {
            float t = m_Rng() / float(m_Rng.max());
            return a + (b - a) * t;
        }

    private:
        std::vector<T> m_Particles;
        std::vector<InstanceData> m_Instances;

        BBox m_Bounds;
        Component::Drawable3D m_Drawable;
        Particle::SpawnMethod m_Spawn;
        Particle::LifetimeMethod m_Lifetime;

        std::mt19937 m_Rng{ std::random_device{}() };
    };

}
