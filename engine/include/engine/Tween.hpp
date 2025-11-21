#pragma once
#include "types.hpp"
#include "Easing.hpp"

namespace Engine::Tween {

    using namespace Engine;
    using Engine::Component::Transform;

    inline Transform Interpolate(
        const Transform& a,
        const Transform& b,
        float t,
        Easing::Func easing = Easing::Linear
    ) {
        Transform out;

        float e = easing(t);

        // Position and scale → standard mix
        out.position = glm::mix(a.position, b.position, e);
        out.scale    = glm::mix(a.scale,    b.scale,    e);

        // Rotation → slerp
        out.rotation = glm::slerp(a.rotation, b.rotation, e);

        // Update model matrix
        out.modelMatrix = glm::translate(glm::mat4(1.f), out.position) *
            glm::mat4_cast(out.rotation) *
            glm::scale(glm::mat4(1.f), out.scale);

        return out;
    }

} // namespace Engine::Tween
