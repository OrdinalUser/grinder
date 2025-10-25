#pragma once

#ifdef ENGINE_BUILD
    #define ENGINE_API __declspec(dllexport)
#else
    #define ENGINE_API __declspec(dllimport)
#endif

#ifdef SCENE_BUILD
    #define SCENE_API __declspec(dllexport)
#else
    #define SCENE_API __declspec(dllimport)
#endif