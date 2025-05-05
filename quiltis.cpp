#include "quiltis.hpp"

#include <numeric>
#include <unordered_set>
#include <vector>
#include <limits>
#include <random>
#include <cmath>

namespace Quiltis
{

namespace
{
    sf::Color lerpColor(sf::Color c1, sf::Color c2, float a)
    {
        return sf::Color(
            std::lerp(c1.r, c2.r, a),
            std::lerp(c1.g, c2.g, a),
            std::lerp(c1.b, c2.b, a),
            std::lerp(c1.a, c2.a, a)
        );
    }

    struct Vector2iHash
    {
        std::size_t operator()(const sf::Vector2i& v) const noexcept
        {
            return std::hash<int>{}(v.x + v.y * 10000);
        }
    };

    enum class Direction
    {
        Horizontal,
        Vertical
    };

    std::vector<float> imageDifference(const sf::Image& src, const sf::Image& dest, sf::IntRect srcRect)
    {
        std::vector<float> difference(srcRect.size.x * srcRect.size.y);
        auto diffPtr = difference.data();

        auto srcPtr = (sf::Color*)src.getPixelsPtr();
        auto dstPtr = (sf::Color*)dest.getPixelsPtr();

        srcPtr += (srcRect.position.x + srcRect.position.y * src.getSize().x);

        const int srcStride = src.getSize().x - srcRect.size.x;
        const int dstStride = dest.getSize().x - srcRect.size.x;

        const auto dist = [](const sf::Color& c1, const sf::Color c2)
        {
            const auto r = c1.r - c2.r;
            const auto g = c1.g - c2.g;
            const auto b = c1.b - c2.b;
            return std::sqrt(r * r + g * g + b * b);
        };

        for (int posY = 0; posY < srcRect.size.y; posY++)
        {
            for (int posX = 0; posX < srcRect.size.x; posX++)
            {
                *diffPtr = dist(*srcPtr, *dstPtr);
                diffPtr++;
                srcPtr++;
                dstPtr++;
            }

            srcPtr += srcStride;
            dstPtr += dstStride;
        }

        return difference;
    }

    template<Direction direction>
    std::vector<sf::Vector2i> generatePath(const std::vector<float>& differenceMap, sf::Vector2i mapSize)
    {
        std::vector<sf::Vector2i> path;

        struct Node
        {
            sf::Vector2i parent{ -1, -1 };
            double gScore = std::numeric_limits<double>::max();
            double fScore = std::numeric_limits<double>::max();
        };

        static const std::array<sf::Vector2i, 8> directions = {
            {{-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}}
        };

        const auto getHeuristic = [&](sf::Vector2i pos) -> double
        {
            if constexpr (direction == Direction::Horizontal)
            {
                return mapSize.y - pos.y - 1;
            }
            else
            {
                return mapSize.x - pos.x - 1;
            }
        };

        const auto posToIndex = [&](sf::Vector2i pos)
        {
            return pos.x + pos.y * mapSize.x;
        };

        const auto indexToPos = [&](std::size_t index)
        {
            return sf::Vector2i(index % mapSize.x, index / mapSize.x);
        };

        std::unordered_set<std::size_t> openTiles{};

        std::vector<Node> nodes(differenceMap.size());

        for (int x = 0; x < (direction == Direction::Horizontal ? mapSize.x : mapSize.y); x++)
        {
            auto& node = nodes[x];
            sf::Vector2i pos;

            if constexpr (direction == Direction::Horizontal)
            {
                pos = { x, 0 };
            }
            else
            {
                pos = { 0, x };
            }

            node.gScore = 0;
            node.fScore = getHeuristic(pos);
            openTiles.insert(posToIndex(pos));
        }

        while (!openTiles.empty())
        {
            double minCost = std::numeric_limits<double>::max();
            std::size_t minIndex{};
            for (const auto index : openTiles)
            {
                const auto& node = nodes[index];
                if (node.fScore < minCost)
                {
                    minCost = node.fScore;
                    minIndex = index;
                }
            }

            auto& current = nodes[minIndex];
            const auto currentPos = indexToPos(minIndex);
            if ((direction == Direction::Horizontal ? currentPos.y == mapSize.y - 1 : currentPos.x == mapSize.x - 1))
            {
                sf::Vector2i pos = currentPos;
                while (pos.x >= 0)
                {
                    path.push_back(pos);
                    const auto parent = nodes[posToIndex(pos)].parent;
                    nodes[posToIndex(pos)].parent = { -1, -1 };
                    pos = parent;
                }

                return path;

            }

            openTiles.erase(minIndex);

            for (int x = 0; x < directions.size(); x++)
            {
                const auto newPos = currentPos + directions[x];
                if (newPos.x < 0 || newPos.x >= mapSize.x || newPos.y < 0 || newPos.y >= mapSize.y)
                {
                    continue;
                }

                const auto newIndex = posToIndex(newPos);
                auto& newNode = nodes[newIndex];
                const auto tempCost = current.gScore + differenceMap[newIndex];
                if (tempCost < newNode.gScore)
                {
                    newNode.parent = currentPos;
                    newNode.gScore = tempCost;
                    newNode.fScore = tempCost + getHeuristic(newPos);
                    openTiles.insert(newIndex);
                }
            }
        }

        return path;
    }

    template<Direction direction>
    void cutImage(sf::Image& image, std::vector<sf::Vector2i> path)
    {
        std::unordered_set<sf::Vector2i, Vector2iHash> border(path.begin(), path.end());

        static const std::array<sf::Vector2i, 4> directions = {
            {{-1, 0}, {0, -1}, {1, 0}, {0, 1}}
        };

        std::unordered_set<sf::Vector2i, Vector2iHash> closedTiles{};
        std::unordered_set<sf::Vector2i, Vector2iHash> openTiles{};

        if constexpr (direction == Direction::Horizontal)
        {
            for (int y = 0; y < image.getSize().y; y++)
            {
                openTiles.insert(sf::Vector2i(0, y));
            }
        }
        else
        {
            for (int x = 0; x < image.getSize().x; x++)
            {
                openTiles.insert(sf::Vector2i(x, 0));
            }
        }

        while (!openTiles.empty())
        {
            const auto pos = *openTiles.begin();
            openTiles.erase(pos);
            closedTiles.insert(pos);

            if (border.find(pos) != border.end())
            {
                continue;
            }

            image.setPixel(sf::Vector2u(pos), sf::Color::Transparent);

            for (int x = 0; x < directions.size(); x++)
            {
                const auto newPos = pos + directions[x];
                if (newPos.x < 0 || newPos.x >= image.getSize().x || newPos.y < 0 || newPos.y >= image.getSize().y)
                {
                    continue;
                }

                if (closedTiles.find(newPos) != closedTiles.end())
                {
                    continue;
                }

                openTiles.insert(newPos);
            }
        }
    }

    template<typename RndEngine>
    sf::Vector2i selectRandomBlock(RndEngine& rngEngine, const sf::Image& srcImage, sf::Vector2i blockSize)
    {
        std::uniform_int_distribution<int> srcDistX(0, srcImage.getSize().x - blockSize.x);
        std::uniform_int_distribution<int> srcDistY(0, srcImage.getSize().y - blockSize.y);
        return { srcDistX(rngEngine), srcDistY(rngEngine) };
    }

    template<typename RndEngine>
    sf::Vector2i weightedSelection(RndEngine& rngEngine, const std::uint32_t* data, sf::Vector2i area, float selectionSpan)
    {
        const auto count = area.x * area.y;
        std::vector<std::size_t> idx(count);
        std::iota(idx.begin(), idx.end(), 0);

        std::sort(idx.begin(), idx.end(), [&](size_t i1, size_t i2) {return data[i1] < data[i2]; });

        std::uniform_int_distribution<int> selectionDistribution(0, count * selectionSpan);
        const auto selectionIndex = idx[selectionDistribution(rngEngine)];

        const sf::Vector2i bestPos(selectionIndex % area.x, selectionIndex / area.x);

        return bestPos;
    }

    sf::Shader& getBlockSelectionShader()
    {
        static constexpr std::string_view vertSrc = R"===(
void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
)===";

        static constexpr std::string_view  fragSrc = R"===(
#version 130

uniform ivec2 srcSize;
uniform sampler2D srcTexture;

uniform ivec2 blockSize;
uniform sampler2D blockTexture;

uniform ivec2 topOverlap;
uniform ivec2 leftOverlap;
uniform int corse;

void main()
{
    vec2 blockPos = gl_TexCoord[0].xy;
    
    float count = float(topOverlap.x * topOverlap.y + leftOverlap.x * leftOverlap.y - topOverlap.y * leftOverlap.x);
    float error = 0.f;

    for(int y = 0; y < topOverlap.y; y += corse)
    {
        for(int x = 0; x < topOverlap.x; x += corse)
        {
            vec2 offset = vec2(x + 0.5, y + 0.5);
            vec4 blockPixel = texture2D(blockTexture, offset / blockSize);
            vec4 srcPixel = texture2D(srcTexture, (blockPos + offset) / srcSize);
            float diff = length(blockPixel - srcPixel);
            error += diff;
        }
    }

    for(int y = topOverlap.y; y < leftOverlap.y; y += corse)
    {
        for(int x = 0; x < leftOverlap.x; x += corse)
        {
            vec2 offset = vec2(x + 0.5, y + 0.5);
            vec4 blockPixel = texture2D(blockTexture, offset / blockSize);
            vec4 srcPixel = texture2D(srcTexture, (blockPos + offset) / srcSize);
            float diff = length(blockPixel - srcPixel);            
            error += diff;
        }
    }

    error /= count;

    uint data = uint(error * 255);
    gl_FragColor = vec4(
        float((data >> uint(24)) & uint(0xFF)) / 255.f,
        float((data >> uint(16)) & uint(0xFF)) / 255.f,
        float((data >> uint(8)) & uint(0xFF)) / 255.f,
        float((data >> uint(0)) & uint(0xFF)) / 255.f
    );
}
)===";

        static sf::Shader shader(vertSrc, fragSrc);
        return shader;
    }

    template<typename RndEngine>
    sf::Vector2i selectBestBlockGpu(const WeightedBlockSelection& settings, RndEngine& rngEngine, const sf::Texture& srcTexture, const sf::Image& quiltImage, sf::Vector2i blockSize, sf::Vector2i blockPos, sf::Vector2i overlap)
    {
        const auto area = sf::Vector2i(srcTexture.getSize()) - blockSize;
        std::vector<float> blockErrors(area.x * area.y);
        sf::RenderTexture target{ sf::Vector2u(area) };

        sf::Vector2i topOverlap(blockSize.x, overlap.y);
        sf::Vector2i leftOverlap(overlap.x, blockSize.y);

        if (blockPos.x == 0)
        {
            leftOverlap = {};
        }

        if (blockPos.y == 0)
        {
            topOverlap = {};
        }

        sf::Texture blockTexture(quiltImage, false, { blockPos, blockSize });
        blockTexture.setSmooth(0);

        auto& shader = getBlockSelectionShader();

        sf::RenderStates renderStates{};
        renderStates.shader = &shader;
        renderStates.coordinateType = sf::CoordinateType::Pixels;
        renderStates.blendMode = sf::BlendNone;

        shader.setUniform("srcTexture", srcTexture);
        shader.setUniform("srcSize", sf::Vector2i(srcTexture.getSize()));
        shader.setUniform("blockTexture", blockTexture);
        shader.setUniform("blockSize", blockSize);
        shader.setUniform("overlap", overlap);
        shader.setUniform("topOverlap", topOverlap);
        shader.setUniform("leftOverlap", leftOverlap);
        shader.setUniform("corse", settings.searchStride);

        sf::RectangleShape shape{ sf::Vector2f(area) };
        shape.setTextureRect({ {}, area });

        target.draw(shape, renderStates);
        target.display();

        const auto result = target.getTexture().copyToImage();

        std::uint32_t minError = std::numeric_limits<std::int32_t>::max();
        std::uint32_t minIndex{};

        const auto count = result.getSize().x * result.getSize().y;

        auto ptr = (std::uint32_t*)result.getPixelsPtr();

        return weightedSelection(rngEngine, ptr, area, settings.selectionSpan);
    }

    template<typename RndEngine>
    sf::Vector2i selectBestBlockCpu(const WeightedBlockSelection& settings, RndEngine& rngEngine, const sf::Image& srcImage, const sf::Image& quiltImage, sf::Vector2i blockSize, sf::Vector2i blockPos, sf::Vector2i overlap)
    {
        const auto area = sf::Vector2i(srcImage.getSize()) - blockSize;

        sf::Image blockImage{ sf::Vector2u(blockSize) };

        const sf::Vector2i topOverlap(blockSize.x, overlap.y);
        const sf::Vector2i leftOverlap(overlap.x, blockSize.y);

        const auto average = [&](auto vec, auto offset) -> float
        {
            auto const count = static_cast<float>(vec.size() - offset);
            return std::reduce(vec.begin() + offset, vec.end()) / count;
        };

        std::vector<std::uint32_t> blockErrors(area.x * area.y);
        for (int y = 0; y < area.y; y += settings.searchStride)
        {
            for (int x = 0; x < area.x; x += settings.searchStride)
            {
                blockImage.copy(srcImage, {}, { {x, y}, blockSize });
                std::vector<float> topDifference = imageDifference(quiltImage, blockImage, { blockPos, topOverlap });
                std::vector<float> leftDifference = imageDifference(quiltImage, blockImage, { blockPos, leftOverlap });

                const auto topAverage = average(topDifference, 0);
                const auto leftAverage = average(leftDifference, topOverlap.y * leftOverlap.x);
                const auto average = (topAverage + leftAverage) / 2.f;
                blockErrors[x + y * area.x] = average * 255.f;
            }
        }

        return weightedSelection(rngEngine, blockErrors.data(), area, settings.selectionSpan);
    }
}

sf::Image quilt(const sf::Image& sourceImage, const Settings& settings)
{
    const auto overlap = settings.overlap;
    const auto blockSize = settings.blockSize;
    const auto quiltSize = settings.quiltSize;

    if (blockSize.x <= 0 || blockSize.y <= 0)
    {
        return {};
    }

    if (overlap.x <= 0 || overlap.y <= 0 || overlap.x >= blockSize.x || overlap.y >= blockSize.y)
    {
        return {};
    }

    if (quiltSize.x < 1 || quiltSize.y < 1)
    {
        return {};
    }

    if (settings.makeTileable && (quiltSize.x < 2 || quiltSize.y < 2))
    {
        return {};
    }

    if (blockSize.x >= sourceImage.getSize().x || blockSize.y >= sourceImage.getSize().y)
    {
        return {};
    }

    if (auto* select = std::get_if<WeightedBlockSelection>(&settings.blockSelection))
    {
        if (select->searchStride < 1)
        {
            return {};
        }
    }

    sf::Texture sourceTexture(sf::Vector2u{1, 1});
    if (settings.useGpuAcceleration)
    {
        sourceTexture.loadFromImage(sourceImage);
        sourceTexture.setSmooth(0);
    }

    sf::Image quiltImage;
    quiltImage.resize(sf::Vector2u(quiltSize.componentWiseMul(blockSize - overlap) + overlap));

    sf::Image seamsImage;
    if (settings.showSeams)
    {
        seamsImage.resize(quiltImage.getSize(), sf::Color::Transparent);
    }

    const bool needSources = settings.makeTileable;

    std::vector<sf::Vector2i> blockSources;
    if (needSources)
    {
        blockSources.reserve(quiltSize.x * quiltSize.y);
    }

    std::default_random_engine rng(settings.seed);

    for (int y = 0; y < quiltSize.y; y++)
    {
        for (int x = 0; x < quiltSize.x; x++)
        {
            const auto blockPos = (blockSize - overlap).componentWiseMul({ x, y });

            sf::Vector2i srcPos{};
            if (settings.makeTileable && (x == quiltSize.x - 1 || y == quiltSize.y - 1))
            {
                if (x == quiltSize.x - 1)
                {
                    srcPos = blockSources[y * quiltSize.x];
                }
                else
                {
                    srcPos = blockSources[x];
                }
            }
            else if ((x == 0 && y == 0) || std::get_if<RandomBlockSelection>(&settings.blockSelection))
            {
                srcPos = selectRandomBlock(rng, sourceImage, blockSize);
            }
            else if (auto* select = std::get_if<WeightedBlockSelection>(&settings.blockSelection))
            {
                if (settings.useGpuAcceleration)
                {
                    srcPos = selectBestBlockGpu(*select, rng, sourceTexture, quiltImage, blockSize, blockPos, overlap);
                }
                else
                {
                    srcPos = selectBestBlockCpu(*select, rng, sourceImage, quiltImage, blockSize, blockPos, overlap);
                }
            }

            if (needSources)
            {
                blockSources.push_back(srcPos);
            }

            sf::Image blockImage{ sf::Vector2u(blockSize) };
            blockImage.copy(sourceImage, {}, { srcPos, blockSize });

            const sf::Vector2i topOverlap(blockSize.x, overlap.y);
            const sf::Vector2i leftOverlap(overlap.x, blockSize.y);

            const auto handleOverlap = [&]<Direction direction>(auto overlap)
            {
                std::vector<float> difference = imageDifference(quiltImage, blockImage, { blockPos, overlap });

                if(settings.showDifference)
                {
                    const auto maxDifference = *std::max_element(difference.begin(), difference.end());
                    for (int x = 0; x < difference.size(); x++)
                    {
                        const auto diff = difference[x] / maxDifference;
                        const auto color = sf::Color(255 * diff, 255 * diff, 255 * diff, 255);
                        const auto pos = sf::Vector2u(x % overlap.x, x / overlap.x);
                        blockImage.setPixel(pos, color);
                    }
                }

                if (settings.useLogCost)
                {
                    for (auto& cost : difference)
                    {
                        if (cost > 0)
                        {
                            cost = std::log(cost);
                        }
                    }
                }

                auto path = generatePath<direction>(difference, overlap);

                if (settings.blendSeams)
                {
                    for (const auto pos : path)
                    {
                        const auto c1 = quiltImage.getPixel(sf::Vector2u(pos + blockPos));
                        const auto c2 = blockImage.getPixel(sf::Vector2u(pos));
                        blockImage.setPixel(sf::Vector2u(pos), lerpColor(c1, c2, 0.5f));
                    }
                }

                if (settings.doCut)
                {
                    cutImage<direction>(blockImage, path);
                }

                if (settings.showSeams)
                {
                    for (auto pos : path)
                    {
                        seamsImage.setPixel(sf::Vector2u(pos.x, pos.y) + sf::Vector2u(blockPos), sf::Color::Red);
                    }
                }
            };

            if (blockPos.x > 0)
            {
                handleOverlap.template operator()<Direction::Horizontal>(leftOverlap);
            }

            if (blockPos.y > 0)
            {
                handleOverlap.template operator()<Direction::Vertical>(topOverlap);
            }

            quiltImage.copy(blockImage, sf::Vector2u(blockPos), {}, true);
        }
    }

    if (settings.showSeams)
    {
        quiltImage.copy(seamsImage, {}, {}, true);
    }

    if (settings.makeTileable)
    {
        const auto temp = std::move(quiltImage);
        const auto newQuiltDimension = sf::Vector2i(quiltImage.getSize()) - blockSize;
        quiltImage.resize(sf::Vector2u(newQuiltDimension));
        quiltImage.copy(temp, {}, { blockSize / 2, newQuiltDimension });
    }

    return quiltImage;
}

}
