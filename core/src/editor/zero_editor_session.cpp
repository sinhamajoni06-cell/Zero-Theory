#include "zero_editor_session.h"

#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "MapObject.h"
#include "EditorState.h"

// ----------------------------------------------------------
// Inspector panel: shows/edits Type, X, Y, W, H, Rotation of a
// single selected object. All edits go through
// EditorState::beginChange()/commitChange() so they're undoable.
// ----------------------------------------------------------

namespace {

constexpr float kInspectorWidth = 220.f;
constexpr float kRowHeight = 34.f;
constexpr float kRowPad = 10.f;

// Canvas framing (the "small screen" the map is edited inside,
// inset from the window edges like Godot's 2D viewport).
constexpr float kCanvasMarginTop = 56.f;
constexpr float kCanvasMarginSides = 24.f;
constexpr float kCanvasMarginBottom = 24.f;

enum class InspectorField { None, X, Y, W, H, Rotation };
enum class ExitPrompt { None, ConfirmHome, ConfirmSaveHome, ConfirmSaveQuit };

struct InspectorRowLayout {
    sf::FloatRect typeRow;
    sf::FloatRect xRow, yRow, wRow, hRow, rotRow;
};

InspectorRowLayout ComputeInspectorLayout(unsigned int WINDOW_WIDTH) {
    InspectorRowLayout L;
    float panelX = static_cast<float>(WINDOW_WIDTH) - kInspectorWidth;
    float y = 40.f;
    L.typeRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight}); y += kRowHeight;
    L.xRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight}); y += kRowHeight;
    L.yRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight}); y += kRowHeight;
    L.wRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight}); y += kRowHeight;
    L.hRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight}); y += kRowHeight;
    L.rotRow = sf::FloatRect({panelX + kRowPad, y}, {kInspectorWidth - kRowPad * 2.f, kRowHeight});
    return L;
}

// Screen-space rect of the actual editable map canvas (excludes the
// inspector dock and the surrounding chrome margins).
sf::FloatRect ComputeCanvasRect(unsigned int WINDOW_WIDTH, unsigned int WINDOW_HEIGHT) {
    float left = kCanvasMarginSides;
    float top = kCanvasMarginTop;
    float right = static_cast<float>(WINDOW_WIDTH) - kInspectorWidth - kCanvasMarginSides;
    float bottom = static_cast<float>(WINDOW_HEIGHT) - kCanvasMarginBottom;
    if (right < left + 40.f) right = left + 40.f;
    if (bottom < top + 40.f) bottom = top + 40.f;
    return sf::FloatRect({left, top}, {right - left, bottom - top});
}

void ApplyCanvasViewport(sf::View& camera, unsigned int WINDOW_WIDTH, unsigned int WINDOW_HEIGHT) {
    sf::FloatRect canvas = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
    camera.setViewport(sf::FloatRect(
        {canvas.position.x / static_cast<float>(WINDOW_WIDTH), canvas.position.y / static_cast<float>(WINDOW_HEIGHT)},
        {canvas.size.x / static_cast<float>(WINDOW_WIDTH), canvas.size.y / static_cast<float>(WINDOW_HEIGHT)}));
}

std::string FormatNumber(float v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << v;
    return oss.str();
}

float ParseNumberOr(const std::string& s, float fallback) {
    try {
        size_t pos = 0;
        float v = std::stof(s, &pos);
        if (pos == 0) return fallback;
        return v;
    } catch (...) {
        return fallback;
    }
}

void DrawInspectorRow(sf::RenderWindow& window, sf::Font& uiFont,
                       const sf::FloatRect& bounds, const std::string& label,
                       const std::string& value, bool editing) {
    sf::RectangleShape bg(bounds.size);
    bg.setPosition(bounds.position);
    bg.setFillColor(editing ? sf::Color(60, 60, 100) : sf::Color(40, 40, 46));
    bg.setOutlineThickness(editing ? 2.f : 1.f);
    bg.setOutlineColor(editing ? sf::Color(120, 120, 240) : sf::Color(60, 60, 68));
    window.draw(bg);

    sf::Text labelText(uiFont, label, 14);
    labelText.setFillColor(sf::Color(150, 150, 160));
    labelText.setPosition({bounds.position.x + 8.f, bounds.position.y + 3.f});
    window.draw(labelText);

    sf::Text valueText(uiFont, value, 16);
    valueText.setFillColor(sf::Color::White);
    valueText.setPosition({bounds.position.x + 8.f, bounds.position.y + 15.f});
    window.draw(valueText);
}

// Small centered "Are you sure?" style modal with a title and a hint line.
void DrawConfirmModal(sf::RenderWindow& window, sf::Font& uiFont,
                       unsigned int WINDOW_WIDTH, unsigned int WINDOW_HEIGHT,
                       const std::string& title, const std::string& hint) {
    sf::RectangleShape dim({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
    dim.setFillColor(sf::Color(0, 0, 0, 140));
    window.draw(dim);

    float boxW = 420.f, boxH = 110.f;
    float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
    float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;

    sf::RectangleShape box({boxW, boxH});
    box.setPosition({boxX, boxY});
    box.setFillColor(sf::Color(38, 38, 44));
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color(90, 90, 240));
    window.draw(box);

    sf::Text titleText(uiFont, title, 20);
    titleText.setFillColor(sf::Color::White);
    titleText.setPosition({boxX + boxW / 2.f - titleText.getLocalBounds().size.x / 2.f, boxY + 22.f});
    window.draw(titleText);

    sf::Text hintText(uiFont, hint, 15);
    hintText.setFillColor(sf::Color(160, 160, 170));
    hintText.setPosition({boxX + boxW / 2.f - hintText.getLocalBounds().size.x / 2.f, boxY + 62.f});
    window.draw(hintText);
}

} // namespace

bool RunMapEditorSession(sf::RenderWindow& window,
                          const std::string& projectNameIn,
                          unsigned int WINDOW_WIDTH,
                          unsigned int WINDOW_HEIGHT,
                          sf::Font& uiFont) {
    const float GRID_SIZE = 40.f;
    std::string projectName = projectNameIn;

    if (!window.isOpen()) return true;

    std::filesystem::create_directories("main/assets/map/" + projectName);
    const std::string SAVE_PATH = "main/assets/map/" + projectName + "/map.json";

    sf::FloatRect initialCanvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
    sf::View camera(sf::FloatRect({0.f, 0.f}, initialCanvasRect.size));
    ApplyCanvasViewport(camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    window.setView(camera);

    // ---------------- Editor state ----------------
    EditorState editor;

    // Auto-load the project's existing map, if any.
    if (std::filesystem::exists(SAVE_PATH)) {
        editor.loadFromFile(SAVE_PATH);
    }
    size_t savedUndoDepth = editor.undoDepth();
    auto isDirty = [&]() { return editor.undoDepth() != savedUndoDepth; };

    ObjectType currentType = ObjectType::Block;
    bool snapEnabled = true;
    bool panningCamera = false;
    sf::Vector2i lastMousePixel;

    bool draggingSelection = false;
    bool boxSelecting = false;
    sf::Vector2f dragStartWorld;

    // ---------------- Inspector state ----------------
    InspectorField editingField = InspectorField::None;
    std::string editBuffer;
    sf::Clock cursorBlinkClock;

    // ---------------- Exit / unsaved-changes state ----------------
    ExitPrompt exitPrompt = ExitPrompt::None;
    bool returnToHome = false;

    auto singleSelectedIndex = [&]() -> int {
        if (editor.selection().size() != 1) return -1;
        return *editor.selection().begin();
    };

    auto beginEditingField = [&](InspectorField field, float currentValue) {
        editingField = field;
        editBuffer = FormatNumber(currentValue);
    };

    auto commitEditingField = [&]() {
        int idx = singleSelectedIndex();
        if (idx == -1 || editingField == InspectorField::None) {
            editingField = InspectorField::None;
            return;
        }
        auto& objs = editor.mutableObjects();
        if (idx < 0 || idx >= static_cast<int>(objs.size())) { editingField = InspectorField::None; return; }

        editor.beginChange();
        MapObject& obj = objs[idx];
        switch (editingField) {
            case InspectorField::X:        obj.x = ParseNumberOr(editBuffer, obj.x); break;
            case InspectorField::Y:        obj.y = ParseNumberOr(editBuffer, obj.y); break;
            case InspectorField::W:        obj.w = std::max(10.f, ParseNumberOr(editBuffer, obj.w)); break;
            case InspectorField::H:        obj.h = std::max(10.f, ParseNumberOr(editBuffer, obj.h)); break;
            case InspectorField::Rotation: obj.rotation = ParseNumberOr(editBuffer, obj.rotation); break;
            default: break;
        }
        editor.commitChange();
        editingField = InspectorField::None;
    };

    while (window.isOpen() && !returnToHome) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                if (exitPrompt == ExitPrompt::None) {
                    if (isDirty()) {
                        exitPrompt = ExitPrompt::ConfirmSaveQuit;
                    } else {
                        window.close();
                    }
                }
                continue;
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                WINDOW_WIDTH = resized->size.x;
                WINDOW_HEIGHT = resized->size.y;
                sf::FloatRect newCanvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                // Keep 1 world unit == 1 screen pixel inside the canvas instead
                // of stretching the old (pre-resize) world extent into it.
                float zoomRatio = camera.getSize().x / std::max(1.f, newCanvasRect.size.x);
                camera.setSize(newCanvasRect.size);
                camera.zoom(zoomRatio); // preserve whatever zoom level the user had
                ApplyCanvasViewport(camera, WINDOW_WIDTH, WINDOW_HEIGHT);
            }

            // ---- Exit / unsaved-changes modal takes priority over everything ----
            if (exitPrompt != ExitPrompt::None) {
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    bool yes = (keyPressed->code == sf::Keyboard::Key::Y || keyPressed->code == sf::Keyboard::Key::Enter);
                    bool no = (keyPressed->code == sf::Keyboard::Key::N);
                    bool cancel = (keyPressed->code == sf::Keyboard::Key::Escape || keyPressed->code == sf::Keyboard::Key::C);

                    if (exitPrompt == ExitPrompt::ConfirmHome) {
                        if (yes) {
                            exitPrompt = isDirty() ? ExitPrompt::ConfirmSaveHome : ExitPrompt::None;
                            if (exitPrompt == ExitPrompt::None) returnToHome = true;
                        } else if (no || cancel) {
                            exitPrompt = ExitPrompt::None;
                        }
                    } else if (exitPrompt == ExitPrompt::ConfirmSaveHome) {
                        if (yes) {
                            editor.saveToFile(SAVE_PATH);
                            returnToHome = true;
                            exitPrompt = ExitPrompt::None;
                        } else if (no) {
                            returnToHome = true;
                            exitPrompt = ExitPrompt::None;
                        } else if (cancel) {
                            exitPrompt = ExitPrompt::None;
                        }
                    } else if (exitPrompt == ExitPrompt::ConfirmSaveQuit) {
                        if (yes) {
                            editor.saveToFile(SAVE_PATH);
                            window.close();
                        } else if (no) {
                            window.close();
                        } else if (cancel) {
                            exitPrompt = ExitPrompt::None;
                        }
                    }
                }
                continue; // swallow all other input while a modal is up
            }

            // ---- Inspector text input takes priority while editing a field ----
            if (editingField != InspectorField::None) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    char32_t unicode = textEntered->unicode;
                    if (unicode == 8) {
                        if (!editBuffer.empty()) editBuffer.pop_back();
                    } else if (unicode >= 32 && unicode < 127) {
                        char c = static_cast<char>(unicode);
                        bool allowed = (c >= '0' && c <= '9') || c == '-' || c == '.';
                        if (allowed) editBuffer += c;
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        commitEditingField();
                    } else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        editingField = InspectorField::None;
                    }
                }
                continue;
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                float zoomFactor = (wheel->delta > 0) ? 0.9f : 1.1f;
                camera.zoom(zoomFactor);
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f screenPos(static_cast<float>(mousePressed->position.x),
                                        static_cast<float>(mousePressed->position.y));

                // ---- Inspector panel hit-test takes priority over world clicks ----
                int selIdx = singleSelectedIndex();
                if (mousePressed->button == sf::Mouse::Button::Left && selIdx != -1) {
                    InspectorRowLayout L = ComputeInspectorLayout(WINDOW_WIDTH);
                    const auto& objs = editor.objects();
                    const MapObject& obj = objs[selIdx];

                    if (L.typeRow.contains(screenPos)) {
                        editor.beginChange();
                        auto& mobjs = editor.mutableObjects();
                        int next = (static_cast<int>(mobjs[selIdx].type) + 1) % 4;
                        mobjs[selIdx].type = static_cast<ObjectType>(next);
                        editor.commitChange();
                        continue;
                    } else if (L.xRow.contains(screenPos)) { beginEditingField(InspectorField::X, obj.x); continue; }
                    else if (L.yRow.contains(screenPos)) { beginEditingField(InspectorField::Y, obj.y); continue; }
                    else if (L.wRow.contains(screenPos)) { beginEditingField(InspectorField::W, obj.w); continue; }
                    else if (L.hRow.contains(screenPos)) { beginEditingField(InspectorField::H, obj.h); continue; }
                    else if (L.rotRow.contains(screenPos)) { beginEditingField(InspectorField::Rotation, obj.rotation); continue; }
                }
                if (screenPos.x >= static_cast<float>(WINDOW_WIDTH) - kInspectorWidth) {
                    // Click landed inside the inspector dock but not on a row: ignore.
                    continue;
                }

                // Ignore clicks outside the editable canvas (the chrome margins).
                sf::FloatRect canvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                if (!canvasRect.contains(screenPos)) {
                    continue;
                }

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
                        boxSelecting = true;
                        dragStartWorld = worldPos;
                    } else {
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
                    sf::FloatRect canvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                    camera.move({-static_cast<float>(delta.x) * (camera.getSize().x / canvasRect.size.x),
                                 -static_cast<float>(delta.y) * (camera.getSize().y / canvasRect.size.y)});
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
                else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    if (editor.hasSelection()) {
                        editor.clearSelection();
                    } else {
                        exitPrompt = ExitPrompt::ConfirmHome;
                    }
                }
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
                    savedUndoDepth = editor.undoDepth();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::O) {
                    editor.loadFromFile(SAVE_PATH);
                    savedUndoDepth = editor.undoDepth();
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

        if (!window.isOpen() || returnToHome) break;

        // ---------------- Draw ----------------
        window.clear(sf::Color(16, 16, 19)); // chrome background outside the canvas (cleared once)

        sf::FloatRect canvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);

        // Paint the canvas background in screen space, using the window's
        // current (default, full-window) view, before switching to the
        // restricted camera viewport for world content.
        {
            sf::RectangleShape canvasBg(canvasRect.size);
            canvasBg.setPosition(canvasRect.position);
            canvasBg.setFillColor(sf::Color(30, 30, 34));
            window.draw(canvasBg);
        }

        window.setView(camera); // restricted to the inset canvas viewport; content below is clipped to it

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

        // ---------------- Screen-space overlay: canvas border, inspector, modals ----------------
        sf::View screenView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
        window.setView(screenView);

        sf::RectangleShape canvasBorder(canvasRect.size);
        canvasBorder.setPosition(canvasRect.position);
        canvasBorder.setFillColor(sf::Color::Transparent);
        canvasBorder.setOutlineThickness(2.f);
        canvasBorder.setOutlineColor(sf::Color(70, 70, 80));
        window.draw(canvasBorder);

        sf::Text projectLabel(uiFont, projectName + (isDirty() ? " *" : ""), 18);
        projectLabel.setFillColor(sf::Color(200, 200, 210));
        projectLabel.setPosition({kCanvasMarginSides, 16.f});
        window.draw(projectLabel);

        int selIdx = singleSelectedIndex();
        if (selIdx != -1) {
            const auto& objsForPanel = editor.objects();
            const MapObject& obj = objsForPanel[selIdx];

            sf::RectangleShape panelBg({kInspectorWidth, static_cast<float>(WINDOW_HEIGHT)});
            panelBg.setPosition({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth, 0.f});
            panelBg.setFillColor(sf::Color(24, 24, 28, 235));
            window.draw(panelBg);

            sf::Text header(uiFont, "Inspector", 18);
            header.setFillColor(sf::Color::White);
            header.setPosition({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth + kRowPad, 8.f});
            window.draw(header);

            InspectorRowLayout L = ComputeInspectorLayout(WINDOW_WIDTH);
            bool cursorOn = std::fmod(cursorBlinkClock.getElapsedTime().asSeconds(), 1.f) < 0.5f;

            DrawInspectorRow(window, uiFont, L.typeRow, "Type", ObjectTypeToString(obj.type), false);
            DrawInspectorRow(window, uiFont, L.xRow, "X",
                              editingField == InspectorField::X ? editBuffer + (cursorOn ? "_" : "") : FormatNumber(obj.x),
                              editingField == InspectorField::X);
            DrawInspectorRow(window, uiFont, L.yRow, "Y",
                              editingField == InspectorField::Y ? editBuffer + (cursorOn ? "_" : "") : FormatNumber(obj.y),
                              editingField == InspectorField::Y);
            DrawInspectorRow(window, uiFont, L.wRow, "W",
                              editingField == InspectorField::W ? editBuffer + (cursorOn ? "_" : "") : FormatNumber(obj.w),
                              editingField == InspectorField::W);
            DrawInspectorRow(window, uiFont, L.hRow, "H",
                              editingField == InspectorField::H ? editBuffer + (cursorOn ? "_" : "") : FormatNumber(obj.h),
                              editingField == InspectorField::H);
            DrawInspectorRow(window, uiFont, L.rotRow, "Rotation",
                              editingField == InspectorField::Rotation ? editBuffer + (cursorOn ? "_" : "") : FormatNumber(obj.rotation),
                              editingField == InspectorField::Rotation);
        }

        if (exitPrompt == ExitPrompt::ConfirmHome) {
            DrawConfirmModal(window, uiFont, WINDOW_WIDTH, WINDOW_HEIGHT,
                              "Return to Home page?", "Are you sure? [Y]es  /  [N]o");
        } else if (exitPrompt == ExitPrompt::ConfirmSaveHome) {
            DrawConfirmModal(window, uiFont, WINDOW_WIDTH, WINDOW_HEIGHT,
                              "You have unsaved changes!", "Save before leaving?  [Y]es  /  [N]o  /  [Esc] Cancel");
        } else if (exitPrompt == ExitPrompt::ConfirmSaveQuit) {
            DrawConfirmModal(window, uiFont, WINDOW_WIDTH, WINDOW_HEIGHT,
                              "You have unsaved changes!", "Save before closing?  [Y]es  /  [N]o  /  [Esc] Cancel");
        }

        window.display();
    }

    return !returnToHome;
}