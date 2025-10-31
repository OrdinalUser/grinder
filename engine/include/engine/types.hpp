#pragma once

// Core forward declarations and lightweight aliases.
// No heavy includes, only forward decls for external libs
// ...well this aged poorly

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <cstdint>
#include <optional>
#include <variant>
#include <concepts>
#include <algorithm>
#include <unordered_set>
#include <tuple>
#include <iterator>

namespace Engine {
    namespace Math {
        using namespace ::glm;
        template <typename T>
        inline T lerp(const T& a, const T& b, float t) {
            return mix(a, b, t);
        }
    }

    // --- Math types (from glm) ---
    using vec2 = Math::vec2;
    using vec3 = Math::vec3;
    using vec4 = Math::vec4;
    using mat4 = Math::mat4;
    using quat = Math::quat;

    // --- Primitive aliases ---
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using f32 = float;
    using f64 = double;

    // --- Optional and variant types ---
    template<typename T>
    using optional = std::optional<T>;

    template<typename... Ts>
    using variant = std::variant<Ts...>;

    // --- STL containers ---
    using string = std::string;

    template<typename T>
    using vector = std::vector<T>;

    template<typename K, typename V>
    using unordered_map = std::unordered_map<K, V>;
    
    template<typename K>
    using unordered_set = std::unordered_set<K>;

    template<typename T, typename Container = std::vector<T>>
    using priority_queue = std::priority_queue<T, Container>;

    template<typename... types>
    using tuple = std::tuple<types...>;

    // --- Smart pointers ---
    template<typename T>
    using shared = std::shared_ptr<T>;
    using std::make_shared;

    template<typename T>
    using unique = std::unique_ptr<T>;
    using std::make_unique;

    template<typename T>
    using weak = std::weak_ptr<T>;

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename... Args>
    constexpr Ref<T> MakeRef(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    struct Color {
        f32 r = 1.f, g = 1.f, b = 1.f, a = 1.f;
        
        constexpr Color() = default;
        constexpr Color(f32 r, f32 g, f32 b, f32 a = 1.f)
            : r(r), g(g), b(b), a(a) {}
        constexpr Color(u8 r, u8 g, u8 b, u8 a = 255)
            : r{r/255.0f}, g{g/255.0f}, b{b/255.0f}, a{a/255.0f} {}
        constexpr Color(i32 r, i32 g, i32 b, i32 a = 255)
            : r{ r / 255.0f }, g{ g / 255.0f }, b{ b / 255.0f }, a{ a / 255.0f } {
        }
        constexpr glm::vec4 to_vec4() const noexcept { return { r, g, b, a }; }

        constexpr Color(const Color&) = default;
        constexpr Color(Color&&) noexcept = default;
        constexpr Color& operator=(const Color&) = default;
        constexpr Color& operator=(Color&&) noexcept = default;

        constexpr bool operator==(const Color& other) const noexcept {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }
        constexpr bool operator!=(const Color& other) const noexcept {
            return !(*this == other);
        }

        // predefined colors
        static constexpr Color white() noexcept { return { 1.f, 1.f, 1.f, 1.f }; }
        static constexpr Color black() noexcept { return { 0.f, 0.f, 0.f, 1.f }; }
        static constexpr Color red()   noexcept { return { 1.f, 0.f, 0.f, 1.f }; }
        static constexpr Color green() noexcept { return { 0.f, 1.f, 0.f, 1.f }; }
        static constexpr Color blue()  noexcept { return { 0.f, 0.f, 1.f, 1.f }; }
    };

    template<typename T>
    concept Arithmetic = std::is_arithmetic_v<T>;

    template<typename T>
    concept Iterable = requires(T a) {
        std::begin(a);
        std::end(a);
    };

    // --- Basic utilities ---
    using std::min;
    using std::max;
    using std::clamp;

} // namespace Engine
