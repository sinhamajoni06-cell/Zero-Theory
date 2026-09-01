// ==========================================================
//  Zero Theory - Map Editor (zero_map.cpp)
//  A lightweight, Geometry-Dash-style level editor.
//
//  Controls:
//    Left Click          -> Place object / select (click empty = new object)
//    Shift + Left Click  -> Add/remove object under cursor from selection
//    Left Click + Drag   -> Box-select (on empty space) or move selection
//    Right Click         -> Delete object under cursor
//    Middle Mouse Drag   -> Pan camera
//    Mouse Wheel         -> Zoom in/out
//    1 - 4               -> Choose object type (Block / Spike / Platform / Coin)
//    G                   -> Toggle grid snapping
//    R                   -> Rotate selection (+15 deg)
//    [ / ]               -> Shrink / Grow selection
//    Delete / Backspace  -> Delete selection
//    Ctrl + Z / Ctrl + Y -> Undo / Redo
//    Ctrl + C / Ctrl + V -> Copy / Paste selection
//    Ctrl + A            -> Select all
//    Ctrl + S            -> Save map to JSON
//    Ctrl + O            -> Load map from JSON
//    Escape              -> Deselect
// ==========================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <algorithm>

#include "MapObject.h"
#include "EditorState.h"

// ----------------------------------------------------------
// Home page (project browser / creation) - unchanged behavior
// from the original editor, just kept out of the hot edit loop.
// ----------------------------------------------------------

struct ProjectEntry {
    std::string name;
    uintmax_t sizeBytes = 0;
};

static std::vector<ProjectEntry> ScanProjects() {
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
}

static std::string FormatSize(uintmax_t bytes) {
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
}

// Recolors an icon by ignoring its own RGB and using only its alpha
// channel as a mask - required because our icon PNGs are pure black
// shapes on transparent backgrounds, and black can't be tinted by
// ordinary color modulation or additive blending (0 stays 0 either way).
static const char* kIconTintFragmentShader = R"(
    uniform sampler2D source;
    uniform vec4 tintColor;
    void main() {
        float a = texture2D(source, gl_TexCoord[0].xy).a;
        gl_FragColor = vec4(tintColor.rgb, tintColor.a * a);
    }
)";

// ----------------------------------------------------------
// Main
// ----------------------------------------------------------

int main() {
    const unsigned int WINDOW_WIDTH = 1280;
    const unsigned int WINDOW_HEIGHT = 720;
    const float GRID_SIZE = 40.f;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Zero Theory - Map Editor");
    window.setFramerateLimit(60);
    window.setMinimumSize(sf::Vector2u(640, 360));

    sf::View homeView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}));

    sf::Font uiFont;
    bool uiFontLoaded = uiFont.openFromFile("main/assets/fonts/VT323-Regular.ttf");

    sf::Font titleFont;
    bool titleFontLoaded = titleFont.openFromFile("main/assets/fonts/Honk-Regular-VariableFont_MORF,SHLN.ttf");

    sf::Texture arrowIconTexture;
    bool arrowIconLoaded = arrowIconTexture.loadFromFile("main/assets/images/UI/icons/icon_more_white.png");

    std::filesystem::create_directories("main/assets/map");

    std::vector<ProjectEntry> projects = ScanProjects();

    std::string projectName;
    bool typingStarted = false;
    bool enteringName = false;
    bool creatingProject = true;
    int selectedIndex = 0;
    bool isNewProjectCreation = false;
    int lastClickedIndex = -1;
    sf::Clock doubleClickClock;
    const float DOUBLE_CLICK_MS = 350.f;
    float listScrollOffset = 0.f;
    float listScrollTarget = 0.f;
    bool showHowTo = true;
    float howToAnim = 1.f; // 0 = fully closed, 1 = fully open
    sf::Clock homeAnimClock;

    sf::Texture closeIconTexture;
    bool closeIconLoaded = closeIconTexture.loadFromFile("main/assets/images/UI/icons/icon_close_white.png");

    sf::Texture editIconTexture;
    bool editIconLoaded = editIconTexture.loadFromFile("main/assets/images/UI/icons/edit.png");

    sf::Texture deleteIconTexture;
    bool deleteIconLoaded = deleteIconTexture.loadFromFile("main/assets/images/UI/icons/delete.png");

    sf::Shader iconTintShader;
    bool iconTintShaderLoaded = sf::Shader::isAvailable() &&
        iconTintShader.loadFromMemory(kIconTintFragmentShader, sf::Shader::Type::Fragment);

    float howToScrollOffset = 0.f;

    // Per-project context menu / rename / delete confirmation state.
    int projectMenuIndex = -1;
    bool confirmingDelete = false;
    int pendingDeleteIndex = -1;
    bool renamingProject = false;
    int renamingIndex = -1;
    std::string renameBuffer;
    bool renameTypingStarted = false;

    sf::Clock cursorBlinkClock;
    float contextMenuAnchorX = 0.f;
    float contextMenuAnchorY = 0.f;
    std::string createNameError;
    std::string renameNameError;
    sf::Clock createErrorClock;
    sf::Clock renameErrorClock;

    const std::string howToUseText =
        "HOW TO USE\n"
        "\n"
        "-- Getting Started --\n"
        "Select a project from the list on the\n"
        "left, or create a new one. Each project\n"
        "is saved as its own map.json file.\n"
        "\n"
        "-- Controls --\n"
        "Left Click       Place / Select\n"
        "Shift + Click    Add/remove from selection\n"
        "Drag empty area  Box-select\n"
        "Right Click      Delete object\n"
        "Middle Mouse     Pan camera\n"
        "Mouse Wheel      Zoom in / out\n"
        "1 - 4            Choose object type\n"
        "G                Toggle grid snapping\n"
        "R                Rotate selection\n"
        "[  /  ]          Shrink / Grow selection\n"
        "Delete           Remove selection\n"
        "Ctrl + Z / Y     Undo / Redo\n"
        "Ctrl + C / V     Copy / Paste\n"
        "Ctrl + A         Select all\n"
        "Ctrl + S         Save map\n"
        "Ctrl + O         Load map\n"
        "Escape           Deselect\n"
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
        // Shadow the initial constants with the *current* window size, so every
        // layout calculation below (dividerX, card positions, list width, etc.)
        // automatically reflows to fill whatever size the window actually is.
        unsigned int WINDOW_WIDTH = window.getSize().x;
        unsigned int WINDOW_HEIGHT = window.getSize().y;
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* homeResized = event->getIf<sf::Event::Resized>()) {
                sf::Vector2f newSize(static_cast<float>(homeResized->size.x), static_cast<float>(homeResized->size.y));
                homeView.setSize(newSize);
                homeView.setCenter({newSize.x / 2.f, newSize.y / 2.f});
            }

            if (renamingProject) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    char32_t unicode = textEntered->unicode;
                    if (unicode == 8) {
                        if (!renameBuffer.empty()) renameBuffer.pop_back();
                    } else if (unicode >= 32 && unicode < 127) {
                        char c = static_cast<char>(unicode);
                        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                       (c >= '0' && c <= '9') || c == '_' || c == ' ';
                        if (!allowed) {
                            renameNameError = "[Error!] Only letters, numbers, _ and space allowed";
                            renameErrorClock.restart();
                        } else {
                            renameBuffer += c;
                        }
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        renamingProject = false;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (!renameBuffer.empty() &&
                            renamingIndex >= 0 && renamingIndex < static_cast<int>(projects.size())) {
                            std::filesystem::rename("main/assets/map/" + projects[renamingIndex].name,
                                                     "main/assets/map/" + renameBuffer);
                            projects = ScanProjects();
                            renamingProject = false;
                            projectMenuIndex = -1;
                        } else {
                            renameNameError = "[Error!] Name cannot be empty";
                            renameErrorClock.restart();
                        }
                    }
                }
            }

            if (const auto* wheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                projectMenuIndex = -1;
                sf::Vector2f wheelWorldPos = window.mapPixelToCoords(wheelScrolled->position, homeView);
                float dividerXNow = (WINDOW_WIDTH - 60.f) + (WINDOW_WIDTH * 0.62f - (WINDOW_WIDTH - 60.f)) * howToAnim;

                if (howToAnim > 0.5f && wheelWorldPos.x > dividerXNow) {
                    float howToViewHeight = static_cast<float>(WINDOW_HEIGHT) - 60.f;
                    float lineCount = 1.f;
                    for (char ch : howToUseText) if (ch == '\n') lineCount += 1.f;
                    float howToContentHeight = lineCount * 16.f * 1.2f;
                    float maxHowToScroll = std::max(0.f, howToContentHeight - howToViewHeight);
                    howToScrollOffset -= wheelScrolled->delta * 30.f;
                    if (howToScrollOffset < 0.f) howToScrollOffset = 0.f;
                    if (howToScrollOffset > maxHowToScroll) howToScrollOffset = maxHowToScroll;
                } else {
                    listScrollTarget -= wheelScrolled->delta * 48.f;
                    if (listScrollTarget < 0.f) listScrollTarget = 0.f;
                    float maxScroll = std::max(0.f, static_cast<float>(projects.size()) * 56.f - (WINDOW_HEIGHT - 110.f - 56.f));
                    if (listScrollTarget > maxScroll) listScrollTarget = maxScroll;
                }
            }

            if (enteringName) {
                if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2f mousePos = window.mapPixelToCoords(mousePressed->position, homeView);
                        float iconSize = 22.f;
                        sf::FloatRect toggleIconBounds({static_cast<float>(WINDOW_WIDTH) - iconSize - 20.f, 24.f}, {iconSize, iconSize});
                        if (toggleIconBounds.contains(mousePos)) {
                            showHowTo = !showHowTo;
                        }
                    }
                }
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    char32_t unicode = textEntered->unicode;
                    if (unicode == 8) {
                        if (!projectName.empty()) projectName.pop_back();
                        typingStarted = true;
                    }
                    else if (unicode >= 32 && unicode < 127) {
                        if (!typingStarted) {
                            projectName.clear();
                            typingStarted = true;
                        }
                        char c = static_cast<char>(unicode);
                        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                       (c >= '0' && c <= '9') || c == '_' || c == ' ';
                        if (!allowed) {
                            createNameError = "[Error!] Only letters, numbers, _ and space allowed";
                            createErrorClock.restart();
                        } else {
                            projectName += c;
                        }
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (!projectName.empty()) {
                            isNewProjectCreation = true;
                            creatingProject = false;
                        } else {
                            createNameError = "[Error!] Name cannot be empty";
                            createErrorClock.restart();
                        }
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        enteringName = false;
                        typingStarted = false;
                        projectName.clear();
                    }
                }
            }
            else {
                if (!renamingProject && !confirmingDelete) {
                    if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                        int itemCount = static_cast<int>(projects.size()) + 1;
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
                }

                if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mousePressed->button == sf::Mouse::Button::Right && !projects.empty() && !renamingProject && !confirmingDelete) {
                        sf::Vector2f rightClickPos = window.mapPixelToCoords(mousePressed->position, homeView);
                        float listY = 110.f;
                        float rowHeight = 56.f;
                        for (int i = 0; i < static_cast<int>(projects.size()); ++i) {
                            float y = listY + rowHeight + i * rowHeight - listScrollOffset;
                            sf::FloatRect rowBounds({40.f, y}, {static_cast<float>(WINDOW_WIDTH), rowHeight - 8.f});
                            if (rowBounds.contains(rightClickPos)) {
                                projectMenuIndex = i;
                                contextMenuAnchorX = rightClickPos.x;
                                contextMenuAnchorY = rightClickPos.y;
                                break;
                            }
                        }
                    }
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2f mousePos = window.mapPixelToCoords(mousePressed->position, homeView);
                        float dividerX = (WINDOW_WIDTH - 60.f) + (WINDOW_WIDTH * 0.62f - (WINDOW_WIDTH - 60.f)) * howToAnim;
                        float listX = 40.f;
                        float listY = 110.f;
                        float rowHeight = 56.f;
                        float iconBtnSize = 24.f;

                        if (confirmingDelete) {
                            sf::FloatRect yesBounds({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                            sf::FloatRect noBounds({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                            if (yesBounds.contains(mousePos)) {
                                if (pendingDeleteIndex >= 0 && pendingDeleteIndex < static_cast<int>(projects.size())) {
                                    std::filesystem::remove_all("main/assets/map/" + projects[pendingDeleteIndex].name);
                                    projects = ScanProjects();
                                }
                                confirmingDelete = false;
                                pendingDeleteIndex = -1;
                                projectMenuIndex = -1;
                            } else if (noBounds.contains(mousePos)) {
                                confirmingDelete = false;
                                pendingDeleteIndex = -1;
                            }
                        }
                        else if (renamingProject) {
                            sf::FloatRect okBounds({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                            sf::FloatRect cancelBounds({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                            if (okBounds.contains(mousePos)) {
                                bool validName = !renameBuffer.empty();
                                if (validName && renamingIndex >= 0 && renamingIndex < static_cast<int>(projects.size())) {
                                    std::filesystem::rename("main/assets/map/" + projects[renamingIndex].name,
                                                             "main/assets/map/" + renameBuffer);
                                    projects = ScanProjects();
                                    renamingProject = false;
                                    projectMenuIndex = -1;
                                }
                            } else if (cancelBounds.contains(mousePos)) {
                                renamingProject = false;
                            }
                        }
                        else if (projectMenuIndex != -1) {
                            float menuIconSize2 = 16.f;
                            float rowH2 = menuIconSize2 + 6.f * 2.f;
                            sf::Text measureRename(uiFont, "Rename", 18);
                            sf::Text measureDelete(uiFont, "Delete", 18);
                            float textW2 = std::max(measureRename.getLocalBounds().size.x, measureDelete.getLocalBounds().size.x);
                            float menuWidth2 = 10.f + menuIconSize2 + 8.f + textW2 + 10.f;
                            float menuX2 = contextMenuAnchorX;
                            float menuTop2 = contextMenuAnchorY;
                            float menuHeight2 = rowH2 * 2.f;
                            float menuMargin2 = 8.f;
                            if (menuX2 < menuMargin2) menuX2 = menuMargin2;
                            if (menuX2 + menuWidth2 > static_cast<float>(WINDOW_WIDTH) - menuMargin2) {
                                menuX2 = static_cast<float>(WINDOW_WIDTH) - menuWidth2 - menuMargin2;
                            }
                            if (menuTop2 + menuHeight2 > static_cast<float>(WINDOW_HEIGHT) - menuMargin2) {
                                menuTop2 = contextMenuAnchorY - menuHeight2 - 4.f;
                            }
                            if (menuTop2 < menuMargin2) menuTop2 = menuMargin2;
                            sf::FloatRect renameBounds({menuX2, menuTop2}, {menuWidth2, rowH2});
                            sf::FloatRect deleteBounds({menuX2, menuTop2 + rowH2}, {menuWidth2, rowH2});
                            if (renameBounds.contains(mousePos)) {
                                renamingProject = true;
                                renamingIndex = projectMenuIndex;
                                renameBuffer = projects[projectMenuIndex].name;
                                renameTypingStarted = false;
                            } else if (deleteBounds.contains(mousePos)) {
                                confirmingDelete = true;
                                pendingDeleteIndex = projectMenuIndex;
                            } else {
                                projectMenuIndex = -1;
                            }
                        }
                        else {
                            float iconSize = 22.f;
                            sf::FloatRect toggleIconBounds({static_cast<float>(WINDOW_WIDTH) - iconSize - 20.f, 24.f}, {iconSize, iconSize});
                            if (toggleIconBounds.contains(mousePos)) {
                                showHowTo = !showHowTo;
                            }
                            else if (projects.empty()) {
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
                                bool hitSomething = false;
                                for (int i = 0; i < static_cast<int>(projects.size()); ++i) {
                                    float y = listY + rowHeight + i * rowHeight - listScrollOffset;
                                    sf::FloatRect iconBounds(
                                        {listX + (dividerX - 60.f) - iconBtnSize - 16.f, y + (rowHeight - 8.f) / 2.f - iconBtnSize / 2.f},
                                        {iconBtnSize, iconBtnSize});
                                    if (iconBounds.contains(mousePos)) {
                                        projectMenuIndex = (projectMenuIndex == i) ? -1 : i;
                                        contextMenuAnchorX = mousePos.x;
                                        contextMenuAnchorY = mousePos.y;
                                        hitSomething = true;
                                        break;
                                    }
                                }

                                if (!hitSomething) {
                                    sf::FloatRect createRowBounds({listX, listY}, {dividerX - 60.f, rowHeight - 8.f});
                                    if (createRowBounds.contains(mousePos)) {
                                        selectedIndex = 0;
                                        bool isDoubleClick = (lastClickedIndex == 0 && doubleClickClock.getElapsedTime().asMilliseconds() < DOUBLE_CLICK_MS);
                                        doubleClickClock.restart();
                                        lastClickedIndex = 0;
                                        if (isDoubleClick) {
                                            enteringName = true;
                                            typingStarted = false;
                                            projectName = "New Project";
                                            lastClickedIndex = -1;
                                        }
                                        hitSomething = true;
                                    }
                                }

                                if (!hitSomething) {
                                    for (int i = 0; i < static_cast<int>(projects.size()); ++i) {
                                        int itemIndex = i + 1;
                                        float y = listY + rowHeight + i * rowHeight - listScrollOffset;
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
            }
        }
    

        float dt = homeAnimClock.restart().asSeconds();
        float howToTarget = showHowTo ? 1.f : 0.f;
        float animSpeed = 6.f; // higher = snappier slide
        howToAnim += (howToTarget - howToAnim) * std::min(1.f, dt * animSpeed);
        if (std::abs(howToAnim - howToTarget) < 0.002f) howToAnim = howToTarget;

        float scrollSpeed = 12.f; // higher = snappier scroll, lower = smoother/floatier
        listScrollOffset += (listScrollTarget - listScrollOffset) * std::min(1.f, dt * scrollSpeed);
        if (std::abs(listScrollOffset - listScrollTarget) < 0.05f) listScrollOffset = listScrollTarget;

        window.clear(sf::Color(20, 20, 24));
        window.setView(homeView);

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

                        float dividerX = (WINDOW_WIDTH - 60.f) + (WINDOW_WIDTH * 0.62f - (WINDOW_WIDTH - 60.f)) * howToAnim;

        if (uiFontLoaded) {
            if (enteringName) {
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

                bool cursorOn = std::fmod(cursorBlinkClock.getElapsedTime().asSeconds(), 1.f) < 0.5f;
                sf::Text nameText(uiFont, projectName + (cursorOn ? "_" : " "), 28);
                nameText.setFillColor(sf::Color::White);
                nameText.setPosition({dividerX / 2.f - 220.f, WINDOW_HEIGHT / 2.f - 10.f});
                window.draw(nameText);

                sf::Text hint(uiFont, "Press Enter to create, Esc to cancel", 16);
                hint.setFillColor(sf::Color(130, 130, 140));
                hint.setPosition({dividerX / 2.f - hint.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f + 65.f});
                window.draw(hint);

                if (!createNameError.empty() && createErrorClock.getElapsedTime().asSeconds() < 3.f) {
                    sf::Text errText(uiFont, createNameError, 15);
                    errText.setFillColor(sf::Color(230, 90, 90));
                    errText.setPosition({dividerX / 2.f - errText.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f + 90.f});
                    window.draw(errText);
                }
            }
            else if (projects.empty()) {
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
                float listX = 40.f;
                float listY = 110.f;
                float rowHeight = 56.f;

                // Scrollable project rows are drawn first, clipped to start
                // below the pinned "Create New +" row so they never cover it.
                for (int i = 0; i < static_cast<int>(projects.size()); ++i) {
                    float y = listY + rowHeight + i * rowHeight - listScrollOffset;
                    if (y + (rowHeight - 8.f) <= listY + rowHeight) continue; // fully hidden behind pinned row
                    if (y > WINDOW_HEIGHT) continue;

                    int itemIndex = i + 1;
                    sf::RectangleShape row({dividerX - 60.f, rowHeight - 8.f});
                    row.setPosition({listX, y});
                    bool isSelected = (selectedIndex == itemIndex);
                    row.setFillColor(isSelected ? sf::Color(50, 50, 90) : sf::Color(32, 32, 38));
                    if (isSelected) {
                        row.setOutlineThickness(2.f);
                        row.setOutlineColor(sf::Color(90, 90, 240));
                    }
                    window.draw(row);

                    const ProjectEntry& p = projects[i];
                    sf::Text nameText(uiFont, p.name, 22);
                    nameText.setFillColor(sf::Color::White);
                    nameText.setPosition({listX + 16.f, y + 6.f});
                    window.draw(nameText);

                    sf::Text sizeText(uiFont, FormatSize(p.sizeBytes), 14);
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

                // Pinned "Create New +" row, drawn last so it always sits on top and never scrolls.
                sf::RectangleShape createRow({dividerX - 60.f, rowHeight - 8.f});
                createRow.setPosition({listX, listY});
                bool createSelected = (selectedIndex == 0);
                createRow.setFillColor(createSelected ? sf::Color(50, 50, 90) : sf::Color(32, 32, 38));
                if (createSelected) {
                    createRow.setOutlineThickness(2.f);
                    createRow.setOutlineColor(sf::Color(90, 90, 240));
                }
                window.draw(createRow);

                sf::Text plus(uiFont, "Create New +", 22);
                plus.setFillColor(sf::Color::White);
                plus.setPosition({listX + 16.f, listY + 12.f});
                window.draw(plus);
            }

            sf::RectangleShape divider({2.f, static_cast<float>(WINDOW_HEIGHT)});
            divider.setPosition({dividerX, 0.f});
            divider.setFillColor(sf::Color(60, 60, 66));
            window.draw(divider);

            if (!projects.empty() && !enteringName) {
                float listY = 110.f;
                float rowHeight = 56.f;
                float viewHeight = WINDOW_HEIGHT - listY - rowHeight;
                float contentHeight = static_cast<float>(projects.size()) * rowHeight;
                if (contentHeight > viewHeight) {
                    float trackX = dividerX - 14.f;
                    sf::RectangleShape track({4.f, viewHeight});
                    track.setPosition({trackX, listY + rowHeight});
                    track.setFillColor(sf::Color(45, 45, 50));
                    window.draw(track);

                    float thumbHeight = std::max(24.f, viewHeight * (viewHeight / contentHeight));
                    float maxScroll = contentHeight - viewHeight;
                    float thumbY = listY + rowHeight + (listScrollOffset / maxScroll) * (viewHeight - thumbHeight);
                    sf::RectangleShape thumb({4.f, thumbHeight});
                    thumb.setPosition({trackX, thumbY});
                    thumb.setFillColor(sf::Color(120, 120, 230));
                    window.draw(thumb);
                }
            }

            if (howToAnim > 0.01f) {
                sf::Text howTo(uiFont, howToUseText, 16);
                howTo.setFillColor(sf::Color(210, 210, 215));
                howTo.setPosition({dividerX + 30.f, 30.f - howToScrollOffset});
                howTo.setLineSpacing(1.2f);
                window.draw(howTo);

                float howToViewHeight = static_cast<float>(WINDOW_HEIGHT) - 60.f;
                float howToLineCount = 1.f;
                for (char ch : howToUseText) if (ch == '\n') howToLineCount += 1.f;
                float howToContentHeight = howToLineCount * 16.f * 1.2f;
                if (howToContentHeight > howToViewHeight) {
                    float trackX2 = static_cast<float>(WINDOW_WIDTH) - 10.f;
                    sf::RectangleShape track2({4.f, howToViewHeight});
                    track2.setPosition({trackX2, 30.f});
                    track2.setFillColor(sf::Color(45, 45, 50));
                    window.draw(track2);

                    float thumbHeight2 = std::max(24.f, howToViewHeight * (howToViewHeight / howToContentHeight));
                    float maxHowToScroll2 = howToContentHeight - howToViewHeight;
                    float thumbY2 = 30.f + (howToScrollOffset / maxHowToScroll2) * (howToViewHeight - thumbHeight2);
                    sf::RectangleShape thumb2({4.f, thumbHeight2});
                    thumb2.setPosition({trackX2, thumbY2});
                    thumb2.setFillColor(sf::Color(120, 120, 230));
                    window.draw(thumb2);
                }

                if (closeIconLoaded) {
                    sf::Sprite closeSprite(closeIconTexture);
                    sf::Vector2u texSize = closeIconTexture.getSize();
                    float iconSize2 = 22.f;
                    float scale = (texSize.x > 0) ? (iconSize2 / static_cast<float>(texSize.x)) : 1.f;
                    closeSprite.setScale({scale, scale});
                    closeSprite.setPosition({static_cast<float>(WINDOW_WIDTH) - iconSize2 - 20.f, 24.f});
                    window.draw(closeSprite);
                }
            } else if (arrowIconLoaded) {
                sf::Sprite reopenSprite(arrowIconTexture);
                sf::Vector2u texSize = arrowIconTexture.getSize();
                float iconSize2 = 22.f;
                float scale = (texSize.x > 0) ? (iconSize2 / static_cast<float>(texSize.x)) : 1.f;
                reopenSprite.setScale({scale, scale});
                reopenSprite.setPosition({static_cast<float>(WINDOW_WIDTH) - iconSize2 - 20.f, 24.f});
                window.draw(reopenSprite);
            }
        }

        if (uiFontLoaded && (projectMenuIndex != -1 || confirmingDelete || renamingProject)) {
            float dividerXNow = (WINDOW_WIDTH - 60.f) + (WINDOW_WIDTH * 0.62f - (WINDOW_WIDTH - 60.f)) * howToAnim;
            float listY = 110.f;
            float rowHeight = 56.f;

            if (projectMenuIndex != -1 && !confirmingDelete && !renamingProject) {
                float menuIconSize = 16.f;
                float iconTextGap = 8.f;
                float sidePad = 10.f;
                float rowPadTop = 6.f;
                float rowH = menuIconSize + rowPadTop * 2.f;

                sf::Text renameLabel(uiFont, "Rename", 18);
                renameLabel.setFillColor(sf::Color::White);
                sf::Text deleteLabel(uiFont, "Delete", 18);
                deleteLabel.setFillColor(sf::Color(230, 90, 90));

                float textW = std::max(renameLabel.getLocalBounds().size.x, deleteLabel.getLocalBounds().size.x);
                float menuWidth = sidePad + menuIconSize + iconTextGap + textW + sidePad;
                float menuHeight = rowH * 2.f;

                float menuX = contextMenuAnchorX;
                float menuTop = contextMenuAnchorY;

                float menuMargin = 8.f;
                if (menuX < menuMargin) menuX = menuMargin;
                if (menuX + menuWidth > static_cast<float>(WINDOW_WIDTH) - menuMargin) {
                    menuX = static_cast<float>(WINDOW_WIDTH) - menuWidth - menuMargin;
                }
                if (menuTop + menuHeight > static_cast<float>(WINDOW_HEIGHT) - menuMargin) {
                    menuTop = contextMenuAnchorY - menuHeight - 4.f; // flip upward if it would overflow the bottom
                }
                if (menuTop < menuMargin) menuTop = menuMargin;

                sf::RectangleShape menuBg({menuWidth, menuHeight});
                menuBg.setPosition({menuX, menuTop});
                menuBg.setFillColor(sf::Color(40, 40, 46));
                menuBg.setOutlineThickness(1.f);
                menuBg.setOutlineColor(sf::Color(90, 90, 240));
                window.draw(menuBg);

                if (editIconLoaded) {
                    sf::Sprite editSprite(editIconTexture);
                    sf::Vector2u texSize = editIconTexture.getSize();
                    float scale = (texSize.x > 0) ? (menuIconSize / static_cast<float>(texSize.x)) : 1.f;
                    editSprite.setScale({scale, scale});
                    editSprite.setPosition({menuX + sidePad, menuTop + rowPadTop});
                    if (iconTintShaderLoaded) {
                        iconTintShader.setUniform("source", editIconTexture);
                        iconTintShader.setUniform("tintColor", sf::Glsl::Vec4(1.f, 1.f, 1.f, 1.f));
                        window.draw(editSprite, &iconTintShader);
                    } else {
                        window.draw(editSprite);
                    }
                }
                renameLabel.setPosition({menuX + sidePad + menuIconSize + iconTextGap, menuTop + rowPadTop - 3.f});
                window.draw(renameLabel);

                sf::RectangleShape separator({menuWidth, 1.f});
                separator.setPosition({menuX, menuTop + rowH});
                separator.setFillColor(sf::Color(70, 70, 78));
                window.draw(separator);

                if (deleteIconLoaded) {
                    sf::Sprite deleteSprite(deleteIconTexture);
                    sf::Vector2u texSize = deleteIconTexture.getSize();
                    float scale = (texSize.x > 0) ? (menuIconSize / static_cast<float>(texSize.x)) : 1.f;
                    deleteSprite.setScale({scale, scale});
                    deleteSprite.setPosition({menuX + sidePad, menuTop + rowH + rowPadTop});
                    if (iconTintShaderLoaded) {
                        iconTintShader.setUniform("source", deleteIconTexture);
                        iconTintShader.setUniform("tintColor", sf::Glsl::Vec4(230.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f));
                        window.draw(deleteSprite, &iconTintShader);
                    } else {
                        window.draw(deleteSprite);
                    }
                }
                deleteLabel.setPosition({menuX + sidePad + menuIconSize + iconTextGap, menuTop + rowH + rowPadTop - 3.f});
                window.draw(deleteLabel);
            }

            if (confirmingDelete) {
                sf::RectangleShape overlay({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
                overlay.setFillColor(sf::Color(0, 0, 0, 150));
                window.draw(overlay);

                sf::RectangleShape dialog({340.f, 130.f});
                dialog.setPosition({WINDOW_WIDTH / 2.f - 170.f, WINDOW_HEIGHT / 2.f - 60.f});
                dialog.setFillColor(sf::Color(38, 38, 44));
                dialog.setOutlineThickness(2.f);
                dialog.setOutlineColor(sf::Color(230, 90, 90));
                window.draw(dialog);

                sf::Text question(uiFont, "Are you sure?", 22);
                question.setPosition({WINDOW_WIDTH / 2.f - question.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f - 40.f});
                question.setFillColor(sf::Color::White);
                window.draw(question);

                sf::RectangleShape yesBtn({100.f, 36.f});
                yesBtn.setPosition({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f});
                yesBtn.setFillColor(sf::Color(230, 90, 90));
                window.draw(yesBtn);
                sf::Text yesLabel(uiFont, "Delete", 18);
                yesLabel.setFillColor(sf::Color::White);
                yesLabel.setPosition({WINDOW_WIDTH / 2.f - 110.f + 20.f, WINDOW_HEIGHT / 2.f + 26.f});
                window.draw(yesLabel);

                sf::RectangleShape noBtn({100.f, 36.f});
                noBtn.setPosition({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f});
                noBtn.setFillColor(sf::Color(70, 70, 78));
                window.draw(noBtn);
                sf::Text noLabel(uiFont, "Cancel", 18);
                noLabel.setFillColor(sf::Color::White);
                noLabel.setPosition({WINDOW_WIDTH / 2.f + 10.f + 20.f, WINDOW_HEIGHT / 2.f + 26.f});
                window.draw(noLabel);
            }

            if (renamingProject) {
                sf::RectangleShape overlay({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
                overlay.setFillColor(sf::Color(0, 0, 0, 150));
                window.draw(overlay);

                sf::RectangleShape dialog({380.f, 140.f});
                dialog.setPosition({WINDOW_WIDTH / 2.f - 190.f, WINDOW_HEIGHT / 2.f - 65.f});
                dialog.setFillColor(sf::Color(38, 38, 44));
                dialog.setOutlineThickness(2.f);
                dialog.setOutlineColor(sf::Color(90, 90, 240));
                window.draw(dialog);

                sf::Text label(uiFont, "Rename Project", 16);
                label.setFillColor(sf::Color(150, 150, 160));
                label.setPosition({WINDOW_WIDTH / 2.f - 170.f, WINDOW_HEIGHT / 2.f - 50.f});
                window.draw(label);

                bool renameCursorOn = std::fmod(cursorBlinkClock.getElapsedTime().asSeconds(), 1.f) < 0.5f;
                sf::Text nameText(uiFont, renameBuffer + (renameCursorOn ? "_" : " "), 24);
                nameText.setFillColor(sf::Color::White);
                nameText.setPosition({WINDOW_WIDTH / 2.f - 170.f, WINDOW_HEIGHT / 2.f - 20.f});
                window.draw(nameText);

                if (!renameNameError.empty() && renameErrorClock.getElapsedTime().asSeconds() < 3.f) {
                    sf::Text errText(uiFont, renameNameError, 15);
                    errText.setFillColor(sf::Color(230, 90, 90));
                    errText.setPosition({WINDOW_WIDTH / 2.f - errText.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f + 60.f});
                    window.draw(errText);
                }

                sf::RectangleShape okBtn({100.f, 36.f});
                okBtn.setPosition({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f});
                okBtn.setFillColor(sf::Color(90, 160, 90));
                window.draw(okBtn);
                sf::Text okLabel(uiFont, "Save", 18);
                okLabel.setFillColor(sf::Color::White);
                okLabel.setPosition({WINDOW_WIDTH / 2.f - 110.f + 30.f, WINDOW_HEIGHT / 2.f + 26.f});
                window.draw(okLabel);

                sf::RectangleShape cancelBtn({100.f, 36.f});
                cancelBtn.setPosition({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f});
                cancelBtn.setFillColor(sf::Color(70, 70, 78));
                window.draw(cancelBtn);
                sf::Text cancelLabel(uiFont, "Cancel", 18);
                cancelLabel.setFillColor(sf::Color::White);
                cancelLabel.setPosition({WINDOW_WIDTH / 2.f + 10.f + 20.f, WINDOW_HEIGHT / 2.f + 26.f});
                window.draw(cancelLabel);
            }
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

    return 0;
}
