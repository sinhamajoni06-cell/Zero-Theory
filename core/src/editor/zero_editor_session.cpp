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
#include "TileSet.h"
#include "TileLayer.h"

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
constexpr float kCanvasMarginBottom = 108.f; // leaves room for the bottom tile palette
constexpr float kResizeIconX = kCanvasMarginSides + 90.f;
constexpr float kResizeIconY = 14.f;
constexpr float kResizeIconSize = 20.f;

sf::FloatRect ComputeResizeIconBounds() {
    return sf::FloatRect({kResizeIconX, kResizeIconY}, {kResizeIconSize, kResizeIconSize});
}

// Draws faint grid lines across whatever part of the tile layer is currently
// visible in `camera`, plus a bright outline around the map's actual bounds
// (0,0 to width*tileSize, height*tileSize) so the player can see where the
// playable area ends vs. empty space beyond it.
void DrawTileGrid(sf::RenderWindow& window, const sf::View& camera, const TileLayer& tileLayer) {
    float tileSize = tileLayer.tileSize();
    if (tileSize <= 0.f) return;

    float mapW = tileLayer.widthTiles() * tileSize;
    float mapH = tileLayer.heightTiles() * tileSize;

    sf::FloatRect view(camera.getCenter() - camera.getSize() / 2.f, camera.getSize());
    float left = std::max(0.f, view.position.x);
    float top = std::max(0.f, view.position.y);
    float right = std::min(mapW, view.position.x + view.size.x);
    float bottom = std::min(mapH, view.position.y + view.size.y);
    if (right <= left || bottom <= top) return;

    sf::VertexArray lines(sf::PrimitiveType::Lines);
    sf::Color gridColor(255, 255, 255, 22);

    int firstCol = static_cast<int>(std::floor(left / tileSize));
    int lastCol  = static_cast<int>(std::ceil(right / tileSize));
    for (int c = firstCol; c <= lastCol; ++c) {
        float x = c * tileSize;
        lines.append(sf::Vertex{{x, top}, gridColor});
        lines.append(sf::Vertex{{x, bottom}, gridColor});
    }

    int firstRow = static_cast<int>(std::floor(top / tileSize));
    int lastRow  = static_cast<int>(std::ceil(bottom / tileSize));
    for (int r = firstRow; r <= lastRow; ++r) {
        float y = r * tileSize;
        lines.append(sf::Vertex{{left, y}, gridColor});
        lines.append(sf::Vertex{{right, y}, gridColor});
    }
    window.draw(lines);

    sf::RectangleShape bounds({mapW, mapH});
    bounds.setPosition({0.f, 0.f});
    bounds.setFillColor(sf::Color::Transparent);
    bounds.setOutlineThickness(2.f);
    bounds.setOutlineColor(sf::Color(120, 170, 255, 200));
    window.draw(bounds);
}

struct MapSizePreset { const char* label; int w; int h; };
const MapSizePreset kResizePresets[] = {
    {"Small  (80 x 50)",   80,  50},
    {"Medium (200 x 120)", 200, 120},
    {"Large  (300 x 180)", 300, 180},
    {"Huge   (400 x 240)", 400, 240},
};
constexpr int kResizePresetCount = 4;
constexpr float kPaletteHeight = 84.f;
constexpr float kPaletteItemSize = 48.f;
constexpr float kPalettePad = 8.f;

enum class InspectorField { None, X, Y, W, H, Rotation };
enum class ExitPrompt { None, ConfirmHome, ConfirmSaveHome, ConfirmSaveQuit };
enum class EditorMode { Objects, TilePaint };
enum class Tool { Pointer, Pencil, Eraser, Fill, Move };
enum class ImportField { None, TileW, TileH };

struct TileImportState {
    bool open = false;
    std::vector<std::string> images;
    int selectedImage = -1;
    int tileW = 32;
    int tileH = 32;
    ImportField editingField = ImportField::None;
    std::string editBuffer;
    sf::Texture previewTexture;
    bool previewLoaded = false;
};

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

// Screen-space rect of the bottom tile-piece palette strip, directly under
// the canvas and sharing its left/right extent.
sf::FloatRect ComputePaletteRect(unsigned int WINDOW_WIDTH, unsigned int WINDOW_HEIGHT) {
    sf::FloatRect canvas = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
    float top = canvas.position.y + canvas.size.y + 8.f;
    return sf::FloatRect({canvas.position.x, top}, {canvas.size.x, kPaletteHeight});
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

void FloodFillTile(TileLayer& layer, int startX, int startY, int newTile) {
    int target = layer.get(startX, startY);
    if (target == newTile) return;
    std::vector<sf::Vector2i> stack{{startX, startY}};
    while (!stack.empty()) {
        sf::Vector2i c = stack.back(); stack.pop_back();
        if (c.x < 0 || c.y < 0 || c.x >= layer.widthTiles() || c.y >= layer.heightTiles()) continue;
        if (layer.get(c.x, c.y) != target) continue;
        layer.set(c.x, c.y, newTile);
        stack.push_back({c.x + 1, c.y});
        stack.push_back({c.x - 1, c.y});
        stack.push_back({c.x, c.y + 1});
        stack.push_back({c.x, c.y - 1});
    }
}

struct ToolbarButton {
    Tool tool;
    sf::FloatRect bounds;
    std::string label;
};

std::vector<ToolbarButton> ComputeToolbarLayout() {
    std::vector<ToolbarButton> buttons;
    struct Def { Tool t; std::string label; };
    std::vector<Def> defs = {
        {Tool::Pointer, "Mo"},
        {Tool::Pencil, "P"},
        {Tool::Eraser, "E"},
        {Tool::Fill,   "F"},
        {Tool::Move,   "M"},
    };
    float buttonSize = 32.f;
    float pad = 6.f;
    float x = 200.f; // Placed horizontally in top bar after project title
    float y = 10.f;  // Aligned in top header
    for (auto& d : defs) {
        buttons.push_back({d.t, sf::FloatRect({x, y}, {buttonSize, buttonSize}), d.label});
        x += buttonSize + pad;
    }
    return buttons;
}

void DrawToolbar(sf::RenderWindow& window, sf::Font& uiFont, Tool currentTool,
                 const sf::Texture& texPencil, const sf::Texture& texEraser,
                 const sf::Texture& texFill, const sf::Texture& texMove,
                 const sf::Texture& texPointer) {
    for (const auto& btn : ComputeToolbarLayout()) {
        bool active = (btn.tool == currentTool);
        sf::RectangleShape bg(btn.bounds.size);
        bg.setPosition(btn.bounds.position);
        bg.setFillColor(active ? sf::Color(90, 90, 240) : sf::Color(40, 40, 46));
        bg.setOutlineThickness(1.f);
        bg.setOutlineColor(sf::Color(70, 70, 80));
        window.draw(bg);

        const sf::Texture* tex = nullptr;
        if (btn.tool == Tool::Pencil) tex = &texPencil;
        else if (btn.tool == Tool::Eraser) tex = &texEraser;
        else if (btn.tool == Tool::Fill) tex = &texFill;
        else if (btn.tool == Tool::Move) tex = &texMove;
        else if (btn.tool == Tool::Pointer) tex = &texPointer;

        sf::Vector2u texSize = (tex && tex->getSize().x > 1 && tex->getSize().y > 1) ? tex->getSize() : sf::Vector2u(0, 0);
        if (texSize.x > 0 && texSize.y > 0) {
            sf::Sprite icon(*tex);
            float targetSize = 28.f;
            float scaleX = targetSize / static_cast<float>(texSize.x);
            float scaleY = targetSize / static_cast<float>(texSize.y);
            icon.setScale({scaleX, scaleY});
            icon.setPosition({
                btn.bounds.position.x + (btn.bounds.size.x - targetSize) / 2.f,
                btn.bounds.position.y + (btn.bounds.size.y - targetSize) / 2.f
            });
            window.draw(icon);
        } else {
            // Fallback text if an icon fails to load
            sf::Text label(uiFont, btn.label, 18);
            label.setFillColor(sf::Color::White);
            sf::FloatRect lb = label.getLocalBounds();
            label.setPosition({btn.bounds.position.x + btn.bounds.size.x / 2.f - lb.size.x / 2.f,
                                btn.bounds.position.y + btn.bounds.size.y / 2.f - lb.size.y / 2.f - 4.f});
            window.draw(label);
        }
    }
}

struct PaletteItem {
    int tileIndex;
    sf::FloatRect bounds;
};

// One horizontally-scrollable row of tile thumbnails, offset by `scrollX`.
std::vector<PaletteItem> ComputePaletteLayout(const TileSet& tileSet, const sf::FloatRect& paletteRect, float scrollX) {
    std::vector<PaletteItem> items;
    float x = paletteRect.position.x + kPalettePad - scrollX;
    float y = paletteRect.position.y + (paletteRect.size.y - kPaletteItemSize) / 2.f;
    for (int i = 0; i < tileSet.tileCount(); ++i) {
        items.push_back({i, sf::FloatRect({x, y}, {kPaletteItemSize, kPaletteItemSize})});
        x += kPaletteItemSize + kPalettePad;
    }
    return items;
}

float PaletteContentWidth(const TileSet& tileSet) {
    if (tileSet.tileCount() <= 0) return 0.f;
    return static_cast<float>(tileSet.tileCount()) * (kPaletteItemSize + kPalettePad) + kPalettePad;
}

void DrawPalette(sf::RenderWindow& window, sf::Font& uiFont, const TileSet& tileSet,
                  const sf::FloatRect& paletteRect, float scrollX, int selectedTileIndex) {
    sf::RectangleShape bg(paletteRect.size);
    bg.setPosition(paletteRect.position);
    bg.setFillColor(sf::Color(24, 24, 28));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(60, 60, 68));
    window.draw(bg);

    if (!tileSet.loaded || tileSet.tileCount() <= 0) {
        sf::Text hint(uiFont, "No tileset imported - [I] Import an image to fill this palette", 14);
        hint.setFillColor(sf::Color(140, 140, 150));
        hint.setPosition({paletteRect.position.x + kPalettePad, paletteRect.position.y + paletteRect.size.y / 2.f - 8.f});
        window.draw(hint);
        return;
    }

    for (const auto& item : ComputePaletteLayout(tileSet, paletteRect, scrollX)) {
        if (item.bounds.position.x + item.bounds.size.x < paletteRect.position.x ||
            item.bounds.position.x > paletteRect.position.x + paletteRect.size.x) {
            continue; // scrolled out of view
        }
        bool active = (item.tileIndex == selectedTileIndex);

        sf::RectangleShape slot(item.bounds.size);
        slot.setPosition(item.bounds.position);
        slot.setFillColor(sf::Color(40, 40, 46));
        slot.setOutlineThickness(active ? 3.f : 1.f);
        slot.setOutlineColor(active ? sf::Color(120, 170, 255) : sf::Color(70, 70, 80));
        window.draw(slot);

        sf::Sprite icon(tileSet.texture, tileSet.tileRect(item.tileIndex));
        sf::FloatRect texBounds = icon.getLocalBounds();
        float pad = 4.f;
        float scaleX = (item.bounds.size.x - pad * 2.f) / std::max(1.f, texBounds.size.x);
        float scaleY = (item.bounds.size.y - pad * 2.f) / std::max(1.f, texBounds.size.y);
        icon.setScale({scaleX, scaleY});
        icon.setPosition({item.bounds.position.x + pad, item.bounds.position.y + pad});
        window.draw(icon);
    }
}

} // namespace

bool RunMapEditorSession(sf::RenderWindow& window,
                          const std::string& projectNameIn,
                          unsigned int WINDOW_WIDTH,
                          unsigned int WINDOW_HEIGHT,
                          sf::Font& uiFont,
                          int mapWidthTiles,
                          int mapHeightTiles,
                          bool isNewProject) {
    const float GRID_SIZE = 40.f;
    std::string projectName = projectNameIn;

    if (!window.isOpen()) return true;

    std::filesystem::create_directories("main/assets/map/" + projectName);
    const std::string SAVE_PATH = "main/assets/map/" + projectName + "/map.json";

    sf::FloatRect initialCanvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
    sf::View camera(sf::FloatRect({0.f, 0.f}, initialCanvasRect.size));
    ApplyCanvasViewport(camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    window.setView(camera);

    // ---------------- Load UI Tool Icons ----------------
    sf::Texture texPointer, texPencil, texEraser, texFill, texMove;
    const std::string ICON_PATH = "main/assets/images/editor/UI/icons/";
    bool pointerLoaded = texPointer.loadFromFile(ICON_PATH + "mouse-pointer-icon.png");
    bool pencilLoaded = texPencil.loadFromFile(ICON_PATH + "pencil-icon.png");
    bool eraserLoaded = texEraser.loadFromFile(ICON_PATH + "eraser-icon.png");
    bool fillLoaded   = texFill.loadFromFile(ICON_PATH + "color-fill-tool-icon.png");
    bool moveLoaded   = texMove.loadFromFile(ICON_PATH + "move-drag-arrow-icon.png");

    sf::Texture editIconTexture;
    bool editIconLoaded = editIconTexture.loadFromFile("main/assets/images/UI/icons/edit.png");

    // ---------------- Editor state ----------------
    EditorState editor;

    // Auto-load the project's existing map, if any.
    if (std::filesystem::exists(SAVE_PATH)) {
        editor.loadFromFile(SAVE_PATH);
    }
    size_t savedUndoDepth = editor.undoDepth();
    auto isDirty = [&]() { return editor.undoDepth() != savedUndoDepth; };

    // ---------------- Tile layer / tile set state ----------------
    const std::string TILESET_META_PATH = "main/assets/map/" + projectName + "/tileset.meta";
    const std::string TILE_LAYER_PATH = "main/assets/map/" + projectName + "/tiles.layer";
    const std::string RAW_IMAGE_FOLDER = "main/assets/raw";

    EditorMode editorMode = EditorMode::TilePaint;
    TileSet tileSet;
    TileLayer tileLayer = isNewProject ? TileLayer(mapWidthTiles, mapHeightTiles) : TileLayer();
    int selectedTileIndex = 0;
    Tool currentTool = Tool::Pencil;
    TileImportState importState;

    // ---- Pointer tool: last tile clicked, shown in the details panel ----
    int pointerTileX = -1;
    int pointerTileY = -1;
    bool tileDetailsOpen = false;

    // ---- Bottom tile-piece palette ----
    float paletteScrollX = 0.f;

    // ---- Map resize dialog ----
    bool resizingMap = false;
    bool resizeConfirming = false; // true = showing the "data outside will be deleted" warning
    int resizeSelectedPreset = 1;
    bool resizeEnteringCustom = false;
    std::string resizeCustomBuffer;
    int resizePendingW = 0;
    int resizePendingH = 0;
    std::string resizeError;
    sf::Clock resizeErrorClock;
    int resizeLastClickedPreset = -1;
    sf::Clock resizeDoubleClickClock;
    const float DOUBLE_CLICK_MS = 350.f;

    if (tileSet.loadMeta(TILESET_META_PATH)) {
        tileLayer.load(TILE_LAYER_PATH);
        tileLayer.rebuildVertices(tileSet);
    }

    auto openImportPanel = [&]() {
        importState.open = true;
        importState.images = ListImportableImages(RAW_IMAGE_FOLDER);
        importState.selectedImage = importState.images.empty() ? -1 : 0;
        importState.editingField = ImportField::None;
        importState.previewLoaded = !importState.images.empty() &&
            importState.previewTexture.loadFromFile(RAW_IMAGE_FOLDER + "/" + importState.images[0]);
    };

    auto confirmImport = [&]() {
        if (importState.selectedImage < 0 || importState.selectedImage >= static_cast<int>(importState.images.size())) return;
        std::string path = RAW_IMAGE_FOLDER + "/" + importState.images[importState.selectedImage];
        if (tileSet.loadFromImage(path, importState.tileW, importState.tileH)) {
            tileSet.saveMeta(TILESET_META_PATH);
            tileLayer.rebuildVertices(tileSet);
            selectedTileIndex = 0;
            paletteScrollX = 0.f;
            editorMode = EditorMode::TilePaint;
            importState.open = false;
        }
    };

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

            // ---- Sprite-sheet import panel takes priority over everything ----
            if (importState.open) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    if (importState.editingField != ImportField::None) {
                        char32_t unicode = textEntered->unicode;
                        if (unicode == 8) {
                            if (!importState.editBuffer.empty()) importState.editBuffer.pop_back();
                        } else if (unicode >= '0' && unicode <= '9') {
                            importState.editBuffer += static_cast<char>(unicode);
                        }
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        if (importState.editingField != ImportField::None) importState.editingField = ImportField::None;
                        else importState.open = false;
                    } else if (keyPressed->code == sf::Keyboard::Key::Up && importState.editingField == ImportField::None) {
                        if (!importState.images.empty()) {
                            importState.selectedImage = (importState.selectedImage - 1 + static_cast<int>(importState.images.size())) % static_cast<int>(importState.images.size());
                            importState.previewLoaded = importState.previewTexture.loadFromFile(RAW_IMAGE_FOLDER + "/" + importState.images[importState.selectedImage]);
                        }
                    } else if (keyPressed->code == sf::Keyboard::Key::Down && importState.editingField == ImportField::None) {
                        if (!importState.images.empty()) {
                            importState.selectedImage = (importState.selectedImage + 1) % static_cast<int>(importState.images.size());
                            importState.previewLoaded = importState.previewTexture.loadFromFile(RAW_IMAGE_FOLDER + "/" + importState.images[importState.selectedImage]);
                        }
                    } else if (keyPressed->code == sf::Keyboard::Key::Tab) {
                        importState.editingField = (importState.editingField == ImportField::TileW) ? ImportField::TileH : ImportField::TileW;
                        importState.editBuffer = std::to_string(importState.editingField == ImportField::TileW ? importState.tileW : importState.tileH);
                    } else if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (importState.editingField != ImportField::None) {
                            int v = ParseNumberOr(importState.editBuffer, 32.f) > 0 ? static_cast<int>(ParseNumberOr(importState.editBuffer, 32.f)) : 32;
                            if (importState.editingField == ImportField::TileW) importState.tileW = v; else importState.tileH = v;
                            importState.editingField = ImportField::None;
                        } else {
                            confirmImport();
                        }
                    }
                }
                continue; // swallow all other input while the import panel is up
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
                sf::Vector2f wheelScreenPos(static_cast<float>(wheel->position.x), static_cast<float>(wheel->position.y));
                sf::FloatRect paletteRect = ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                if (paletteRect.contains(wheelScreenPos)) {
                    float contentWidth = PaletteContentWidth(tileSet);
                    float maxScroll = std::max(0.f, contentWidth - paletteRect.size.x);
                    paletteScrollX -= wheel->delta * 40.f;
                    paletteScrollX = std::clamp(paletteScrollX, 0.f, maxScroll);
                } else {
                    float zoomFactor = (wheel->delta > 0) ? 0.9f : 1.1f;
                    camera.zoom(zoomFactor);
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f screenPos(static_cast<float>(mousePressed->position.x),
                                        static_cast<float>(mousePressed->position.y));

                // ---- Top bar buttons hit-test ----
                float importBtnX = static_cast<float>(WINDOW_WIDTH) - kInspectorWidth - 110.f - 10.f;
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    if (sf::FloatRect({importBtnX, 10.f}, {100.f, 32.f}).contains(screenPos)) {
                        openImportPanel();
                        continue;
                    }
                }

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

                // ---- Toolbar hit-test (Always active) ----
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    bool hitToolbar = false;
                    for (const auto& btn : ComputeToolbarLayout()) {
                        if (btn.bounds.contains(screenPos)) {
                            currentTool = btn.tool;
                            editorMode = EditorMode::TilePaint; // Auto-switches to tile paint mode when clicked
                            hitToolbar = true;
                            break;
                        }
                    }
                    if (hitToolbar) continue;
                }

                // ---- Resize icon hit-test ----
                if (mousePressed->button == sf::Mouse::Button::Left && !resizingMap) {
                    if (ComputeResizeIconBounds().contains(screenPos)) {
                        resizingMap = true;
                        resizeConfirming = false;
                        resizeEnteringCustom = false;
                        resizeCustomBuffer.clear();
                        resizeSelectedPreset = 1;
                        continue;
                    }
                }

                // ---- Resize dialog is modal: swallow all clicks while open ----
                if (resizingMap) {
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        float cardX = kCanvasMarginSides + 40.f;
                        float cardY = 60.f;
                        for (int i = 0; i < kResizePresetCount; ++i) {
                            sf::FloatRect optRect({cardX + 20.f, cardY + 40.f + i * 34.f}, {440.f, 28.f});
                            if (optRect.contains(screenPos)) {
                                resizeEnteringCustom = false;
                                resizeSelectedPreset = i;

                                bool isDoubleClick = (resizeLastClickedPreset == i &&
                                    resizeDoubleClickClock.getElapsedTime().asMilliseconds() < DOUBLE_CLICK_MS);
                                resizeDoubleClickClock.restart();
                                resizeLastClickedPreset = i;
                                if (isDoubleClick) {
                                    resizePendingW = kResizePresets[i].w;
                                    resizePendingH = kResizePresets[i].h;
                                    resizeConfirming = true;
                                }
                            }
                        }
                        sf::FloatRect customRect({cardX + 20.f, cardY + 40.f + kResizePresetCount * 34.f}, {440.f, 28.f});
                        if (customRect.contains(screenPos)) {
                            resizeEnteringCustom = true;
                            resizeCustomBuffer.clear();
                        }
                        sf::FloatRect confirmRect({cardX + 20.f, cardY + 40.f + (kResizePresetCount + 2) * 34.f}, {200.f, 32.f});
                        sf::FloatRect cancelRect({cardX + 240.f, cardY + 40.f + (kResizePresetCount + 2) * 34.f}, {200.f, 32.f});
                        if (resizeConfirming && confirmRect.contains(screenPos)) {
                            TileLayer resized(resizePendingW, resizePendingH, tileLayer.tileSize());
                            int copyW = std::min(resizePendingW, tileLayer.widthTiles());
                            int copyH = std::min(resizePendingH, tileLayer.heightTiles());
                            for (int ty = 0; ty < copyH; ++ty)
                                for (int tx = 0; tx < copyW; ++tx)
                                    resized.set(tx, ty, tileLayer.get(tx, ty));
                            tileLayer = resized;
                            tileLayer.rebuildVertices(tileSet);
                            tileLayer.save(TILE_LAYER_PATH);
                            resizingMap = false;
                            resizeConfirming = false;
                        } else if (resizeConfirming && cancelRect.contains(screenPos)) {
                            resizeConfirming = false;
                        }
                    }
                    continue;
                }

                // ---- Bottom palette hit-test: pick which tile piece paints next ----
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    sf::FloatRect paletteRect = ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                    if (paletteRect.contains(screenPos)) {
                        for (const auto& item : ComputePaletteLayout(tileSet, paletteRect, paletteScrollX)) {
                            if (item.bounds.contains(screenPos)) {
                                selectedTileIndex = item.tileIndex;
                                break;
                            }
                        }
                        continue;
                    }
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
                else if (editorMode == EditorMode::TilePaint && currentTool == Tool::Move &&
                         mousePressed->button == sf::Mouse::Button::Left) {
                    panningCamera = true;
                    lastMousePixel = mousePressed->position;
                    continue;
                }
                else if (editorMode == EditorMode::TilePaint && mousePressed->button == sf::Mouse::Button::Left) {
                    sf::Vector2i cell = tileLayer.worldToTile(worldPos);
                    if (currentTool == Tool::Pencil) tileLayer.set(cell.x, cell.y, selectedTileIndex);
                    else if (currentTool == Tool::Eraser) tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                    else if (currentTool == Tool::Fill) FloodFillTile(tileLayer, cell.x, cell.y, selectedTileIndex);
                    else if (currentTool == Tool::Pointer) {
                        pointerTileX = cell.x;
                        pointerTileY = cell.y;
                        tileDetailsOpen = true;
                        editor.clearSelection();
                    }
                    continue;
                }
                else if (editorMode == EditorMode::TilePaint && currentTool == Tool::Eraser &&
                         mousePressed->button == sf::Mouse::Button::Right) {
                    sf::Vector2i cell = tileLayer.worldToTile(worldPos);
                    tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                    continue;
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
                    if (currentTool == Tool::Move) {
                        panningCamera = false;
                    }
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
                if (editorMode == EditorMode::TilePaint && currentTool != Tool::Move &&
                    sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                    sf::FloatRect canvasRectNow = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                    sf::Vector2f screenPosNow(static_cast<float>(mouseMoved->position.x), static_cast<float>(mouseMoved->position.y));
                    if (canvasRectNow.contains(screenPosNow)) {
                        sf::Vector2f worldPos = window.mapPixelToCoords(mouseMoved->position, camera);
                        sf::Vector2i cell = tileLayer.worldToTile(worldPos);
                        if (currentTool == Tool::Pencil) tileLayer.set(cell.x, cell.y, selectedTileIndex);
                        else if (currentTool == Tool::Eraser) tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                    }
                }
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

            if (resizingMap) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    if (resizeEnteringCustom && !resizeConfirming) {
                        char32_t unicode = textEntered->unicode;
                        if (unicode == 8) {
                            if (!resizeCustomBuffer.empty()) resizeCustomBuffer.pop_back();
                        } else if (unicode >= '0' && unicode <= '9') {
                            resizeCustomBuffer += static_cast<char>(unicode);
                        } else if (unicode == 'x' || unicode == 'X') {
                            if (!resizeCustomBuffer.empty() && resizeCustomBuffer.back() != 'x') resizeCustomBuffer += 'x';
                        }
                    }
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (resizeConfirming) {
                        if (keyPressed->code == sf::Keyboard::Key::Enter) {
                            TileLayer resized(resizePendingW, resizePendingH, tileLayer.tileSize());
                            int copyW = std::min(resizePendingW, tileLayer.widthTiles());
                            int copyH = std::min(resizePendingH, tileLayer.heightTiles());
                            for (int ty = 0; ty < copyH; ++ty)
                                for (int tx = 0; tx < copyW; ++tx)
                                    resized.set(tx, ty, tileLayer.get(tx, ty));
                            tileLayer = resized;
                            tileLayer.rebuildVertices(tileSet);
                            tileLayer.save(TILE_LAYER_PATH);
                            resizingMap = false;
                            resizeConfirming = false;
                        } else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                            resizeConfirming = false;
                        }
                    } else if (keyPressed->code == sf::Keyboard::Key::Left ||
                               keyPressed->code == sf::Keyboard::Key::Up) {
                        resizeEnteringCustom = false;
                        resizeSelectedPreset = (resizeSelectedPreset - 1 + kResizePresetCount) % kResizePresetCount;
                    } else if (keyPressed->code == sf::Keyboard::Key::Right ||
                               keyPressed->code == sf::Keyboard::Key::Down) {
                        resizeEnteringCustom = false;
                        resizeSelectedPreset = (resizeSelectedPreset + 1) % kResizePresetCount;
                    } else if (keyPressed->code == sf::Keyboard::Key::C) {
                        resizeEnteringCustom = true;
                        resizeCustomBuffer.clear();
                    } else if (keyPressed->code == sf::Keyboard::Key::Enter) {
                        if (resizeEnteringCustom) {
                            size_t xPos = resizeCustomBuffer.find('x');
                            bool ok = false;
                            int w = 0, h = 0;
                            if (xPos != std::string::npos && xPos > 0 && xPos + 1 < resizeCustomBuffer.size()) {
                                try {
                                    w = std::stoi(resizeCustomBuffer.substr(0, xPos));
                                    h = std::stoi(resizeCustomBuffer.substr(xPos + 1));
                                    ok = (w >= 10 && w <= 2000 && h >= 10 && h <= 2000);
                                } catch (...) { ok = false; }
                            }
                            if (!ok) {
                                resizeError = "[Error!] Enter a size like 250x150 (10-2000 each)";
                                resizeErrorClock.restart();
                            } else {
                                resizePendingW = w;
                                resizePendingH = h;
                                resizeConfirming = true;
                            }
                        } else {
                            resizePendingW = kResizePresets[resizeSelectedPreset].w;
                            resizePendingH = kResizePresets[resizeSelectedPreset].h;
                            resizeConfirming = true;
                        }
                    } else if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        resizingMap = false;
                        resizeEnteringCustom = false;
                        resizeConfirming = false;
                    }
                }
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

                if (keyPressed->code == sf::Keyboard::Key::Num1) currentType = ObjectType::Block;
                else if (keyPressed->code == sf::Keyboard::Key::Num2) currentType = ObjectType::Spike;
                else if (keyPressed->code == sf::Keyboard::Key::Num3) currentType = ObjectType::Platform;
                else if (keyPressed->code == sf::Keyboard::Key::Num4) currentType = ObjectType::Coin;
                else if (keyPressed->code == sf::Keyboard::Key::G) snapEnabled = !snapEnabled;
                else if (keyPressed->code == sf::Keyboard::Key::T) {
                    editorMode = (editorMode == EditorMode::Objects) ? EditorMode::TilePaint : EditorMode::Objects;
                }
                else if (editorMode == EditorMode::TilePaint && keyPressed->code == sf::Keyboard::Key::P) {
                    currentTool = Tool::Pencil;
                }
                else if (editorMode == EditorMode::TilePaint && keyPressed->code == sf::Keyboard::Key::E) {
                    currentTool = Tool::Eraser;
                }
                else if (editorMode == EditorMode::TilePaint && keyPressed->code == sf::Keyboard::Key::F) {
                    currentTool = Tool::Fill;
                }
                else if (editorMode == EditorMode::TilePaint && keyPressed->code == sf::Keyboard::Key::M) {
                    currentTool = Tool::Move;
                }
                else if (editorMode == EditorMode::TilePaint && keyPressed->code == sf::Keyboard::Key::N) {
                    currentTool = Tool::Pointer;
                }
                else if (keyPressed->code == sf::Keyboard::Key::I) {
                    openImportPanel();
                }
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
                    tileLayer.save(TILE_LAYER_PATH);
                    savedUndoDepth = editor.undoDepth();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::O) {
                    editor.loadFromFile(SAVE_PATH);
                    tileLayer.load(TILE_LAYER_PATH);
                    tileLayer.rebuildVertices(tileSet);
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

        if (tileLayer.dirty) tileLayer.rebuildVertices(tileSet);
        tileLayer.draw(window, tileSet); // background art, drawn beneath objects
        DrawTileGrid(window, camera, tileLayer); // faint grid + bright map-bounds outline

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

        // --- Top Bar Navigation / Mode Buttons ---
        std::string sizeLabelStr = std::to_string(tileLayer.widthTiles()) + " x " +
                                    std::to_string(tileLayer.heightTiles()) + (isDirty() ? " *" : "");
        sf::Text sizeLabel(uiFont, sizeLabelStr, 18);
        sizeLabel.setFillColor(sf::Color(200, 200, 210));
        sizeLabel.setPosition({kCanvasMarginSides, 16.f});
        window.draw(sizeLabel);

        sf::FloatRect resizeIconBounds = ComputeResizeIconBounds();
        sf::RectangleShape resizeIconBg(resizeIconBounds.size);
        resizeIconBg.setPosition(resizeIconBounds.position);
        resizeIconBg.setFillColor(sf::Color(45, 45, 52));
        resizeIconBg.setOutlineThickness(1.f);
        resizeIconBg.setOutlineColor(sf::Color(80, 80, 90));
        window.draw(resizeIconBg);
        if (editIconLoaded) {
            sf::Sprite editSprite(editIconTexture);
            sf::Vector2u texSize = editIconTexture.getSize();
            float scale = (texSize.x > 0) ? (14.f / static_cast<float>(texSize.x)) : 1.f;
            editSprite.setScale({scale, scale});
            editSprite.setPosition({resizeIconBounds.position.x + 3.f, resizeIconBounds.position.y + 3.f});
            window.draw(editSprite);
        } else {
            sf::Text editFallback(uiFont, "E", 14);
            editFallback.setFillColor(sf::Color(200, 200, 210));
            editFallback.setPosition({resizeIconBounds.position.x + 5.f, resizeIconBounds.position.y + 1.f});
            window.draw(editFallback);
        }

        if (resizingMap) {
            float cardX = kCanvasMarginSides + 40.f;
            float cardY = 60.f;
            float cardH = 40.f + (kResizePresetCount + (resizeConfirming ? 3 : 1)) * 34.f + 20.f;

            sf::RectangleShape overlay({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            overlay.setFillColor(sf::Color(0, 0, 0, 140));
            window.draw(overlay);

            sf::RectangleShape card({480.f, cardH});
            card.setFillColor(sf::Color(38, 38, 44));
            card.setOutlineThickness(2.f);
            card.setOutlineColor(sf::Color(90, 90, 240));
            card.setPosition({cardX, cardY});
            window.draw(card);

            sf::Text label(uiFont, "Resize Map (tiles)", 18);
            label.setFillColor(sf::Color(150, 150, 160));
            label.setPosition({cardX + 20.f, cardY + 10.f});
            window.draw(label);

            for (int i = 0; i < kResizePresetCount; ++i) {
                bool selected = !resizeEnteringCustom && resizeSelectedPreset == i;
                sf::RectangleShape row({440.f, 28.f});
                row.setPosition({cardX + 20.f, cardY + 40.f + i * 34.f});
                row.setFillColor(selected ? sf::Color(50, 50, 90) : sf::Color(30, 30, 36));
                if (selected) {
                    row.setOutlineThickness(2.f);
                    row.setOutlineColor(sf::Color(90, 90, 240));
                }
                window.draw(row);

                sf::Text optText(uiFont, kResizePresets[i].label, 18);
                optText.setFillColor(sf::Color::White);
                optText.setPosition({cardX + 30.f, cardY + 44.f + i * 34.f});
                window.draw(optText);
            }

            bool cursorOn = std::fmod(cursorBlinkClock.getElapsedTime().asSeconds(), 1.f) < 0.5f;
            sf::RectangleShape customRow({440.f, 28.f});
            customRow.setPosition({cardX + 20.f, cardY + 40.f + kResizePresetCount * 34.f});
            customRow.setFillColor(resizeEnteringCustom ? sf::Color(50, 50, 90) : sf::Color(30, 30, 36));
            if (resizeEnteringCustom) {
                customRow.setOutlineThickness(2.f);
                customRow.setOutlineColor(sf::Color(90, 90, 240));
            }
            window.draw(customRow);

            std::string customLabel = "Custom: " + (resizeCustomBuffer.empty() ? std::string("e.g. 250x150") : resizeCustomBuffer);
            sf::Text customText(uiFont, customLabel + (resizeEnteringCustom && cursorOn ? "_" : ""), 18);
            customText.setFillColor(resizeEnteringCustom ? sf::Color::White : sf::Color(150, 150, 160));
            customText.setPosition({cardX + 30.f, cardY + 44.f + kResizePresetCount * 34.f});
            window.draw(customText);

            if (!resizeConfirming) {
                sf::Text hint(uiFont, "Left/Right to pick a preset, C for custom, Enter to continue, Esc to cancel", 15);
                hint.setFillColor(sf::Color(130, 130, 140));
                hint.setPosition({cardX, cardY + cardH - 26.f});
                window.draw(hint);
            } else {
                sf::Text warn(uiFont,
                    "Warning: tiles outside " + std::to_string(resizePendingW) + " x " +
                    std::to_string(resizePendingH) + " will be permanently deleted.", 15);
                warn.setFillColor(sf::Color(230, 160, 90));
                warn.setPosition({cardX + 20.f, cardY + 40.f + (kResizePresetCount + 1) * 34.f});
                window.draw(warn);

                sf::RectangleShape confirmBtn({200.f, 32.f});
                confirmBtn.setPosition({cardX + 20.f, cardY + 40.f + (kResizePresetCount + 2) * 34.f});
                confirmBtn.setFillColor(sf::Color(120, 50, 50));
                confirmBtn.setOutlineThickness(1.f);
                confirmBtn.setOutlineColor(sf::Color(200, 90, 90));
                window.draw(confirmBtn);
                sf::Text confirmText(uiFont, "Confirm Resize", 16);
                confirmText.setFillColor(sf::Color::White);
                confirmText.setPosition({cardX + 40.f, cardY + 48.f + (kResizePresetCount + 2) * 34.f});
                window.draw(confirmText);

                sf::RectangleShape cancelBtn({200.f, 32.f});
                cancelBtn.setPosition({cardX + 240.f, cardY + 40.f + (kResizePresetCount + 2) * 34.f});
                cancelBtn.setFillColor(sf::Color(50, 50, 60));
                cancelBtn.setOutlineThickness(1.f);
                cancelBtn.setOutlineColor(sf::Color(90, 90, 100));
                window.draw(cancelBtn);
                sf::Text cancelText(uiFont, "Cancel", 16);
                cancelText.setFillColor(sf::Color::White);
                cancelText.setPosition({cardX + 300.f, cardY + 48.f + (kResizePresetCount + 2) * 34.f});
                window.draw(cancelText);
            }

            if (!resizeError.empty() && resizeErrorClock.getElapsedTime().asSeconds() < 3.f) {
                sf::Text errText(uiFont, resizeError, 15);
                errText.setFillColor(sf::Color(230, 90, 90));
                errText.setPosition({cardX, cardY + cardH + 6.f});
                window.draw(errText);
            }
        }

        // Import Button (Aligned top-right)
        float importBtnX = static_cast<float>(WINDOW_WIDTH) - kInspectorWidth - 110.f - 10.f;
        sf::RectangleShape importBtn({100.f, 32.f});
        importBtn.setPosition({importBtnX, 10.f});
        importBtn.setFillColor(sf::Color(50, 50, 60));
        importBtn.setOutlineThickness(1.f);
        importBtn.setOutlineColor(sf::Color(90, 90, 100));
        window.draw(importBtn);

        sf::Text importTxt(uiFont, "[I] Import", 14);
        importTxt.setFillColor(sf::Color::White);
        importTxt.setPosition({importBtnX + 16.f, 16.f});
        window.draw(importTxt);

        // Draw toolbar unconditionally so it is always visible
        DrawToolbar(window, uiFont, currentTool, texPencil, texEraser, texFill, texMove, texPointer);

        // Bottom tile-piece palette (pieces from the imported tileset: Box/Circle/Triangle/etc.)
        DrawPalette(window, uiFont, tileSet, ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT), paletteScrollX, selectedTileIndex);

        // --- Render Tile Import Modal ---
        if (importState.open) {
            sf::RectangleShape dim({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            dim.setFillColor(sf::Color(0, 0, 0, 180));
            window.draw(dim);

            float boxW = 500.f, boxH = 300.f;
            float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
            float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;

            sf::RectangleShape box({boxW, boxH});
            box.setPosition({boxX, boxY});
            box.setFillColor(sf::Color(35, 35, 42));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(sf::Color(90, 90, 240));
            window.draw(box);

            sf::Text title(uiFont, "Import Image ([Up/Down] Select, [Enter] Confirm)", 16);
            title.setFillColor(sf::Color::White);
            title.setPosition({boxX + 20.f, boxY + 20.f});
            window.draw(title);

            float imgY = boxY + 60.f;
            for (size_t i = 0; i < importState.images.size(); ++i) {
                sf::Text imgText(uiFont, importState.images[i], 15);
                imgText.setFillColor(static_cast<int>(i) == importState.selectedImage ? sf::Color(100, 200, 255) : sf::Color(180, 180, 190));
                imgText.setPosition({boxX + 25.f, imgY});
                window.draw(imgText);
                imgY += 24.f;
            }

            sf::Text sizeText(uiFont, "Tile W: " + std::to_string(importState.tileW) + " | Tile H: " + std::to_string(importState.tileH) + " ([Tab] Edit)", 14);
            sizeText.setFillColor(sf::Color(200, 200, 200));
            sizeText.setPosition({boxX + 20.f, boxY + boxH - 40.f});
            window.draw(sizeText);
        }

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
        else if (tileDetailsOpen && currentTool == Tool::Pointer && pointerTileX != -1) {
            sf::RectangleShape panelBg({kInspectorWidth, static_cast<float>(WINDOW_HEIGHT)});
            panelBg.setPosition({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth, 0.f});
            panelBg.setFillColor(sf::Color(24, 24, 28, 235));
            window.draw(panelBg);

            sf::Text header(uiFont, "Tile Details", 18);
            header.setFillColor(sf::Color::White);
            header.setPosition({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth + kRowPad, 8.f});
            window.draw(header);

            int tileIndex = tileLayer.get(pointerTileX, pointerTileY);
            std::string tileValueStr = (tileIndex == TileLayer::kEmpty) ? "Empty" : std::to_string(tileIndex);

            sf::FloatRect cellRow({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth + kRowPad, 40.f},
                                   {kInspectorWidth - kRowPad * 2.f, kRowHeight});
            sf::FloatRect tileRow({static_cast<float>(WINDOW_WIDTH) - kInspectorWidth + kRowPad, 40.f + kRowHeight},
                                   {kInspectorWidth - kRowPad * 2.f, kRowHeight});

            DrawInspectorRow(window, uiFont, cellRow, "Cell",
                              std::to_string(pointerTileX) + ", " + std::to_string(pointerTileY), false);
            DrawInspectorRow(window, uiFont, tileRow, "Tile", tileValueStr, false);
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