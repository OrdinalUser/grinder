add_library(scene_dev SHARED main.cpp)
target_include_directories(scene_dev PRIVATE ${CMAKE_SOURCE_DIR}/engine/include)
target_link_libraries(scene_dev PRIVATE glm engine spdlog)
