add_executable(runtime
    src/main.cpp
)

target_include_directories(runtime
    PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/include
)

target_link_libraries(runtime
    PRIVATE
        engine
        spdlog::spdlog
)
