// ==========================================================
//  Zero Theory - Map Editor (zero_map.cpp)
//  A lightweight, Geometry-Dash-style level editor.
//
//  Controls:
//    Left Click        -> Place object (or select/drag existing object)
//    Right Click        -> Delete object under cursor
//    Middle Mouse Drag  -> Pan camera
//    Mouse Wheel        -> Zoom in/out
//    1 - 4              -> Choose object type (Block / Spike / Platform / Coin)
//    G                  -> Toggle grid snapping
//    R                  -> Rotate selected object (+15 deg)
//    [ / ]              -> Shrink / Grow selected object
//    Delete / Backspace -> Delete selected object
//    Ctrl + S           -> Save map to JSON
//    Ctrl + O           -> Load map from JSON
//    Escape             -> Deselect current object
// ==========================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <iostream>
#include <optional>
#include <filesystem>

// ----------------------------------------------------------
// Data model
// ----------------------------------------------------------

enum class ObjectType {
    Block = 0,
    Spike,
    Platform,
    Coin
};

std::string ObjectTypeToString(ObjectType type) {
    switch (type) {
        case ObjectType::Block:    return "block";
        case ObjectType::Spike:    return "spike";
        case ObjectType::Platform: return "platform";
        case ObjectType::Coin:     return "coin";
    }
    return "block";
}

ObjectType ObjectTypeFromString(const std::string& s) {
    if (s == "spike")    return ObjectType::Spike;
    if (s == "platform") return ObjectType::Platform;
    if (s == "coin")     return ObjectType::Coin;
    return ObjectType::Block;
}

sf::Color ObjectTypeColor(ObjectType type) {
    switch (type) {
        case ObjectType::Block:    return sf::Color(120, 120, 130);
        case ObjectType::Spike:    return sf::Color(220, 60, 60);
        case ObjectType::Platform: return sf::Color(90, 160, 220);
        case ObjectType::Coin:     return sf::Color(240, 200, 60);
    }
    return sf::Color::White;
}

struct MapObject {
    ObjectType type;
    float x, y;         // world position (center)
    float w, h;          // size
    float rotation = 0.f; // degrees
};

// ----------------------------------------------------------
// Minimal JSON writer / reader (schema-specific, no external lib)
// ----------------------------------------------------------

void SaveMapToJson(const std::string& path, const std::vector<MapObject>& objects) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[MapEditor] Failed to open file for saving: " << path << std::endl;
        return;
    }

    file << "{\n  \"objects\": [\n";
    for (size_t i = 0; i < objects.size(); ++i) {
        const MapObject& obj = objects[i];
        file << "    { \"type\": \"" << ObjectTypeToString(obj.type) << "\", "
             << "\"x\": " << obj.x << ", "
             << "\"y\": " << obj.y << ", "
             << "\"w\": " << obj.w << ", "
             << "\"h\": " << obj.h << ", "
             << "\"rotation\": " << obj.rotation << " }";
        if (i + 1 < objects.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n}\n";

    file.close();
    std::cout << "[MapEditor] Saved " << objects.size() << " objects to " << path << std::endl;
}

// Extremely small hand-rolled parser tailored to the format written above.
// It looks for repeating "key": value pairs inside each { ... } block.
std::vector<MapObject> LoadMapFromJson(const std::string& path) {
    std::vector<MapObject> objects;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[MapEditor] Failed to open file for loading: " << path << std::endl;
        return objects;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    size_t pos = 0;
    while (true) {
        size_t objStart = content.find('{', pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;

        // Skip the outer "{ objects: [" wrapper (it has no "type" key).
        std::string block = content.substr(objStart, objEnd - objStart);
        pos = objEnd + 1;

        if (block.find("\"type\"") == std::string::npos) continue;

        MapObject obj{};
        auto extractString = [&](const std::string& key) -> std::string {
            size_t k = block.find("\"" + key + "\"");
            if (k == std::string::npos) return "";
            size_t colon = block.find(':', k);
            size_t q1 = block.find('"', colon);
            size_t q2 = block.find('"', q1 + 1);
            if (q1 == std::string::npos || q2 == std::string::npos) return "";
            return block.substr(q1 + 1, q2 - q1 - 1);
        };
        auto extractFloat = [&](const std::string& key) -> float {
            size_t k = block.find("\"" + key + "\"");
            if (k == std::string::npos) return 0.f;
            size_t colon = block.find(':', k);
            size_t end = block.find_first_of(",}", colon);
            std::string numStr = block.substr(colon + 1, end - colon - 1);
            try { return std::stof(numStr); } catch (...) { return 0.f; }
        };

        obj.type = ObjectTypeFromString(extractString("type"));
        obj.x = extractFloat("x");
        obj.y = extractFloat("y");
        obj.w = extractFloat("w");
        obj.h = extractFloat("h");
        obj.rotation = extractFloat("rotation");

        objects.push_back(obj);
    }

    std::cout << "[MapEditor] Loaded " << objects.size() << " objects from " << path << std::endl;
    return objects;
}

// ----------------------------------------------------------
// Helpers
// ----------------------------------------------------------

float SnapValue(float value, float gridSize) {
    return std::round(value / gridSize) * gridSize;
}

sf::RectangleShape MakeShapeForObject(const MapObject& obj) {
    sf::RectangleShape shape({obj.w, obj.h});
    shape.setOrigin({obj.w / 2.f, obj.h / 2.f});
    shape.setPosition({obj.x, obj.y});
    shape.setRotation(sf::degrees(obj.rotation));
    shape.setFillColor(ObjectTypeColor(obj.type));
    shape.setOutlineThickness(1.f);
    shape.setOutlineColor(sf::Color(0, 0, 0, 180));
    return shape;
}

bool PointInObject(const MapObject& obj, sf::Vector2f point) {
    // Simple AABB hit test (rotation ignored for selection simplicity).
    float left = obj.x - obj.w / 2.f;
    float right = obj.x + obj.w / 2.f;
    float top = obj.y - obj.h / 2.f;
    float bottom = obj.y + obj.h / 2.f;
    return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

// ----------------------------------------------------------
// Main
// ----------------------------------------------------------

int main() {
    const unsigned int WINDOW_WIDTH = 1280;
    const unsigned int WINDOW_HEIGHT = 720;
    const float GRID_SIZE = 40.f;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Zero Theory - Map Editor");
    window.setFramerateLimit(60);

    sf::Font uiFont;
    bool uiFontLoaded = uiFont.openFromFile("main/assets/fonts/VT323-Regular.ttf");

    sf::Font titleFont;
    bool titleFontLoaded = titleFont.openFromFile("main/assets/fonts/Honk-Regular-VariableFont_MORF,SHLN.ttf");
    if (!titleFontLoaded) titleFontLoaded = false; // fallback handled at draw time

    sf::Texture arrowIconTexture;
    bool arrowIconLoaded = arrowIconTexture.loadFromFile("main/assets/images/UI/icons/icon_more_white.png");

    // ---------------------------------------------------
    // HOME PAGE (project browser + creation)
    // ---------------------------------------------------
    std::filesystem::create_directories("main/assets/map");

    struct ProjectEntry {
        std::string name;
        uintmax_t sizeBytes = 0;
    };

    auto scanProjects = [&]() -> std::vector<ProjectEntry> {
        std::vector<ProjectEntry> found;
        for (const auto& entry : std::filesystem::directory_iterator("main/assets/map")) {
            if (!entry.is_directory()) continue;
            ProjectEntry p;
            p.name = entry.path().filename().string();
            std::filesystem::path mapFile = entry.path() / "map.json";
            if (std::filesystem::exists(mapFile)) {
                p.sizeBytes = std::filesystem::file_size(mapFile);
            }
            found.push_back(p);
        }
        return found;
    };

    auto formatSize = [](uintmax_t bytes) -> std::string {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        double kb = bytes / 1024.0;
        if (kb < 1024.0) {
            std::ostringstream oss;
            oss.precision(1);
            oss << std::fixed << kb << " KB";
            return oss.str();
        }
        std::ostringstream oss;
        oss.precision(1);
        oss << std::fixed << (kb / 1024.0) << " MB";
        return oss.str();
    };

    std::vector<ProjectEntry> projects = scanProjects();

    std::string projectName;
    bool typingStarted = false;
    bool enteringName = false;   // true while typing a new project name
    bool creatingProject = true; // true while on the home page
    int selectedIndex = 0;       // 0 = "Create New +", 1..N = existing projects
    bool isNewProjectCreation = false; // true only when finalizing a brand-new project
    int lastClickedIndex = -1;
    sf::Clock doubleClickClock;
    const float DOUBLE_CLICK_MS = 350.f;

    const std::string howToUseText =
        "HOW TO USE\n"
        "\n"
        "-- Getting Started --\n"
        "Select a project from the list on the\n"
        "left, or create a new one. Each project\n"
        "is saved as its own map.json file.\n"
        "\n"
        "-- Controls --\n"
        "Left Click     Place / Select & Drag\n"
        "Right Click    Delete object\n"
        "Middle Mouse   Pan camera\n"
        "Mouse Wheel    Zoom in / out\n"
        "1 - 4          Choose object type\n"
        "G              Toggle grid snapping\n"
        "R              Rotate selected object\n"
        "[  /  ]        Shrink / Grow selected\n"
        "Delete         Remove selected object\n"
        "Ctrl + S       Save map\n"
        "Ctrl + O       Load map\n"
        "Escape         Deselect\n"
        "\n"
        "-- Tips --\n"
        "Enable grid snapping while blocking\n"
        "out a level, disable it for fine detail\n"
        "placement. Save often with Ctrl+S.\n"
        "\n"
        "-- Workflow --\n"
        "1. Create or open a project\n"
        "2. Place and arrange objects\n"
        "3. Save your map\n"
        "4. Load it back into the Game Engine";

    while (window.isOpen() && creatingProject) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (enteringName) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    char32_t unicode = textEntered->unicode;
                    if (unicode == 8) { // Backspace
                        if (!projectName.empty()) projectName.pop_back();
                        typingStarted = true;
                    }
                    else if (unicode >= 32 && unicode < 127) {
                        if (!typingStarted) {
                            projectName.clear();
                            typingStarted = true;
                        }
                        if (projectName.size() < 40) {
                            projectName += static_cast<char>(unicode);
                        }
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (projectName.empty()) projectName = "New Project";
                        isNewProjectCreation = true;
                        creatingProject = false;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        enteringName = false;
                        typingStarted = false;
                        projectName.clear();
                    }
                }
            }
            else {
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    int itemCount = static_cast<int>(projects.size()) + 1; // +1 for "Create New"
                    if (keyPressed->code == sf::Keyboard::Key::Up) {
                        selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Down) {
                        selectedIndex = (selectedIndex + 1) % itemCount;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (selectedIndex == 0) {
                            enteringName = true;
                            typingStarted = false;
                            projectName = "New Project";
                        } else {
                            projectName = projects[selectedIndex - 1].name;
                            creatingProject = false;
                        }
                    }
                }

                if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2f mousePos = window.mapPixelToCoords(mousePressed->position, window.getDefaultView());
                        float dividerX = WINDOW_WIDTH * 0.62f;

                        if (projects.empty()) {
                            sf::FloatRect cardBounds({dividerX / 2.f - 160.f, WINDOW_HEIGHT / 2.f - 50.f}, {320.f, 100.f});
                            if (cardBounds.contains(mousePos)) {
                                bool isDoubleClick = (lastClickedIndex == 0 && doubleClickClock.getElapsedTime().asMilliseconds() < DOUBLE_CLICK_MS);
                                doubleClickClock.restart();
                                lastClickedIndex = 0;
                                if (isDoubleClick) {
                                    enteringName = true;
                                    typingStarted = false;
                                    projectName = "New Project";
                                    lastClickedIndex = -1;
                                }
                            }
                        } else {
                            float listX = 40.f;
                            float listY = 110.f;
                            float rowHeight = 56.f;

                            for (int i = -1; i < static_cast<int>(projects.size()); ++i) {
                                int itemIndex = i + 1;
                                float y = listY + itemIndex * rowHeight;
                                sf::FloatRect rowBounds({listX, y}, {dividerX - 60.f, rowHeight - 8.f});
                                if (rowBounds.contains(mousePos)) {
                                    selectedIndex = itemIndex;

                                    bool isDoubleClick = (lastClickedIndex == itemIndex && doubleClickClock.getElapsedTime().asMilliseconds() < DOUBLE_CLICK_MS);
                                    doubleClickClock.restart();
                                    lastClickedIndex = itemIndex;

                                    if (isDoubleClick) {
                                        if (i == -1) {
                                            enteringName = true;
                                            typingStarted = false;
                                            projectName = "New Project";
                                        } else {
                                            projectName = projects[i].name;
                                            creatingProject = false;
                                        }
                                        lastClickedIndex = -1;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        window.clear(sf::Color(20, 20, 24));

        // ---- Top-left title ----
        if (titleFontLoaded) {
            sf::Text title(titleFont, "Zero Theory - Map Editor", 32);
            title.setFillColor(sf::Color::White);
            title.setPosition({24.f, 18.f});
            window.draw(title);
        } else if (uiFontLoaded) {
            sf::Text title(uiFont, "Zero Theory - Map Editor", 32);
            title.setFillColor(sf::Color::White);
            title.setPosition({24.f, 18.f});
            window.draw(title);
        }

        float dividerX = WINDOW_WIDTH * 0.62f;

        if (uiFontLoaded) {
            if (enteringName) {
                // ---- Name entry overlay ----
                sf::RectangleShape card({480.f, 90.f});
                card.setFillColor(sf::Color(38, 38, 44));
                card.setOutlineThickness(2.f);
                card.setOutlineColor(sf::Color(90, 90, 240));
                card.setPosition({dividerX / 2.f - 240.f, WINDOW_HEIGHT / 2.f - 45.f});
                window.draw(card);

                sf::Text label(uiFont, "Project Name", 18);
                label.setFillColor(sf::Color(150, 150, 160));
                label.setPosition({dividerX / 2.f - 220.f, WINDOW_HEIGHT / 2.f - 35.f});
                window.draw(label);

                sf::Text nameText(uiFont, projectName + "_", 28);
                nameText.setFillColor(sf::Color::White);
                nameText.setPosition({dividerX / 2.f - 220.f, WINDOW_HEIGHT / 2.f - 10.f});
                window.draw(nameText);

                sf::Text hint(uiFont, "Press Enter to create, Esc to cancel", 16);
                hint.setFillColor(sf::Color(130, 130, 140));
                hint.setPosition({dividerX / 2.f - hint.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f + 65.f});
                window.draw(hint);
            }
            else if (projects.empty()) {
                // ---- No projects: centered "Create New +" box ----
                sf::RectangleShape card({320.f, 100.f});
                card.setFillColor(sf::Color(34, 34, 40));
                card.setOutlineThickness(2.f);
                card.setOutlineColor(sf::Color(90, 90, 240));
                card.setPosition({dividerX / 2.f - 160.f, WINDOW_HEIGHT / 2.f - 50.f});
                window.draw(card);

                sf::Text plus(uiFont, "Create New +", 28);
                plus.setFillColor(sf::Color::White);
                plus.setPosition({dividerX / 2.f - plus.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f - 18.f});
                window.draw(plus);

                sf::Text hint(uiFont, "Press Enter to start a new project", 16);
                hint.setFillColor(sf::Color(130, 130, 140));
                hint.setPosition({dividerX / 2.f - hint.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f + 60.f});
                window.draw(hint);
            }
            else {
                // ---- Project list ----
                float listX = 40.f;
                float listY = 110.f;
                float rowHeight = 56.f;

                for (int i = -1; i < static_cast<int>(projects.size()); ++i) {
                    int itemIndex = i + 1; // 0 = Create New, 1..N = projects
                    float y = listY + itemIndex * rowHeight;

                    sf::RectangleShape row({dividerX - 60.f, rowHeight - 8.f});
                    row.setPosition({listX, y});
                    bool isSelected = (selectedIndex == itemIndex);
                    row.setFillColor(isSelected ? sf::Color(50, 50, 90) : sf::Color(32, 32, 38));
                    if (isSelected) {
                        row.setOutlineThickness(2.f);
                        row.setOutlineColor(sf::Color(90, 90, 240));
                    }
                    window.draw(row);

                    if (i == -1) {
                        sf::Text plus(uiFont, "Create New +", 22);
                        plus.setFillColor(sf::Color::White);
                        plus.setPosition({listX + 16.f, y + 12.f});
                        window.draw(plus);
                    } else {
                        const ProjectEntry& p = projects[i];
                        sf::Text nameText(uiFont, p.name, 22);
                        nameText.setFillColor(sf::Color::White);
                        nameText.setPosition({listX + 16.f, y + 6.f});
                        window.draw(nameText);

                        sf::Text sizeText(uiFont, formatSize(p.sizeBytes), 14);
                        sizeText.setFillColor(sf::Color(150, 150, 160));
                        sizeText.setPosition({listX + 16.f, y + 30.f});
                        window.draw(sizeText);

                        if (arrowIconLoaded) {
                            sf::Sprite arrowSprite(arrowIconTexture);
                            sf::Vector2u texSize = arrowIconTexture.getSize();
                            float iconSize = 24.f;
                            float scale = (texSize.x > 0) ? (iconSize / static_cast<float>(texSize.x)) : 1.f;
                            arrowSprite.setScale({scale, scale});
                            arrowSprite.setPosition({listX + (dividerX - 60.f) - iconSize - 16.f, y + (rowHeight - 8.f) / 2.f - iconSize / 2.f});
                            window.draw(arrowSprite);
                        }
                    }
                }
            }

            // ---- Right divider panel: How To Use ----
            sf::RectangleShape divider({2.f, static_cast<float>(WINDOW_HEIGHT)});
            divider.setPosition({dividerX, 0.f});
            divider.setFillColor(sf::Color(60, 60, 66));
            window.draw(divider);

            sf::Text howTo(uiFont, howToUseText, 16);
            howTo.setFillColor(sf::Color(210, 210, 215));
            howTo.setPosition({dividerX + 30.f, 30.f});
            howTo.setLineSpacing(1.2f);
            window.draw(howTo);
        }

        window.display();
    }

    if (!window.isOpen()) return 0;

    if (isNewProjectCreation) {
        std::string baseName = projectName;
        std::string finalName = baseName;
        int suffix = 1;
        while (std::filesystem::exists("main/assets/map/" + finalName)) {
            finalName = baseName + " (L" + std::to_string(suffix) + ")";
            suffix++;
        }
        projectName = finalName;
    }

    std::filesystem::create_directories("main/assets/map/" + projectName);
    const std::string SAVE_PATH = "main/assets/map/" + projectName + "/map.json";

    sf::View camera(sf::FloatRect({0.f, 0.f}, {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
    window.setView(camera);

    std::vector<MapObject> objects;
    ObjectType currentType = ObjectType::Block;
    bool snapEnabled = true;
    selectedIndex = -1;
    bool draggingObject = false;
    bool panningCamera = false;
    sf::Vector2i lastMousePixel;

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea({0.f, 0.f}, {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)});
                camera.setSize(visibleArea.size);
            }

            // ---- Mouse wheel: zoom ----
            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                float zoomFactor = (wheel->delta > 0) ? 0.9f : 1.1f;
                camera.zoom(zoomFactor);
            }

            // ---- Mouse buttons ----
            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f worldPos = window.mapPixelToCoords(mousePressed->position, camera);

                if (mousePressed->button == sf::Mouse::Button::Middle) {
                    panningCamera = true;
                    lastMousePixel = mousePressed->position;
                }
                else if (mousePressed->button == sf::Mouse::Button::Left) {
                    // Try to select an existing object first (topmost = last drawn).
                    int hitIndex = -1;
                    for (int i = static_cast<int>(objects.size()) - 1; i >= 0; --i) {
                        if (PointInObject(objects[i], worldPos)) {
                            hitIndex = i;
                            break;
                        }
                    }

                    if (hitIndex != -1) {
                        selectedIndex = hitIndex;
                        draggingObject = true;
                    } else {
                        // Place a new object.
                        MapObject obj;
                        obj.type = currentType;
                        obj.w = 40.f;
                        obj.h = 40.f;
                        obj.x = snapEnabled ? SnapValue(worldPos.x, GRID_SIZE) : worldPos.x;
                        obj.y = snapEnabled ? SnapValue(worldPos.y, GRID_SIZE) : worldPos.y;
                        objects.push_back(obj);
                        selectedIndex = static_cast<int>(objects.size()) - 1;
                    }
                }
                else if (mousePressed->button == sf::Mouse::Button::Right) {
                    for (int i = static_cast<int>(objects.size()) - 1; i >= 0; --i) {
                        if (PointInObject(objects[i], worldPos)) {
                            objects.erase(objects.begin() + i);
                            if (selectedIndex == i) selectedIndex = -1;
                            break;
                        }
                    }
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Middle) {
                    panningCamera = false;
                }
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    draggingObject = false;
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                if (panningCamera) {
                    sf::Vector2i delta = mouseMoved->position - lastMousePixel;
                    camera.move({-static_cast<float>(delta.x) * (camera.getSize().x / WINDOW_WIDTH),
                                 -static_cast<float>(delta.y) * (camera.getSize().y / WINDOW_HEIGHT)});
                    lastMousePixel = mouseMoved->position;
                }
                else if (draggingObject && selectedIndex != -1) {
                    sf::Vector2f worldPos = window.mapPixelToCoords(mouseMoved->position, camera);
                    MapObject& obj = objects[selectedIndex];
                    obj.x = snapEnabled ? SnapValue(worldPos.x, GRID_SIZE) : worldPos.x;
                    obj.y = snapEnabled ? SnapValue(worldPos.y, GRID_SIZE) : worldPos.y;
                }
            }

            // ---- Keyboard ----
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                bool ctrlHeld = keyPressed->control;

                if (keyPressed->code == sf::Keyboard::Key::Num1) currentType = ObjectType::Block;
                else if (keyPressed->code == sf::Keyboard::Key::Num2) currentType = ObjectType::Spike;
                else if (keyPressed->code == sf::Keyboard::Key::Num3) currentType = ObjectType::Platform;
                else if (keyPressed->code == sf::Keyboard::Key::Num4) currentType = ObjectType::Coin;
                else if (keyPressed->code == sf::Keyboard::Key::G) snapEnabled = !snapEnabled;
                else if (keyPressed->code == sf::Keyboard::Key::Escape) selectedIndex = -1;
                else if ((keyPressed->code == sf::Keyboard::Key::Delete ||
                          keyPressed->code == sf::Keyboard::Key::Backspace) && selectedIndex != -1) {
                    objects.erase(objects.begin() + selectedIndex);
                    selectedIndex = -1;
                }
                else if (keyPressed->code == sf::Keyboard::Key::R && selectedIndex != -1) {
                    objects[selectedIndex].rotation += 15.f;
                    if (objects[selectedIndex].rotation >= 360.f) objects[selectedIndex].rotation -= 360.f;
                }
                else if (keyPressed->code == sf::Keyboard::Key::LBracket && selectedIndex != -1) {
                    objects[selectedIndex].w = std::max(10.f, objects[selectedIndex].w - 5.f);
                    objects[selectedIndex].h = std::max(10.f, objects[selectedIndex].h - 5.f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::RBracket && selectedIndex != -1) {
                    objects[selectedIndex].w += 5.f;
                    objects[selectedIndex].h += 5.f;
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::S) {
                    SaveMapToJson(SAVE_PATH, objects);
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::O) {
                    objects = LoadMapFromJson(SAVE_PATH);
                    selectedIndex = -1;
                }
            }
        }

        window.setView(camera);

        // ---- Draw ----
        window.clear(sf::Color(30, 30, 34));

        // Objects
        for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
            sf::RectangleShape shape = MakeShapeForObject(objects[i]);
            if (i == selectedIndex) {
                shape.setOutlineThickness(3.f);
                shape.setOutlineColor(sf::Color::White);
            }
            window.draw(shape);
        }

        window.display();
    }

    return 0;
}
