#include <iostream>
#include <cmath>
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

    sf::Texture infoBgTexture;
    if(infoBgTexture.loadFromFile("main/assets/images/UI/icons/button_icon_circle.png")){
        LOG_FULL("Info background texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load info background texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    infoBgTexture.setSmooth(false);

    sf::Texture infoIconTexture;
    if(infoIconTexture.loadFromFile("main/assets/images/UI/icons/icon_info_white.png")){
        LOG_FULL("Info icon texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load info icon texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    infoIconTexture.setSmooth(false);

    sf::Texture htpIconTexture;
    if(htpIconTexture.loadFromFile("main/assets/images/UI/icons/icon_question_white.png")){
        LOG_FULL("How To Play icon texture loaded successfully.", DebugSeverity::Info, "Assets", __LINE__);
    }else{
        LOG_FULL("Failed to load How To Play icon texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    htpIconTexture.setSmooth(false);

    sf::Texture closeIconTexture;
    if(closeIconTexture.loadFromFile("main/assets/images/UI/icons/icon_close_white.png")){
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

    // Button Texture Setup
    sf::Texture buttonBoxTex, menuExitTex;
    if (!buttonBoxTex.loadFromFile("main/assets/images/UI/gui_sprites/bar_horizontal_dotted_dark.png")) {
        LOG_FULL("Failed to load button box texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    if (!menuExitTex.loadFromFile("main/assets/images/UI/gui_sprites/bar_horizontal_plain_dark.png")) {
        LOG_FULL("Failed to load main menu exit button texture.", DebugSeverity::Error, "Assets", __LINE__);
    }

    const float menuButtonWidth = 220.f;
    const float menuButtonHeight = 60.f;
    const float menuButtonSpacing = 20.f;

    sf::Vector2u btnTexSize = buttonBoxTex.getSize();
    sf::Vector2u exitBtnTexSize = menuExitTex.getSize();

    // Vertically center 3 main buttons (Start, Settings, Exit)
    float totalMenuHeight = (menuButtonHeight * 3.f) + (menuButtonSpacing * 2.f);
    float startY = SCREEN_HEIGHT / 2.f - totalMenuHeight / 2.f + menuButtonHeight / 2.f;

    sf::Sprite startButton(buttonBoxTex);
    startButton.setOrigin({btnTexSize.x / 2.f, btnTexSize.y / 2.f});
    startButton.setScale({menuButtonWidth / btnTexSize.x, menuButtonHeight / btnTexSize.y});
    startButton.setPosition({SCREEN_WIDTH / 2.f, startY});

    const float infoIconSize = 60.f;

    sf::Sprite infoButtonBg(infoBgTexture);
    sf::Vector2u infoBgTexSize = infoBgTexture.getSize();
    infoButtonBg.setOrigin({infoBgTexSize.x / 2.f, infoBgTexSize.y / 2.f});
    infoButtonBg.setScale({
        infoIconSize / infoBgTexSize.x,
        infoIconSize / infoBgTexSize.y
    });
    infoButtonBg.setPosition({40.f + infoIconSize / 2.f, 40.f + infoIconSize / 2.f});

    sf::Sprite infoButton(infoIconTexture);
    sf::Vector2u infoIconTexSize = infoIconTexture.getSize();
    infoButton.setOrigin({infoIconTexSize.x / 2.f, infoIconTexSize.y / 2.f});
    infoButton.setScale({
        infoIconSize / infoIconTexSize.x,
        infoIconSize / infoIconTexSize.y
    });
    infoButton.setPosition(infoButtonBg.getPosition());

    sf::Sprite settingsButton(buttonBoxTex);
    settingsButton.setOrigin({btnTexSize.x / 2.f, btnTexSize.y / 2.f});
    settingsButton.setScale({menuButtonWidth / btnTexSize.x, menuButtonHeight / btnTexSize.y});
    settingsButton.setPosition({SCREEN_WIDTH / 2.f, startY + menuButtonHeight + menuButtonSpacing});

    sf::Sprite mainMenuExitButton(menuExitTex);
    mainMenuExitButton.setOrigin({exitBtnTexSize.x / 2.f, exitBtnTexSize.y / 2.f});
    mainMenuExitButton.setScale({menuButtonWidth / exitBtnTexSize.x, menuButtonHeight / exitBtnTexSize.y});
    mainMenuExitButton.setPosition({SCREEN_WIDTH / 2.f, startY + (menuButtonHeight + menuButtonSpacing) * 2.f});

    // New added code for exit program
    std::cout << "[DEBUG] Attempting to load fonts..." << std::endl;
    sf::Font font;
    bool fontLoaded = font.openFromFile("main/assets/fonts/VT323-Regular.ttf");
    std::cout << "[DEBUG] Font loaded = " << fontLoaded << std::endl;
    if(!fontLoaded){
        LOG_FULL("Failed to load font for exit prompt.", DebugSeverity::Error, "Assets", __LINE__);
    }

    sf::Font titleFont;
    bool titleFontLoaded = titleFont.openFromFile("main/assets/fonts/Honk-Regular-VariableFont_MORF,SHLN.ttf");
    if(!titleFontLoaded){
        LOG_FULL("Failed to load title font.", DebugSeverity::Error, "Assets", __LINE__);
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

    sf::Text mainMenuExitText(font, "Exit", 28);
    mainMenuExitText.setFillColor(sf::Color::White);
    sf::FloatRect mainMenuExitBounds = mainMenuExitText.getLocalBounds();
    mainMenuExitText.setOrigin({mainMenuExitBounds.size.x / 2.f, mainMenuExitBounds.size.y / 2.f + mainMenuExitBounds.position.y});
    mainMenuExitText.setPosition(mainMenuExitButton.getPosition());

    // Game Title ("Zero Theory") - Uses Honk Font
    sf::Text gameTitleText(titleFontLoaded ? titleFont : font, "Zero Theory", 64);
    gameTitleText.setFillColor(sf::Color::White);
    sf::FloatRect gameTitleBounds = gameTitleText.getLocalBounds();
    gameTitleText.setOrigin({gameTitleBounds.size.x / 2.f, gameTitleBounds.size.y / 2.f + gameTitleBounds.position.y});
    gameTitleText.setPosition({SCREEN_WIDTH / 2.f, 100.f});

    // Game Subtitle ("A word finding game.") - Uses Regular Font
    sf::Text gameSubtitleText(font, "A Undesigned Game.", 24);
    gameSubtitleText.setFillColor(sf::Color(200, 200, 200, 255));
    sf::FloatRect gameSubtitleBounds = gameSubtitleText.getLocalBounds();
    gameSubtitleText.setOrigin({gameSubtitleBounds.size.x / 2.f, gameSubtitleBounds.size.y / 2.f + gameSubtitleBounds.position.y});
    gameSubtitleText.setPosition({SCREEN_WIDTH / 2.f, 150.f});

    // Global menu state variable (Add near top variables if not present)
    bool g_showHowToPlayMenu = false;

    // Settings menu panel
    const float panelWidth = 500.f;
    const float panelHeight = 420.f;
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

    sf::Sprite closeSettingsBg(infoBgTexture);
    closeSettingsBg.setOrigin({infoBgTexSize.x / 2.f, infoBgTexSize.y / 2.f});
    closeSettingsBg.setScale({
        closeIconSize / infoBgTexSize.x,
        closeIconSize / infoBgTexSize.y
    });
    closeSettingsBg.setPosition({
        SCREEN_WIDTH / 2.f + panelWidth / 2.f - closeIconSize / 2.f - 15.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + closeIconSize / 2.f + 15.f
    });

    sf::Sprite closeSettingsButton(closeIconTexture);
    sf::Vector2u closeIconTexSize = closeIconTexture.getSize();
    closeSettingsButton.setOrigin({closeIconTexSize.x / 2.f, closeIconTexSize.y / 2.f});
    closeSettingsButton.setScale({
        closeIconSize / closeIconTexSize.x,
        closeIconSize / closeIconTexSize.y
    });
    closeSettingsButton.setPosition(closeSettingsBg.getPosition());

    // Load Settings Button Textures
    sf::Texture infoBtnTex, exitBtnTex;
    if(!infoBtnTex.loadFromFile("main/assets/images/UI/icons/icon_info_white.png")){
        LOG_FULL("Failed to load info button texture.", DebugSeverity::Error, "Assets", __LINE__);
    }
    if(!exitBtnTex.loadFromFile("main/assets/images/UI/gui_sprites/bar_horizontal_plain_dark.png")){
        LOG_FULL("Failed to load exit button texture.", DebugSeverity::Error, "Assets", __LINE__);
    }

    const float iconBtnSize = 50.f;

    // 1. Info / About Icon Button (Top-Left of settings panel)
    sf::Sprite settingsAboutButton(infoBtnTex);
    sf::Vector2u abtTexSize = infoBtnTex.getSize();
    settingsAboutButton.setOrigin({abtTexSize.x / 2.f, abtTexSize.y / 2.f});
    settingsAboutButton.setScale({iconBtnSize / abtTexSize.x, iconBtnSize / abtTexSize.y});
    settingsAboutButton.setPosition({
        SCREEN_WIDTH / 2.f - panelWidth / 2.f + iconBtnSize / 2.f + 20.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + iconBtnSize / 2.f + 20.f
    });

    // 2. How To Play Button Group (Center of the screen)
    sf::Text howToPlayLabel(font, "How to Play", 28);
    howToPlayLabel.setFillColor(sf::Color::White);
    sf::FloatRect htpLabelBounds = howToPlayLabel.getLocalBounds();

    float htpImgSize = 45.f;

    sf::Sprite howToPlayBg(infoBgTexture);
    howToPlayBg.setOrigin({infoBgTexSize.x / 2.f, infoBgTexSize.y / 2.f});
    howToPlayBg.setScale({htpImgSize / infoBgTexSize.x, htpImgSize / infoBgTexSize.y});

    sf::Sprite howToPlayButton(htpIconTexture);
    sf::Vector2u htpTexSize = htpIconTexture.getSize();
    howToPlayButton.setOrigin({htpTexSize.x / 2.f, htpTexSize.y / 2.f});
    howToPlayButton.setScale({htpImgSize / htpTexSize.x, htpImgSize / htpTexSize.y});

    // Calculate total width of "How to Play [Image]" to center them as a unit
    float htpSpacing = 15.f;
    float htpTotalWidth = htpLabelBounds.size.x + htpSpacing + htpImgSize;
    float htpStartX = SCREEN_WIDTH / 2.f - htpTotalWidth / 2.f;

    howToPlayLabel.setOrigin({0.f, htpLabelBounds.size.y / 2.f + htpLabelBounds.position.y});
    howToPlayLabel.setPosition({htpStartX, SCREEN_HEIGHT / 2.f});

    sf::Vector2f htpPos = {
        htpStartX + htpLabelBounds.size.x + htpSpacing + htpImgSize / 2.f,
        SCREEN_HEIGHT / 2.f
    };
    howToPlayBg.setPosition(htpPos);
    howToPlayButton.setPosition(htpPos);

    // 3. Exit Button (Bottom Center of settings panel)
    sf::Sprite settingsExitButton(exitBtnTex);
    sf::Vector2u extTexSize = exitBtnTex.getSize();
    const float exitWidth = 160.f;
    const float exitHeight = 50.f;
    settingsExitButton.setOrigin({extTexSize.x / 2.f, extTexSize.y / 2.f});
    settingsExitButton.setScale({exitWidth / extTexSize.x, exitHeight / extTexSize.y});
    settingsExitButton.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT / 2.f + panelHeight / 2.f - exitHeight / 2.f - 25.f
    });

    sf::Text settingsExitText(font, "Exit", 28);
    settingsExitText.setFillColor(sf::Color::White);
    sf::FloatRect settingsExitBounds = settingsExitText.getLocalBounds();
    settingsExitText.setOrigin({settingsExitBounds.size.x / 2.f, settingsExitBounds.size.y / 2.f + settingsExitBounds.position.y});
    settingsExitText.setPosition(settingsExitButton.getPosition());

    // How To Play Panel Setup
    sf::RectangleShape htpPanel({panelWidth, panelHeight});
    htpPanel.setOrigin({panelWidth / 2.f, panelHeight / 2.f});
    htpPanel.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});
    htpPanel.setFillColor(sf::Color(25, 25, 25, 240));
    htpPanel.setOutlineThickness(2.f);
    htpPanel.setOutlineColor(sf::Color::White);

    sf::Text htpTitle(font, "How to Play", 36);
    htpTitle.setFillColor(sf::Color::White);
    sf::FloatRect htpTitleBounds = htpTitle.getLocalBounds();
    htpTitle.setOrigin({htpTitleBounds.size.x / 2.f, htpTitleBounds.size.y / 2.f + htpTitleBounds.position.y});
    htpTitle.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f - panelHeight / 2.f + 40.f});

    sf::Text htpBodyText(font, "Work in progres!", 22);
    htpBodyText.setFillColor(sf::Color::White);
    sf::FloatRect htpBodyBounds = htpBodyText.getLocalBounds();
    htpBodyText.setOrigin({htpBodyBounds.size.x / 2.f, htpBodyBounds.size.y / 2.f + htpBodyBounds.position.y});
    htpBodyText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});

    sf::Sprite closeHTPBg(infoBgTexture);
    closeHTPBg.setOrigin({infoBgTexSize.x / 2.f, infoBgTexSize.y / 2.f});
    closeHTPBg.setScale({closeIconSize / infoBgTexSize.x, closeIconSize / infoBgTexSize.y});
    closeHTPBg.setPosition({
        SCREEN_WIDTH / 2.f + panelWidth / 2.f - closeIconSize / 2.f - 15.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + closeIconSize / 2.f + 15.f
    });

    sf::Sprite closeHTPButton(closeIconTexture);
    closeHTPButton.setOrigin({closeIconTexSize.x / 2.f, closeIconTexSize.y / 2.f});
    closeHTPButton.setScale({closeIconSize / closeIconTexSize.x, closeIconSize / closeIconTexSize.y});
    closeHTPButton.setPosition(closeHTPBg.getPosition());

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

    sf::Text aboutBodyText(font, "ZeroTheory\nA Undesigned Game.\nVersion 0.1.0", 24);
    aboutBodyText.setFillColor(sf::Color::White);
    sf::FloatRect aboutBodyBounds = aboutBodyText.getLocalBounds();
    aboutBodyText.setOrigin({aboutBodyBounds.size.x / 2.f, aboutBodyBounds.size.y / 2.f + aboutBodyBounds.position.y});
    aboutBodyText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f});

    sf::Sprite closeAboutBg(infoBgTexture);
    closeAboutBg.setOrigin({infoBgTexSize.x / 2.f, infoBgTexSize.y / 2.f});
    closeAboutBg.setScale({closeIconSize / infoBgTexSize.x, closeIconSize / infoBgTexSize.y});
    closeAboutBg.setPosition({
        SCREEN_WIDTH / 2.f + panelWidth / 2.f - closeIconSize / 2.f - 15.f,
        SCREEN_HEIGHT / 2.f - panelHeight / 2.f + closeIconSize / 2.f + 15.f
    });

    sf::Sprite closeAboutButton(closeIconTexture);
    closeAboutButton.setOrigin({closeIconTexSize.x / 2.f, closeIconTexSize.y / 2.f});
    closeAboutButton.setScale({closeIconSize / closeIconTexSize.x, closeIconSize / closeIconTexSize.y});
    closeAboutButton.setPosition(closeAboutBg.getPosition());

    // Composite Modular Loading Bar Textures
    sf::Texture barLeftTex, barMidBgTex, barFillTex, barRightTex;
    if (!barLeftTex.loadFromFile("main/assets/images/UI/Loading_bar/loading_bar_left_end.png")) {
        LOG_FULL("Failed to load barLeftTex.", DebugSeverity::Error, "Assets", __LINE__);
    }
    if (!barMidBgTex.loadFromFile("main/assets/images/UI/Loading_bar/loading_bar_middle_empty.png")) {
        LOG_FULL("Failed to load barMidBgTex.", DebugSeverity::Error, "Assets", __LINE__);
    }
    if (!barFillTex.loadFromFile("main/assets/images/UI/Loading_bar/loading_bar_middle_fill.png")) {
        LOG_FULL("Failed to load barFillTex.", DebugSeverity::Error, "Assets", __LINE__);
    }
    if (!barRightTex.loadFromFile("main/assets/images/UI/Loading_bar/loading_bar_right_end.png")) {
        LOG_FULL("Failed to load barRightTex.", DebugSeverity::Error, "Assets", __LINE__);
    }

    barMidBgTex.setRepeated(true);
    barFillTex.setRepeated(true);

    const float totalBarWidth = 500.f;
    const float barHeight = 40.f;
    const float endCapWidth = 20.f; 
    const float midWidth = totalBarWidth - (endCapWidth * 2.f);

    float startX = (SCREEN_WIDTH / 2.f) - (totalBarWidth / 2.f);
    float centerY = SCREEN_HEIGHT - 100.f;

    // 1. Left End Cap Sprite
    sf::Sprite barLeftSprite(barLeftTex);
    sf::Vector2u leftSize = barLeftTex.getSize();
    barLeftSprite.setOrigin({0.f, leftSize.y / 2.f});
    barLeftSprite.setScale({endCapWidth / leftSize.x, barHeight / leftSize.y});
    barLeftSprite.setPosition({startX, centerY});

    // 2. Middle Empty Background Sprite
    sf::Sprite barMidBgSprite(barMidBgTex);
    sf::Vector2u midBgSize = barMidBgTex.getSize();
    barMidBgSprite.setOrigin({0.f, midBgSize.y / 2.f});
    barMidBgSprite.setScale({1.f, barHeight / midBgSize.y});
    barMidBgSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(midWidth), static_cast<int>(midBgSize.y)}));
    barMidBgSprite.setPosition({startX + endCapWidth, centerY});

    // 3. Middle Fill Sprite
    sf::Sprite barFillSprite(barFillTex);
    sf::Vector2u fillTexSize = barFillTex.getSize();
    barFillSprite.setOrigin({0.f, fillTexSize.y / 2.f});
    barFillSprite.setScale({1.f, barHeight / fillTexSize.y});
    barFillSprite.setPosition({startX + endCapWidth, centerY});

    // 4. Right End Cap Sprite
    sf::Sprite barRightSprite(barRightTex);
    sf::Vector2u rightSize = barRightTex.getSize();
    barRightSprite.setOrigin({0.f, rightSize.y / 2.f});
    barRightSprite.setScale({endCapWidth / rightSize.x, barHeight / rightSize.y});
    barRightSprite.setPosition({startX + endCapWidth + midWidth, centerY});

    sf::Text loadingPercentText(font, "0%", 28);
    loadingPercentText.setFillColor(sf::Color::White);
    loadingPercentText.setPosition({
        SCREEN_WIDTH / 2.f - totalBarWidth / 2.f,
        SCREEN_HEIGHT - 100.f - barHeight / 2.f - 45.f
    });

    sf::Text loadingLabelText(font, "Loading...", 28);
    loadingLabelText.setFillColor(sf::Color::White);
    sf::FloatRect loadingLabelBounds = loadingLabelText.getLocalBounds();
    loadingLabelText.setOrigin({loadingLabelBounds.size.x / 2.f, loadingLabelBounds.size.y / 2.f + loadingLabelBounds.position.y});
    loadingLabelText.setPosition({
        SCREEN_WIDTH / 2.f,
        SCREEN_HEIGHT - 100.f - barHeight / 2.f - 45.f
    });

    // "Game not ready" message
    sf::Text notReadyText(font, "Game is not ready yet.\nWe are working on it.", 34);
    notReadyText.setFillColor(sf::Color::White);
    sf::FloatRect notReadyBounds = notReadyText.getLocalBounds();
    notReadyText.setOrigin({notReadyBounds.size.x / 2.f, notReadyBounds.size.y / 2.f + notReadyBounds.position.y});
    notReadyText.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f - 40.f});

    sf::Texture okBtnTex;
    if(!okBtnTex.loadFromFile("main/assets/images/UI/gui_sprites/bar_horizontal_scallop_dark.png")){
        LOG_FULL("Failed to load OK button texture.", DebugSeverity::Error, "Assets", __LINE__);
    }

    sf::Vector2u okTexSize = okBtnTex.getSize();
    const float okWidth = 160.f;
    const float okHeight = 55.f;

    sf::Sprite notReadyOkButton(okBtnTex);
    notReadyOkButton.setOrigin({okTexSize.x / 2.f, okTexSize.y / 2.f});
    notReadyOkButton.setScale({okWidth / okTexSize.x, okHeight / okTexSize.y});
    notReadyOkButton.setPosition({SCREEN_WIDTH / 2.f, SCREEN_HEIGHT - 100.f});

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

    sf::Texture confirmBtnTex;
    if(!confirmBtnTex.loadFromFile("main/assets/images/UI/gui_sprites/bar_horizontal_scallop_dark.png")){
        LOG_FULL("Failed to load exit confirm button texture.", DebugSeverity::Error, "Assets", __LINE__);
    }

    const float buttonWidth = 160.f;
    const float buttonHeight = 55.f;
    const float buttonMargin = 40.f;
    const float buttonSpacing = 20.f;

    sf::Vector2u confirmTexSize = confirmBtnTex.getSize();

    sf::Sprite cancelButton(confirmBtnTex);
    cancelButton.setOrigin({confirmTexSize.x / 2.f, confirmTexSize.y / 2.f});
    cancelButton.setScale({buttonWidth / confirmTexSize.x, buttonHeight / confirmTexSize.y});
    cancelButton.setPosition({
        SCREEN_WIDTH - buttonMargin - buttonWidth / 2.f,
        SCREEN_HEIGHT - buttonMargin - buttonHeight / 2.f
    });

    sf::Sprite yesButton(confirmBtnTex);
    yesButton.setOrigin({confirmTexSize.x / 2.f, confirmTexSize.y / 2.f});
    yesButton.setScale({buttonWidth / confirmTexSize.x, buttonHeight / confirmTexSize.y});
    yesButton.setPosition({
        SCREEN_WIDTH - buttonMargin - buttonWidth - buttonSpacing - buttonWidth / 2.f,
        SCREEN_HEIGHT - buttonMargin - buttonHeight / 2.f
    });

    sf::Text cancelText(font, "Cancel", 26);
    cancelText.setFillColor(sf::Color::White);
    sf::FloatRect cancelBounds = cancelText.getLocalBounds();
    cancelText.setOrigin({cancelBounds.size.x / 2.f, cancelBounds.size.y / 2.f + cancelBounds.position.y});
    cancelText.setPosition(cancelButton.getPosition());

    sf::Text yesText(font, "Yes", 26);
    yesText.setFillColor(sf::Color::White);
    sf::FloatRect yesBounds = yesText.getLocalBounds();
    yesText.setOrigin({yesBounds.size.x / 2.f, yesBounds.size.y / 2.f + yesBounds.position.y});
    yesText.setPosition(yesButton.getPosition());

    std::cout << "[DEBUG] Entering main loop. isOpen = " << window.isOpen() << std::endl;
    while (window.isOpen()) {
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()){
                LOG_DIRECT("Closing Game Window...",DebugSeverity::Info);
                window.close();
            }

            if(const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()){
                if(g_showHowToPlayMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showHowToPlayMenu = false;
                }
                else if(g_showSettingsMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showSettingsMenu = false;
                }
                else if(g_showAboutMenu && keyPressed->code == sf::Keyboard::Key::Escape){
                    g_showAboutMenu = false;
                }
                else if(!g_showExitConfirm && !g_showSettingsMenu && !g_showAboutMenu && !g_showHowToPlayMenu && keyPressed->code == sf::Keyboard::Key::Escape){
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
                if(g_showHowToPlayMenu){
                    bool isCloseHtpHovered = closeHTPBg.getGlobalBounds().contains(menuMousePos);
                    sf::Color hoverColor = isCloseHtpHovered ? sf::Color(180, 180, 180, 255) : sf::Color::White;
                    closeHTPBg.setColor(hoverColor);
                    closeHTPButton.setColor(hoverColor);
                }
                else if(g_showSettingsMenu){
                    bool isCloseSettingsHovered = closeSettingsBg.getGlobalBounds().contains(menuMousePos);
                    sf::Color closeColor = isCloseSettingsHovered ? sf::Color(180, 180, 180, 255) : sf::Color::White;
                    closeSettingsBg.setColor(closeColor);
                    closeSettingsButton.setColor(closeColor);
                    
                    bool isHtpHovered = howToPlayBg.getGlobalBounds().contains(menuMousePos) || howToPlayLabel.getGlobalBounds().contains(menuMousePos);
                    sf::Color htpColor = isHtpHovered ? sf::Color(180, 180, 180, 255) : sf::Color::White;
                    howToPlayBg.setColor(htpColor);
                    howToPlayButton.setColor(htpColor);
                    howToPlayLabel.setFillColor(htpColor);

                    settingsAboutButton.setColor(settingsAboutButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                    settingsExitButton.setColor(settingsExitButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                } else if(g_showAboutMenu){
                    bool isCloseAboutHovered = closeAboutBg.getGlobalBounds().contains(menuMousePos);
                    sf::Color hoverColor = isCloseAboutHovered ? sf::Color(180, 180, 180, 255) : sf::Color::White;
                    closeAboutBg.setColor(hoverColor);
                    closeAboutButton.setColor(hoverColor);
                } else {
                    startButton.setColor(startButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                    settingsButton.setColor(settingsButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                    mainMenuExitButton.setColor(mainMenuExitButton.getGlobalBounds().contains(menuMousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                    bool isInfoHovered = infoButtonBg.getGlobalBounds().contains(menuMousePos);
                    sf::Color hoverColor = isInfoHovered ? sf::Color(180, 180, 180, 255) : sf::Color::White;
                    infoButtonBg.setColor(hoverColor);
                    infoButton.setColor(hoverColor);
                }
            }

            if(const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()){
                if(mousePressed->button == sf::Mouse::Button::Left){
                    sf::Vector2f menuClickPos(static_cast<float>(mousePressed->position.x), static_cast<float>(mousePressed->position.y));
                    if(g_showHowToPlayMenu){
                        if(closeHTPButton.getGlobalBounds().contains(menuClickPos) || !htpPanel.getGlobalBounds().contains(menuClickPos)){
                            g_showHowToPlayMenu = false;
                        }
                    }
                    else if(g_showSettingsMenu){
                        if(howToPlayBg.getGlobalBounds().contains(menuClickPos) || howToPlayLabel.getGlobalBounds().contains(menuClickPos)){
                            g_showHowToPlayMenu = true;
                        }
                        else if(settingsAboutButton.getGlobalBounds().contains(menuClickPos)){
                            g_showAboutMenu = true;
                        }
                        else if(settingsExitButton.getGlobalBounds().contains(menuClickPos)){
                            g_showExitConfirm = true;
                        }
                        else if(closeSettingsButton.getGlobalBounds().contains(menuClickPos) || !settingsPanel.getGlobalBounds().contains(menuClickPos)){
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
                    else if(!g_showExitConfirm && mainMenuExitButton.getGlobalBounds().contains(menuClickPos)){
                        g_showExitConfirm = true;
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

            if(g_showNotReady){
                if(const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()){
                    sf::Vector2f mousePos(static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y));
                    notReadyOkButton.setColor(notReadyOkButton.getGlobalBounds().contains(mousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                }
            }

            if(g_showExitConfirm){
                if(const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()){
                    sf::Vector2f mousePos(static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y));
                    yesButton.setColor(yesButton.getGlobalBounds().contains(mousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
                    cancelButton.setColor(cancelButton.getGlobalBounds().contains(mousePos)
                        ? sf::Color(180, 180, 180, 255) : sf::Color::White);
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

            // Stage 1 (0.0s - 0.8s): Smooth 0% -> 50%
            if(elapsed <= 0.8f){
                g_loadingProgress = (elapsed / 0.8f) * 50.f;
            }
            // Stage 2 (0.8s - 1.2s): Pause at 50%
            else if(elapsed <= 1.2f){
                g_loadingProgress = 50.f;
            }
            // Stage 3 (1.2s - 1.4s): Quick jump 50% -> 90%
            else if(elapsed <= 1.4f){
                float t = (elapsed - 1.2f) / 0.2f;
                g_loadingProgress = 50.f + (t * 40.f);
            }
            // Stage 4 (1.4s - 2.3s): Slowly climb 90% -> 99% one by one
            else if(elapsed <= 2.3f){
                float stepProgress = (elapsed - 1.4f) / 0.9f;
                g_loadingProgress = 90.f + std::floor(stepProgress * 9.f);
            }
            // Stage 5 (2.3s - 2.8s): Hold/pause at 99%
            else if(elapsed <= 2.8f){
                g_loadingProgress = 99.f;
            }
            // Stage 6 (2.8s+): Snap to 100% and finish
            else {
                g_loadingProgress = 100.f;
                g_showLoading = false;
                g_showNotReady = true;
            }

            int currentWidth = static_cast<int>(midWidth * (g_loadingProgress / 100.f));
            barFillSprite.setTextureRect(sf::IntRect({0, 0}, {currentWidth, static_cast<int>(fillTexSize.y)}));
            loadingPercentText.setString(std::to_string(static_cast<int>(g_loadingProgress)) + "%");

            // Animate Loading text dots based on elapsed time
            int dotCount = static_cast<int>(elapsed * 4.0f) % 4; // Cycles 0 to 3 dots every second
            std::string dots = std::string(dotCount, '.');
            loadingLabelText.setString("Loading" + dots);

            // Re-center text smoothly as dot length changes
            sf::FloatRect newLabelBounds = loadingLabelText.getLocalBounds();
            loadingLabelText.setOrigin({newLabelBounds.size.x / 2.f, newLabelBounds.size.y / 2.f + newLabelBounds.position.y});
            loadingLabelText.setPosition({
                SCREEN_WIDTH / 2.f,
                SCREEN_HEIGHT - 100.f - barHeight / 2.f - 45.f
            });
        }

        window.clear(sf::Color(20, 20, 20));
        window.draw(bgSprite);

        if(fontLoaded){
            // Always draw Title and Subtitle so they don't disappear in sub-menus
            window.draw(gameTitleText);
            window.draw(gameSubtitleText);

            if(g_showHowToPlayMenu){
                window.draw(htpPanel);
                window.draw(htpTitle);
                window.draw(htpBodyText);
                window.draw(closeHTPBg);
                window.draw(closeHTPButton);
            }
            else if(g_showAboutMenu){
                window.draw(aboutPanel);
                window.draw(aboutTitle);
                window.draw(aboutBodyText);
                window.draw(closeAboutBg);
                window.draw(closeAboutButton);
            }
            else if(g_showSettingsMenu){
                window.draw(settingsPanel);
                window.draw(settingsTitle);
                window.draw(closeSettingsBg);
                window.draw(closeSettingsButton);
                
                window.draw(howToPlayLabel);
                window.draw(howToPlayBg);
                window.draw(howToPlayButton);
                window.draw(settingsAboutButton);
                window.draw(settingsExitButton);
                window.draw(settingsExitText);
            }
            else {
                window.draw(startButton);
                window.draw(settingsButton);
                window.draw(mainMenuExitButton);
                window.draw(startText);
                window.draw(settingsText);
                window.draw(mainMenuExitText);
                window.draw(infoButtonBg);
                window.draw(infoButton);
            }
        }

        if(g_showLoading && fontLoaded){
            sf::RectangleShape loadingOverlay({static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)});
            loadingOverlay.setFillColor(sf::Color(0, 0, 0, 180));
            window.draw(loadingOverlay);
            window.draw(loadingLabelText);
            
            // Draw all composite loading bar layers
            window.draw(barMidBgSprite);
            window.draw(barFillSprite);
            window.draw(barLeftSprite);
            window.draw(barRightSprite);
            
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
