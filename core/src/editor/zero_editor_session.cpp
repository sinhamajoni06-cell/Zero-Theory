#include "zero_editor_session.h"

#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <algorithm>

#include "MapObject.h"
#include "EditorState.h"

void RunMapEditorSession(sf::RenderWindow& window,
                          const std::string& projectNameIn,
                          unsigned int WINDOW_WIDTH,
                          unsigned int WINDOW_HEIGHT) {
    const float GRID_SIZE = 40.f;
    std::string projectName = projectNameIn;

    if (!window.isOpen()) return;

    std::filesystem::create_directories("main/assets/map/" + projectName);
    const std::string SAVE_PATH = "main/assets/map/" + projectName + "/map.json";

    sf::View camera(sf::FloatRect({0.f, 0.f}, {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
    window.setView(camera);

    // ---------------- Editor state ----------------
    EditorState editor;
    ObjectType currentType = ObjectType::Block;
    bool snapEnabled = true;
    bool panningCamera = false;
    sf::Vector2i lastMousePixel;

    // Drag state: either dragging the current selection, or box-selecting.
    bool draggingSelection = false;
    bool boxSelecting = false;
    sf::Vector2f dragStartWorld;

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea({0.f, 0.f}, {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)});
                camera.setSize(visibleArea.size);
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                float zoomFactor = (wheel->delta > 0) ? 0.9f : 1.1f;
                camera.zoom(zoomFactor);
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f worldPos = window.mapPixelToCoords(mousePressed->position, camera);
                bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                  sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                if (mousePressed->button == sf::Mouse::Button::Middle) {
                    panningCamera = true;
                    lastMousePixel = mousePressed->position;
                }
                else if (mousePressed->button == sf::Mouse::Button::Left) {
                    int hitIndex = -1;
                    const auto& objs = editor.objects();
                    for (int i = static_cast<int>(objs.size()) - 1; i >= 0; --i) {
                        if (PointInObject(objs[i], worldPos)) { hitIndex = i; break; }
                    }

                    if (hitIndex != -1) {
                        if (shiftHeld) {
                            editor.toggleSelect(hitIndex);
                        } else if (!editor.isSelected(hitIndex)) {
                            editor.selectOnly(hitIndex);
                        }
                        draggingSelection = true;
                        editor.beginChange();
                        dragStartWorld = worldPos;
                    } else if (shiftHeld) {
                        // Shift-drag on empty space = additive box-select.
                        boxSelecting = true;
                        dragStartWorld = worldPos;
                    } else {
                        // Empty space, no shift: start a box-select; if the
                        // mouse never moves we treat it as "place new object"
                        // on release instead (see MouseButtonReleased below).
                        boxSelecting = true;
                        dragStartWorld = worldPos;
                    }
                }
                else if (mousePressed->button == sf::Mouse::Button::Right) {
                    const auto& objs = editor.objects();
                    for (int i = static_cast<int>(objs.size()) - 1; i >= 0; --i) {
                        if (PointInObject(objs[i], worldPos)) {
                            editor.selectOnly(i);
                            editor.deleteSelected();
                            break;
                        }
                    }
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                sf::Vector2f worldPos = window.mapPixelToCoords(mouseReleased->position, camera);

                if (mouseReleased->button == sf::Mouse::Button::Middle) {
                    panningCamera = false;
                }
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    if (draggingSelection) {
                        editor.commitChange();
                        draggingSelection = false;
                    }
                    if (boxSelecting) {
                        float movedDist = std::hypot(worldPos.x - dragStartWorld.x, worldPos.y - dragStartWorld.y);
                        if (movedDist < 3.f) {
                            // Treated as a plain click on empty space -> place a new object.
                            MapObject obj;
                            obj.type = currentType;
                            obj.w = 40.f;
                            obj.h = 40.f;
                            obj.x = snapEnabled ? SnapValue(worldPos.x, GRID_SIZE) : worldPos.x;
                            obj.y = snapEnabled ? SnapValue(worldPos.y, GRID_SIZE) : worldPos.y;
                            editor.addObject(obj);
                        } else {
                            sf::FloatRect rect(
                                {std::min(dragStartWorld.x, worldPos.x), std::min(dragStartWorld.y, worldPos.y)},
                                {std::abs(worldPos.x - dragStartWorld.x), std::abs(worldPos.y - dragStartWorld.y)});
                            bool additive = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                            editor.selectInRect(rect, additive);
                        }
                        boxSelecting = false;
                    }
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                if (panningCamera) {
                    sf::Vector2i delta = mouseMoved->position - lastMousePixel;
                    camera.move({-static_cast<float>(delta.x) * (camera.getSize().x / WINDOW_WIDTH),
                                 -static_cast<float>(delta.y) * (camera.getSize().y / WINDOW_HEIGHT)});
                    lastMousePixel = mouseMoved->position;
                }
                else if (draggingSelection && editor.hasSelection()) {
                    sf::Vector2f worldPos = window.mapPixelToCoords(mouseMoved->position, camera);
                    float targetX = snapEnabled ? SnapValue(worldPos.x, GRID_SIZE) : worldPos.x;
                    float targetY = snapEnabled ? SnapValue(worldPos.y, GRID_SIZE) : worldPos.y;
                    editor.moveSelectedBy(targetX - dragStartWorld.x, targetY - dragStartWorld.y);
                    dragStartWorld = {targetX, targetY};
                }
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

                if (keyPressed->code == sf::Keyboard::Key::Num1) currentType = ObjectType::Block;
                else if (keyPressed->code == sf::Keyboard::Key::Num2) currentType = ObjectType::Spike;
                else if (keyPressed->code == sf::Keyboard::Key::Num3) currentType = ObjectType::Platform;
                else if (keyPressed->code == sf::Keyboard::Key::Num4) currentType = ObjectType::Coin;
                else if (keyPressed->code == sf::Keyboard::Key::G) snapEnabled = !snapEnabled;
                else if (keyPressed->code == sf::Keyboard::Key::Escape) editor.clearSelection();
                else if (keyPressed->code == sf::Keyboard::Key::Delete ||
                         keyPressed->code == sf::Keyboard::Key::Backspace) {
                    editor.deleteSelected();
                }
                else if (keyPressed->code == sf::Keyboard::Key::R) {
                    editor.rotateSelectedBy(15.f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::LBracket) {
                    editor.resizeSelectedBy(-5.f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::RBracket) {
                    editor.resizeSelectedBy(5.f);
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::S) {
                    editor.saveToFile(SAVE_PATH);
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::O) {
                    editor.loadFromFile(SAVE_PATH);
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::Z) {
                    editor.undo();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::Y) {
                    editor.redo();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::C) {
                    editor.copySelected();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::V) {
                    editor.paste(GRID_SIZE, GRID_SIZE);
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::A) {
                    sf::FloatRect everything({-100000.f, -100000.f}, {200000.f, 200000.f});
                    editor.selectInRect(everything, false);
                }
            }
        }

        window.setView(camera);

        window.clear(sf::Color(30, 30, 34));

        const auto& objs = editor.objects();
        for (int i = 0; i < static_cast<int>(objs.size()); ++i) {
            sf::RectangleShape shape = MakeShapeForObject(objs[i]);
            if (editor.isSelected(i)) {
                shape.setOutlineThickness(3.f);
                shape.setOutlineColor(sf::Color::White);
            }
            window.draw(shape);
        }

        if (boxSelecting) {
            sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window), camera);
            sf::RectangleShape marquee(
                {std::abs(mouseWorld.x - dragStartWorld.x), std::abs(mouseWorld.y - dragStartWorld.y)});
            marquee.setPosition(
                {std::min(dragStartWorld.x, mouseWorld.x), std::min(dragStartWorld.y, mouseWorld.y)});
            marquee.setFillColor(sf::Color(90, 90, 240, 40));
            marquee.setOutlineThickness(1.f);
            marquee.setOutlineColor(sf::Color(90, 90, 240, 180));
            window.draw(marquee);
        }

        window.display();
    }
}