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
    const std::string SAVE_PATH = "map.json";
    const float GRID_SIZE = 40.f;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Zero Theory - Map Editor");
    window.setFramerateLimit(60);

    sf::View camera(sf::FloatRect({0.f, 0.f}, {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
    window.setView(camera);

    sf::Font font;
    bool fontLoaded = font.openFromFile("main/assets/fonts/VT323-Regular.ttf");
    if (!fontLoaded) {
        std::cerr << "[MapEditor] Warning: failed to load font. UI text will not be shown." << std::endl;
    }

    std::vector<MapObject> objects;
    ObjectType currentType = ObjectType::Block;
    bool snapEnabled = true;
    int selectedIndex = -1;
    bool draggingObject = false;
    bool panningCamera = false;
    sf::Vector2i lastMousePixel;

    sf::Text hudText(font, "", 16);
    hudText.setFillColor(sf::Color::White);

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

        // Grid
        {
            sf::Vector2f viewCenter = camera.getCenter();
            sf::Vector2f viewSize = camera.getSize();
            float left = viewCenter.x - viewSize.x / 2.f;
            float right = viewCenter.x + viewSize.x / 2.f;
            float top = viewCenter.y - viewSize.y / 2.f;
            float bottom = viewCenter.y + viewSize.y / 2.f;

            float startX = std::floor(left / GRID_SIZE) * GRID_SIZE;
            float startY = std::floor(top / GRID_SIZE) * GRID_SIZE;

            sf::VertexArray gridLines(sf::PrimitiveType::Lines);
            sf::Color gridColor(60, 60, 66);

            for (float gx = startX; gx <= right; gx += GRID_SIZE) {
                gridLines.append(sf::Vertex{{gx, top}, gridColor});
                gridLines.append(sf::Vertex{{gx, bottom}, gridColor});
            }
            for (float gy = startY; gy <= bottom; gy += GRID_SIZE) {
                gridLines.append(sf::Vertex{{left, gy}, gridColor});
                gridLines.append(sf::Vertex{{right, gy}, gridColor});
            }
            window.draw(gridLines);
        }

        // Objects
        for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
            sf::RectangleShape shape = MakeShapeForObject(objects[i]);
            if (i == selectedIndex) {
                shape.setOutlineThickness(3.f);
                shape.setOutlineColor(sf::Color::White);
            }
            window.draw(shape);
        }

        // HUD (drawn in default view so it doesn't pan/zoom with the camera)
        if (fontLoaded) {
            window.setView(window.getDefaultView());

            std::ostringstream hud;
            hud << "Tool: " << ObjectTypeToString(currentType)
                << "   Snap: " << (snapEnabled ? "ON" : "OFF")
                << "   Objects: " << objects.size()
                << "   Selected: " << (selectedIndex == -1 ? "none" : std::to_string(selectedIndex))
                << "\n1-4 Select Type | LClick Place/Move | RClick Delete | MMB Pan | Wheel Zoom"
                << "\nR Rotate | [ ] Resize | Del Remove | Ctrl+S Save | Ctrl+O Load | Esc Deselect";

            hudText.setString(hud.str());
            hudText.setPosition({10.f, 10.f});
            window.draw(hudText);

            window.setView(camera);
        }

        window.display();
    }

    return 0;
}
