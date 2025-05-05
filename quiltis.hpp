#pragma once

#include <variant>

#include <SFML/Graphics.hpp>

#if QUILTIS_SHARED_LIB
#if _WIN32
#ifdef QUILTIS_EXPORTS
#define QUILTIS_API __declspec(dllexport)
#else
#define QUILTIS_API __declspec(dllimport)
#endif
#elif __GNUC__ >= 4
#define QUILTIS_API __attribute__((visibility("default")))
#else
#define QUILTIS_API
#endif
#else
#define QUILTIS_API
#endif

namespace Quiltis
{
    struct RandomBlockSelection {};

    struct WeightedBlockSelection
    {
        int searchStride = 3;
        float selectionSpan = 0.1f;
    };

    using BlockSelection = std::variant<RandomBlockSelection, WeightedBlockSelection>;

    struct Settings
    {
        int seed{ 4830 };

        sf::Vector2i blockSize{ 90, 90 };
        sf::Vector2i overlap{ blockSize / 6};
        sf::Vector2i quiltSize{ 4, 4 };

        bool doCut = true;
        bool showSeams = false;
        bool blendSeams = false;
        bool useLogCost = true;
        bool makeTileable = false;

        bool useGpuAcceleration = true;

        BlockSelection blockSelection{ WeightedBlockSelection{} };
    };


    QUILTIS_API sf::Image quilt(const sf::Image& sourceImage, const Settings& settings);
};