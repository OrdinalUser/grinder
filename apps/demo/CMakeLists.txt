add_library(scene_demo SHARED main.cpp)
target_include_directories(scene_demo PRIVATE ${CMAKE_SOURCE_DIR}/engine/include)
target_link_libraries(scene_demo PRIVATE engine glm)
target_compile_definitions(scene_demo PRIVATE SCENE_BUILD)