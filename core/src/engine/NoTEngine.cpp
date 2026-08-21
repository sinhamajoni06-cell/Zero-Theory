#include <iostream>
#include "../../lib/header/NotDebugger.hpp"
#include <SFML/Graphics.hpp>
//new added "bool g_showExitConfirm = false;"
bool g_showExitConfirm = false;
bool g_showSettingsMenu = false;
bool g_showAboutMenu = false;
bool g_showLoading = false;
bool g_showNotReady = false;
float g_loadingProgress = 0.f;
sf::Clock g_loadingClock;

using namespace NovaEngine::Debugger;

int main() {

#ifdef _NOTDEBUG

    NovaEngine::Debugger::DebugLogger::Log(
        NovaEngine::Debugger::DebuggerEntry{
            "DBG_0001", 
            "TEST_TAG", 
            "TestCategory", 
            "This is a test debug message.", 
            "42", 
            "int", 
            nullptr, 
            __FILE__, 
            __func__, 
            __LINE__, 
            NovaEngine::Debugger::DebugSeverity::Info, 
            5, 
            0, 
            {"main() -> Log()"}
        }
    );
#endif
    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    const unsigned int SCREEN_WIDTH = desktopMode.size.x;
    const unsigned int SCREEN_HEIGHT = desktopMode.size.y;

    std::cout << "[DEBUG] Desktop resolution: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;

    sf::RenderWindow window(desktopMode, "Nova Text Game", sf::Style::None, sf::State::Windowed);
    window.setFramerateLimit(60);

    std::cout << "[DEBUG] Window created. isOpen = " << window.isOpen() << std::endl;

    sf::Texture infoIconTexture;
    if(infoIconTexture.loadFromFile("main/assets/images/UI/Buttons/info.png")){
        LOG_FULL("Info icon texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load info icon texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    infoIconTexture.setSmooth(false);

    sf::Texture closeIconTexture;
    if(closeIconTexture.loadFromFile("main/assets/images/UI/Buttons/close.png")){
        LOG_FULL("Close icon texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load close icon texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    closeIconTexture.setSmooth(false);

    sf::Texture bgTexture;
    if(bgTexture.loadFromFile("main/assets/images/test_bg.jpg")){
        LOG_FULL("Background texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load background texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    sf::Sprite bgSprite(bgTexture);
    sf::Vector2u texSize = bgTexture.getSize();
    bgSprite.setScale({
        static_cast<float>(SCREEN_WIDTH) / texSize.x,
        static_cast<float>(SCREEN_HEIGHT) / texSize.y
    });

    // Main menu buttons (Start / Settings), centered
    const float menuButtonWidth = 220.f;
    const float menuButtonHeight = 60.f;
    const float menuButtonSpacing = 30.f;

    sf::RectangleShape startButton({menuButtonWidth, menuButtonHeight});
    startButton.setOrigin({menuButtonWidth / 2.f, menuButtonHeight / 2.f});
    startButton.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT / 2.f - (menuButtonHeight + menuButtonSpacing) / 2.f
    });
    startButton.setFillColor(sf::Color(60, 60, 60, 220));
    startButton.setOutlineThickness(2.f);
    startButton.setOutlineColor(sf::Color::White);

    const float infoIconSize = 60.f;
    sf::Sprite infoButton(infoIconTexture);
    sf::Vector2u infoIconTexSize = infoIconTexture.getSize();
    infoButton.setOrigin({infoIconTexSize.x / 2.f, infoIconTexSize.y / 2.f});
    infoButton.setScale({
        infoIconSize / infoIconTexSize.x,
        infoIconSize / infoIconTexSize.y
    });
    infoButton.setPosition({40.f + infoIconSize / 2.f, 40.f + infoIconSize / 2.f});

    sf::RectangleShape settingsButton({menuButtonWidth, menuButtonHeight});
    settingsButton.setOrigin({menuButtonWidth / 2.f, menuButtonHeight / 2.f});
    settingsButton.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT / 2.f + (menuButtonHeight + menuButtonSpacing) / 2.f
    });
    settingsButton.setFillColor(sf::Color(60, 60, 60, 220));
    settingsButton.setOutlineThickness(2.f);
    settingsButton.setOutlineColor(sf::Color::White);

    //New added code for exit program
    std::cout << "[DEBUG] Attempting to load font..." << std::endl;
    sf::Font font;
    bool fontLoaded = font.openFromFile("main/assets/fonts/VT323-Regular.ttf");
    std::cout << "[DEBUG] Font loaded = " << fontLoaded << std::endl;
    if(!fontLoaded){
        LOG_FULL("Failed to load font for exit prompt.", DebugSeverity::Error, "Assets", __LINE__);
    }

    sf::Text startText(font, "Start", 28);
    startText.setFillColor(sf::Color::White);
    sf::FloatRect startBounds = startText.getLocalBounds();
    startText.setOrigin({startBounds.size.x / 2.f, startBounds.size.y / 2.f + startBounds.position.y});
    startText.setPosition(startButton.getPosition());

    sf::Text settingsText(font, "Settings", 28);
    settingsText.setFillColor(sf::Color::White);
    sf::FloatRect settingsBounds = settingsText.getLocalBounds();
    settingsText.setOrigin({settingsBounds.size.x / 2.f, settingsBounds.size.y / 2.f + settingsBounds.position.y});
    settingsText.setPosition(settingsButton.getPosition());

    // Settings menu panel
    const float panelWidth = 500.f;
    const float panelHeight = 350.f;
    sf::RectangleShape settingsPanel({panelWidth, panelHeight});
    settingsPanel.setOrigin({panelWidth / 2.f, panelHeight / 2.f});
    settingsPanel.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});
    settingsPanel.setFillColor(sf::Color(25, 25, 25, 240));
    settingsPanel.setOutlineThickness(2.f);
    settingsPanel.setOutlineColor(sf::Color::White);

    sf::Text settingsTitle(font, "Settings", 36);
    settingsTitle.setFillColor(sf::Color::White);
    sf::FloatRect settingsTitleBounds = settingsTitle.getLocalBounds();
    settingsTitle.setOrigin({settingsTitleBounds.size.x / 2.f, settingsTitleBounds.size.y / 2.f + settingsTitleBounds.position.y});
    settingsTitle.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + 40.f
    });

    const float closeIconSize = 40.f;
    sf::Sprite closeSettingsButton(closeIconTexture);
    sf::Vector2u closeIconTexSize = closeIconTexture.getSize();
    closeSettingsButton.setOrigin({closeIconTexSize.x / 2.f, closeIconTexSize.y / 2.f});
    closeSettingsButton.setScale({
        closeIconSize / closeIconTexSize.x,
        closeIconSize / closeIconTexSize.y
    });
    closeSettingsButton.setPosition({
        SCREEN_WIDTH / 2.f + panelWidth / 2.f - closeIconSize / 2.f - 15.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + closeIconSize / 2.f + 15.f
    });

    // About menu panel
    sf::RectangleShape aboutPanel({panelWidth, panelHeight});
    aboutPanel.setOrigin({panelWidth / 2.f, panelHeight / 2.f});
    aboutPanel.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});
    aboutPanel.setFillColor(sf::Color(25, 25, 25, 240));
    aboutPanel.setOutlineThickness(2.f);
    aboutPanel.setOutlineColor(sf::Color::White);

    sf::Text aboutTitle(font, "About", 36);
    aboutTitle.setFillColor(sf::Color::White);
    sf::FloatRect aboutTitleBounds = aboutTitle.getLocalBounds();
    aboutTitle.setOrigin({aboutTitleBounds.size.x / 2.f, aboutTitleBounds.size.y / 2.f + aboutTitleBounds.position.y});
    aboutTitle.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + 40.f
    });

    sf::Text aboutBodyText(font, "Nova Text Game\nA psychological horror experience.\nVersion 0.1", 24);
    aboutBodyText.setFillColor(sf::Color::White);
    sf::FloatRect aboutBodyBounds = aboutBodyText.getLocalBounds();
    aboutBodyText.setOrigin({aboutBodyBounds.size.x / 2.f, aboutBodyBounds.size.y / 2.f + aboutBodyBounds.position.y});
    aboutBodyText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});

    sf::Sprite closeAboutButton(closeIconTexture);
    closeAboutButton.setOrigin({closeIconTexSize.x / 2.f, closeIconTexSize.y / 2.f});
    closeAboutButton.setScale({
        closeIconSize / closeIconTexSize.x,
        closeIconSize / closeIconTexSize.y
    });
    closeAboutButton.setPosition({
        SCREEN_WIDTH / 2.f + panelWidth / 2.f - closeIconSize / 2.f - 15.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + closeIconSize / 2.f + 15.f
    });

    // Loading bar UI
    const float loadingBarWidth = 500.f;
    const float loadingBarHeight = 40.f;

    sf::RectangleShape loadingBarBackground({loadingBarWidth, loadingBarHeight});
    loadingBarBackground.setOrigin({loadingBarWidth / 2.f, loadingBarHeight / 2.f});
    loadingBarBackground.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT - 100.f});
    loadingBarBackground.setFillColor(sf::Color(40, 40, 40, 230));
    loadingBarBackground.setOutlineThickness(2.f);
    loadingBarBackground.setOutlineColor(sf::Color::White);

    sf::RectangleShape loadingBarFill({0.f, loadingBarHeight - 6.f});
    loadingBarFill.setPosition({
        SCREEN_WIDTH / 2.f - loadingBarWidth / 2.f + 3.f,
        SCREEN_HEIGHT - 100.f - loadingBarHeight / 2.f + 3.f
    });
    loadingBarFill.setFillColor(sf::Color(180, 40, 40, 255));

    sf::Text loadingPercentText(font, "0%", 28);
    loadingPercentText.setFillColor(sf::Color::White);
    loadingPercentText.setPosition({
        SCREEN_WIDTH / 2.f - loadingBarWidth / 2.f,
        SCREEN_HEIGHT - 100.f - loadingBarHeight / 2.f - 45.f
    });

    sf::Text loadingLabelText(font, "Loading...", 28);
    loadingLabelText.setFillColor(sf::Color::White);
    sf::FloatRect loadingLabelBounds = loadingLabelText.getLocalBounds();
    loadingLabelText.setOrigin({loadingLabelBounds.size.x / 2.f, loadingLabelBounds.size.y / 2.f + loadingLabelBounds.position.y});
    loadingLabelText.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT - 100.f - loadingBarHeight / 2.f - 45.f
    });

    // "Game not ready" message
    sf::Text notReadyText(font, "Game is not ready yet.\nWe are working on it.", 34);
    notReadyText.setFillColor(sf::Color::White);
    sf::FloatRect notReadyBounds = notReadyText.getLocalBounds();
    notReadyText.setOrigin({notReadyBounds.size.x / 2.f, notReadyBounds.size.y / 2.f + notReadyBounds.position.y});
    notReadyText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f - 40.f});

    sf::RectangleShape notReadyOkButton({160.f, 55.f});
    notReadyOkButton.setOrigin({80.f, 27.5f});
    notReadyOkButton.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT - 100.f});
    notReadyOkButton.setFillColor(sf::Color(60, 60, 60, 220));
    notReadyOkButton.setOutlineThickness(2.f);
    notReadyOkButton.setOutlineColor(sf::Color::White);

    sf::Text notReadyOkText(font, "OK", 26);
    notReadyOkText.setFillColor(sf::Color::White);
    sf::FloatRect notReadyOkBounds = notReadyOkText.getLocalBounds();
    notReadyOkText.setOrigin({notReadyOkBounds.size.x / 2.f, notReadyOkBounds.size.y / 2.f + notReadyOkBounds.position.y});
    notReadyOkText.setPosition(notReadyOkButton.getPosition());

    sf::Text exitText(font, "Do you want to Exit?", 40);
    exitText.setFillColor(sf::Color::White);
    sf::FloatRect textBounds = exitText.getLocalBounds();
    exitText.setOrigin({textBounds.size.x / 2.f, textBounds.size.y / 2.f});
    exitText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});

    // GTA5-style Yes/Cancel buttons, bottom-right corner
    const float buttonWidth = 160.f;
    const float buttonHeight = 55.f;
    const float buttonMargin = 40.f;
    const float buttonSpacing = 20.f;

    sf::RectangleShape cancelButton({buttonWidth, buttonHeight});
    cancelButton.setPosition({
        SCREEN_WIDTH - buttonMargin - buttonWidth,
        SCREEN_HEIGHT - buttonMargin - buttonHeight
    });
    cancelButton.setFillColor(sf::Color(60, 60, 60, 220));
    cancelButton.setOutlineThickness(2.f);
    cancelButton.setOutlineColor(sf::Color::White);

    sf::RectangleShape yesButton({buttonWidth, buttonHeight});
    yesButton.setPosition({
        SCREEN_WIDTH - buttonMargin - buttonWidth - buttonSpacing - buttonWidth,
        SCREEN_HEIGHT - buttonMargin - buttonHeight
    });
    yesButton.setFillColor(sf::Color(60, 60, 60, 220));
    yesButton.setOutlineThickness(2.f);
    yesButton.setOutlineColor(sf::Color::White);

    sf::Text cancelText(font, "Cancel", 26);
    cancelText.setFillColor(sf::Color::White);
    sf::FloatRect cancelBounds = cancelText.getLocalBounds();
    cancelText.setOrigin({cancelBounds.size.x / 2.f, cancelBounds.size.y / 2.f + cancelBounds.position.y});
    cancelText.setPosition({
        cancelButton.getPosition().x + buttonWidth / 2.f,
        cancelButton.getPosition().y + buttonHeight / 2.f
    });

    sf::Text yesText(font, "Yes", 26);
    yesText.setFillColor(sf::Color::White);
    sf::FloatRect yesBounds = yesText.getLocalBounds();
    yesText.setOrigin({yesBounds.size.x / 2.f, yesBounds.size.y / 2.f + yesBounds.position.y});
    yesText.setPosition({
        yesButton.getPosition().x + buttonWidth / 2.f,
        yesButton.getPosition().y + buttonHeight / 2.f
    });

    std::cout << "[DEBUG] Entering main loop. isOpen = " << window.isOpen() << std::endl;
    while (window.isOpen()) {
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()){
                LOG_DIRECT("Closing Game Window...",DebugSeverity::Info);
                window.close();
            }

            if(const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()){
                if(g_showSettingsMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showSettingsMenu = false;
                }
                else if(g_showAboutMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showAboutMenu = false;
                }
                else if(!g_showExitConfirm && !g_showSettingsMenu && !g_showAboutMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showExitConfirm = true;
                }
                else if(g_showExitConfirm && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showExitConfirm = false;
                }
                else if(g_showExitConfirm && keyPressed->code == sf::Keyboard::Key::Enter){
                    LOG_DIRECT("Exit confirmed. Closing Game Window...", DebugSeverity::Info);
                    window.close();
                }
            }

            if(const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()){
                sf::Vector2f menuMousePos(static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y));
                if(g_showSettingsMenu){
                    closeSettingsButton.setColor(closeSettingsButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                } else if(g_showAboutMenu){
                    closeAboutButton.setColor(closeAboutButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                } else {
                    startButton.setFillColor(startButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(80, 80, 80, 230) : sf::Color(60, 60, 60, 220));
                    settingsButton.setFillColor(settingsButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(80, 80, 80, 230) : sf::Color(60, 60, 60, 220));
                    infoButton.setColor(infoButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                }
            }

            if(const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()){
                if(mousePressed->button == sf::Mouse::Button::Left){
                    sf::Vector2f menuClickPos(static_cast<float>(mousePressed->position.x), static_cast<float>(mousePressed->position.y));
                    if(g_showSettingsMenu){
                        if(closeSettingsButton.getGlobalBounds().contains(menuClickPos)
                           || !settingsPanel.getGlobalBounds().contains(menuClickPos)){
                            g_showSettingsMenu = false;
                        }
                    }
                    else if(g_showAboutMenu){
                        if(closeAboutButton.getGlobalBounds().contains(menuClickPos)
                           || !aboutPanel.getGlobalBounds().contains(menuClickPos)){
                            g_showAboutMenu = false;
                        }
                    }
                    else if(!g_showExitConfirm && startButton.getGlobalBounds().contains(menuClickPos)){
                        LOG_DIRECT("Start button clicked.", DebugSeverity::Info);
                        g_showLoading = true;
                        g_loadingProgress = 0.f;
                        g_loadingClock.restart();
                    }
                    else if(!g_showExitConfirm && settingsButton.getGlobalBounds().contains(menuClickPos)){
                        LOG_DIRECT("Settings button clicked.", DebugSeverity::Info);
                        g_showSettingsMenu = true;
                    }
                    else if(!g_showExitConfirm && infoButton.getGlobalBounds().contains(menuClickPos)){
                        LOG_DIRECT("Info button clicked.", DebugSeverity::Info);
                        g_showAboutMenu = true;
                    }
                    else if(g_showNotReady && notReadyOkButton.getGlobalBounds().contains(menuClickPos)){
                        g_showNotReady = false;
                    }
                }
            }

            if(g_showExitConfirm){
                if(const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()){
                    sf::Vector2f mousePos(static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y));
                    yesButton.setFillColor(yesButton.getGlobalBounds().contains(mousePos)
                        ? sf::Color(200, 40, 40, 230) : sf::Color(60, 60, 60, 220));
                    cancelButton.setFillColor(cancelButton.getGlobalBounds().contains(mousePos)
                        ? sf::Color(80, 80, 80, 230) : sf::Color(60, 60, 60, 220));
                }

                if(const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()){
                    if(mousePressed->button == sf::Mouse::Button::Left){
                        sf::Vector2f mousePos(static_cast<float>(mousePressed->position.x), static_cast<float>(mousePressed->position.y));
                        if(yesButton.getGlobalBounds().contains(mousePos)){
                            LOG_DIRECT("Exit confirmed. Closing Game Window...", DebugSeverity::Info);
                            window.close();
                        }
                        else if(cancelButton.getGlobalBounds().contains(mousePos)){
                            g_showExitConfirm = false;
                        }
                    }
                }
            }
        }

        if(g_showLoading){
            float elapsed = g_loadingClock.getElapsedTime().asSeconds();
            g_loadingProgress = (elapsed / 2.0f) * 100.f; // fills over 2 seconds
            if(g_loadingProgress >= 100.f){
                g_loadingProgress = 100.f;
                g_showLoading = false;
                g_showNotReady = true;
            }
            loadingBarFill.setSize({
                (loadingBarWidth - 6.f) * (g_loadingProgress / 100.f),
                loadingBarHeight - 6.f
            });
            loadingPercentText.setString(std::to_string(static_cast<int>(g_loadingProgress)) + "%");
        }

        window.clear(sf::Color(20, 20, 20));
        window.draw(bgSprite);

        if(fontLoaded){
            if(g_showSettingsMenu){
                window.draw(settingsPanel);
                window.draw(settingsTitle);
                window.draw(closeSettingsButton);
            }
            else if(g_showAboutMenu){
                window.draw(aboutPanel);
                window.draw(aboutTitle);
                window.draw(aboutBodyText);
                window.draw(closeAboutButton);
            }
            else {
                window.draw(startButton);
                window.draw(settingsButton);
                window.draw(startText);
                window.draw(settingsText);
                window.draw(infoButton);
            }
        }

        if(g_showLoading && fontLoaded){
            sf::RectangleShape loadingOverlay({static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)});
            loadingOverlay.setFillColor(sf::Color(0, 0, 0, 180));
            window.draw(loadingOverlay);
            window.draw(loadingLabelText);
            window.draw(loadingBarBackground);
            window.draw(loadingBarFill);
            window.draw(loadingPercentText);
        }

        if(g_showNotReady && fontLoaded){
            sf::RectangleShape notReadyOverlay({static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)});
            notReadyOverlay.setFillColor(sf::Color(0, 0, 0, 180));
            window.draw(notReadyOverlay);
            window.draw(notReadyText);
            window.draw(notReadyOkButton);
            window.draw(notReadyOkText);
        }

        if(g_showExitConfirm && fontLoaded){
            sf::RectangleShape overlay({static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)});
            overlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(overlay);
            window.draw(exitText);
            window.draw(yesButton);
            window.draw(cancelButton);
            window.draw(yesText);
            window.draw(cancelText);
        }

        window.display();
    }
    
    
    return 0;
}