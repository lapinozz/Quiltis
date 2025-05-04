#include <SFML/Graphics.hpp>

#include <imgui-SFML.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <chrono>
#include <iostream>

#include "nfd.h"

#include "quiltis.hpp"

int main()
{
    NFD_Init();

    auto window = sf::RenderWindow(sf::VideoMode({ 1400u, 900u }), "Quiltis");
    window.setFramerateLimit(144);
    if (!ImGui::SFML::Init(window))
    {
        return -1;
    }

    std::string texturePath;
    sf::Image sourceImg;
    sf::Texture sourceTexture;
    bool useSquareBlocks{true};
    float overlapPercentage = 1.f / 6.f;

    Quiltis::Settings settings;

    sf::Texture resultTexture;

    bool isDirty = true;

    sf::Clock clock;
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, clock.restart());

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("Root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
        {
            const auto leftPanelWidth = 400.f;
            ImGui::BeginChild("ChildL", ImVec2(leftPanelWidth, 0), ImGuiChildFlags_None);
            {
                ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_Text, sourceImg.getSize().x ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 0, 0, 255));
                isDirty = isDirty || ImGui::InputText("Source File", &texturePath);
                ImGui::SameLine();
                if (ImGui::Button("..."))
                {
                    nfdu8filteritem_t filter{ "Image File", "bmp,png,tga,jpg,gif,psd,hdr,pic,pnm" };
                    nfdopendialogu8args_t args = { 0 };
                    args.filterList = &filter;
                    args.filterCount = 1;
                    nfdu8char_t* outPath;
                    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                    if (result == NFD_OKAY)
                    {
                        texturePath = outPath;
                        NFD_FreePathU8(outPath);

                        isDirty = true;
                    }
                }
                ImGui::PopStyleColor();

                const auto textureSize = sourceImg.getSize();
                if (textureSize.x > 0 && textureSize.y > 0)
                {
                    ImGui::Text("texture size: %d x %d", textureSize.x, textureSize.y);

                    ImGuiStyle& style = ImGui::GetStyle();
                    float avail = ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f - 100.f;
                    const auto scale = std::min({ avail / textureSize.x, avail / textureSize.y, 1.f });

                    sf::Sprite sprite(sourceTexture);
                    sprite.scale({ scale, scale });
                    ImGui::SetCursorPosX((avail - sourceTexture.getSize().x * scale) / 2);

                    ImGui::Image(sprite);

                    const auto showVec = [&](auto name, auto& vec, auto speed, auto min, auto max)
                    {
                        if(useSquareBlocks)
                        {
                            ImGui::DragInt(name, &vec.x, speed, min, max);
                            vec.y = vec.x;
                        }
                        else
                        {
                            ImGui::DragInt2(name, &vec.x, speed, min, max);
                        }

                        isDirty = isDirty || ImGui::IsItemDeactivatedAfterEdit();
                    };

                    isDirty = isDirty || ImGui::Checkbox("Square Blocks", &useSquareBlocks);

                    showVec("Block Size", settings.blockSize, 1, 5, textureSize.x / 2);

                    ImGui::DragFloat("Overlap Percentage", &overlapPercentage, 0.01f, 0.001f, 0.5f);
                    isDirty = isDirty || ImGui::IsItemDeactivatedAfterEdit();

                    showVec("Quilt Size", settings.quiltSize, 0.2f, 2, 10);

                    int selectionIndex = settings.blockSelection.index();
                    const auto items = "Random\0Best";
                    if (ImGui::Combo("Block Selection", &selectionIndex, items))
                    {
                        isDirty = true;
                        if (selectionIndex == 0)
                        {
                            settings.blockSelection = Quiltis::RandomBlockSelection{};
                        }
                        else
                        {
                            settings.blockSelection = Quiltis::WeightedBlockSelection{};
                        }
                    }

                    if (auto* select = std::get_if<Quiltis::WeightedBlockSelection>(&settings.blockSelection))
                    {
                        ImGui::Indent(16.0f);

                        ImGui::DragInt("Search Stride", &select->searchStride, 0.1f, 1, 100);
                        isDirty = isDirty || ImGui::IsItemDeactivatedAfterEdit();

                        ImGui::DragFloat("Selection Span", &select->selectionSpan, 0.01f, 0.f, 1.f);
                        isDirty = isDirty || ImGui::IsItemDeactivatedAfterEdit();

                        ImGui::Unindent(16.0f);
                    }

                    ImGui::DragInt("Seed", &settings.seed, 0, 0, 10000);
                    isDirty = isDirty || ImGui::IsItemDeactivatedAfterEdit();

                    isDirty = isDirty || ImGui::Checkbox("Show Seams", &settings.showSeams);
                    isDirty = isDirty || ImGui::Checkbox("Blend Seams", &settings.blendSeams);
                    isDirty = isDirty || ImGui::Checkbox("Cut", &settings.doCut);
                    isDirty = isDirty || ImGui::Checkbox("Log Cost", &settings.useLogCost);
                    isDirty = isDirty || ImGui::Checkbox("Use Gpu", &settings.useGpuAcceleration);


                    if (ImGui::Button("Export"))
                    {
                        nfdu8filteritem_t filter{ "Image File", "bmp,png,tga,jpg,gif,psd,hdr,pic,pnm" };
                        nfdu8char_t* outPath;
                        nfdresult_t result = NFD_SaveDialog(&outPath, &filter, 1, nullptr, nullptr);
                        if (result == NFD_OKAY)
                        {
                            resultTexture.copyToImage().saveToFile(outPath);
                            NFD_FreePathU8(outPath);
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("ChildR", ImVec2(0, 0), ImGuiChildFlags_None);
            {
                const auto min = ImGui::GetWindowContentRegionMin();
                const auto max = ImGui::GetWindowContentRegionMax();

                sf::Vector2f area{ max.x - min.x, max.y - min.y };

                const auto scale = std::min({ area.x / resultTexture.getSize().x, area.y / resultTexture.getSize().y, 1.f });

                sf::Sprite sprite{ resultTexture };
                sprite.scale({ scale, scale });
                ImGui::Image(sprite);
            }
            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        if (isDirty)
        {

            isDirty = false;

            if (!sourceImg.loadFromFile(texturePath))
            {
                sourceImg.resize({}, nullptr);
            }
            else
            {
                sourceTexture.loadFromImage(sourceImg);

                settings.overlap = sf::Vector2i(settings.blockSize.x * overlapPercentage, settings.blockSize.y * overlapPercentage);

                std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
                auto resultImg = Quiltis::quilt(sourceImg, settings);
                std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();

                resultTexture.loadFromImage(resultImg);


                std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "[ms]" << std::endl;
            }
        }

        window.clear();

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}
