#include "zero_editor_session.h"

#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "MapObject.h"
#include "EditorState.h"
#include "TileSet.h"
#include "TileLayer.h"

// Native "Open File" dialog for the tileset-import picker.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <iostream>

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

// Undo/Redo buttons, placed right after the map-size edit button.
constexpr float kUndoIconX = kResizeIconX + kResizeIconSize + 10.f;
constexpr float kRedoIconX = kUndoIconX + kResizeIconSize + 10.f;

sf::FloatRect ComputeUndoIconBounds() {
    return sf::FloatRect({kUndoIconX, kResizeIconY}, {kResizeIconSize, kResizeIconSize});
}
sf::FloatRect ComputeRedoIconBounds() {
    return sf::FloatRect({kRedoIconX, kResizeIconY}, {kResizeIconSize, kResizeIconSize});
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

// Parallel grid to TileLayer, storing which basic shape (Square/Circle/
// Triangle/Slope/Slope-Mirror, or -1 for none) occupies each cell. Kept
// separate from TileLayer/TileSet since those only know about image-sliced
// tiles; shapes are drawn procedurally instead. A cell holds either a tile
// index (in TileLayer) or a shape id here, never both.
struct ShapeGrid {
    int width = 0, height = 0;
    std::vector<int> cells;

    void resize(int w, int h) {
        width = w; height = h;
        cells.assign(static_cast<size_t>(w) * h, -1);
    }
    int get(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return -1;
        return cells[static_cast<size_t>(y) * width + x];
    }
    void set(int x, int y, int shapeId) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        cells[static_cast<size_t>(y) * width + x] = shapeId;
    }
    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << width << " " << height << "\n";
        for (int v : cells) f << v << " ";
        return true;
    }
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        int w = 0, h = 0;
        f >> w >> h;
        if (w <= 0 || h <= 0) return false;
        resize(w, h);
        for (int i = 0; i < w * h && f; ++i) f >> cells[i];
        return true;
    }
};

constexpr const char* kBasicShapeNames[] = { "Square", "Circle", "Triangle", "Slope", "Slope (Mirror)" };
constexpr int kBasicShapeCount = 5;

// Draws one basic shape (id 0-4, see kBasicShapeNames) centered at `center`,
// `size` wide/tall. Shared by the palette icons and the map's painted shapes.
void DrawBasicShape(sf::RenderWindow& window, int shapeId, sf::Vector2f center, float size, sf::Color color) {
    float s = size + 1.f; // slight overdraw so adjacent tiles overlap by half a pixel each side, eliminating seams
    switch (shapeId) {
        case 0: { // Square
            sf::RectangleShape sq({s, s});
            sq.setOrigin({s / 2.f, s / 2.f});
            sq.setPosition(center);
            sq.setFillColor(color);
            window.draw(sq);
            break;
        }
        case 1: { // Circle
            sf::CircleShape circ(s / 2.f);
            circ.setOrigin({s / 2.f, s / 2.f});
            circ.setPosition(center);
            circ.setFillColor(color);
            window.draw(circ);
            break;
        }
        case 2: { // Triangle
            sf::ConvexShape tri(3);
            tri.setPoint(0, {center.x - s / 2.f, center.y + s / 2.f});
            tri.setPoint(1, {center.x + s / 2.f, center.y + s / 2.f});
            tri.setPoint(2, {center.x, center.y - s / 2.f});
            tri.setFillColor(color);
            window.draw(tri);
            break;
        }
        case 3: { // Slope (rising left -> right)
            sf::ConvexShape slope(3);
            slope.setPoint(0, {center.x - s / 2.f, center.y + s / 2.f});
            slope.setPoint(1, {center.x + s / 2.f, center.y + s / 2.f});
            slope.setPoint(2, {center.x + s / 2.f, center.y - s / 2.f});
            slope.setFillColor(color);
            window.draw(slope);
            break;
        }
        case 4: { // Slope (mirrored: rising right -> left)
            sf::ConvexShape slopeMirror(3);
            slopeMirror.setPoint(0, {center.x + s / 2.f, center.y + s / 2.f});
            slopeMirror.setPoint(1, {center.x - s / 2.f, center.y + s / 2.f});
            slopeMirror.setPoint(2, {center.x - s / 2.f, center.y - s / 2.f});
            slopeMirror.setFillColor(color);
            window.draw(slopeMirror);
            break;
        }
    }
}

// Draws every placed shape currently visible in `camera`.
void DrawShapeGrid(sf::RenderWindow& window, const sf::View& camera, const TileLayer& tileLayer, const ShapeGrid& shapes) {
    float tileSize = tileLayer.tileSize();
    if (tileSize <= 0.f || shapes.width <= 0 || shapes.height <= 0) return;

    sf::FloatRect view(camera.getCenter() - camera.getSize() / 2.f, camera.getSize());
    int firstCol = std::max(0, static_cast<int>(std::floor(view.position.x / tileSize)));
    int lastCol  = std::min(shapes.width - 1, static_cast<int>(std::ceil((view.position.x + view.size.x) / tileSize)));
    int firstRow = std::max(0, static_cast<int>(std::floor(view.position.y / tileSize)));
    int lastRow  = std::min(shapes.height - 1, static_cast<int>(std::ceil((view.position.y + view.size.y) / tileSize)));

    for (int y = firstRow; y <= lastRow; ++y) {
        for (int x = firstCol; x <= lastCol; ++x) {
            int shapeId = shapes.get(x, y);
            if (shapeId < 0) continue;
            sf::Vector2f center(x * tileSize + tileSize / 2.f, y * tileSize + tileSize / 2.f);
            DrawBasicShape(window, shapeId, center, tileSize, sf::Color(150, 170, 220));
        }
    }
}

// Preset dimensions used by the resize dialog.
struct MapSizePreset {
    const char* label;
    int w;
    int h;
};

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
enum class ExitPrompt { None, ConfirmHome, ConfirmSaveHome, ConfirmSaveQuit, ConfirmQuit };
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

enum class AddPanelStage { None, ChooseAction, Create, ImportSize };

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

// "Add" button, pinned to the right edge of the palette bar.
sf::FloatRect ComputeAddButtonBounds(unsigned int WINDOW_WIDTH, unsigned int WINDOW_HEIGHT) {
    sf::FloatRect paletteRect = ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT);
    float size = kPaletteItemSize;
    float x = paletteRect.position.x + paletteRect.size.x - size - kPalettePad;
    float y = paletteRect.position.y + (paletteRect.size.y - size) / 2.f;
    return sf::FloatRect({x, y}, {size, size});
}

// Native Windows "Open File" dialog, filtered to common image types.
std::string OpenFileDialogForImage() {
    char fileBuffer[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Select a tileset image";
    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileBuffer);
    }
    return "";
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
                  const sf::FloatRect& paletteRect, float scrollX, int selectedTileIndex, int selectedShapeId,
                  Tool currentTool) {
    sf::RectangleShape bg(paletteRect.size);
    bg.setPosition(paletteRect.position);
    bg.setFillColor(sf::Color(24, 24, 28));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(60, 60, 68));
    window.draw(bg);

    if (!tileSet.loaded || tileSet.tileCount() <= 0) {
        sf::Text hint(uiFont, "No tileset imported - pick a basic shape below, or [I] Import an image", 13);
        hint.setFillColor(sf::Color(140, 140, 150));
        hint.setPosition({paletteRect.position.x + kPalettePad, paletteRect.position.y + 6.f});
        window.draw(hint);

        float x = paletteRect.position.x + kPalettePad;
        float y = paletteRect.position.y + 28.f;
        for (int i = 0; i < kBasicShapeCount; ++i) {
            sf::FloatRect slotBounds({x, y}, {kPaletteItemSize, kPaletteItemSize});
            bool active = (i == selectedShapeId) && currentTool == Tool::Pencil;

            sf::RectangleShape slot(slotBounds.size);
            slot.setPosition(slotBounds.position);
            slot.setFillColor(sf::Color(40, 40, 46));
            slot.setOutlineThickness(active ? 3.f : 1.f);
            slot.setOutlineColor(active ? sf::Color(120, 170, 255) : sf::Color(70, 70, 80));
            window.draw(slot);

            sf::Vector2f c(slotBounds.position.x + kPaletteItemSize / 2.f, slotBounds.position.y + kPaletteItemSize / 2.f);
            DrawBasicShape(window, i, c, kPaletteItemSize * 0.6f, sf::Color(150, 170, 220));

            sf::Text nameText(uiFont, kBasicShapeNames[i], 10);
            nameText.setFillColor(sf::Color(150, 150, 160));
            sf::FloatRect nb = nameText.getLocalBounds();
            nameText.setPosition({c.x - nb.size.x / 2.f, slotBounds.position.y + kPaletteItemSize + 2.f});
            window.draw(nameText);

            x += kPaletteItemSize + kPalettePad;
        }
        return;
    }

    for (const auto& item : ComputePaletteLayout(tileSet, paletteRect, scrollX)) {
        if (item.bounds.position.x + item.bounds.size.x < paletteRect.position.x ||
            item.bounds.position.x > paletteRect.position.x + paletteRect.size.x) {
            continue; // scrolled out of view
        }
        bool active = (item.tileIndex == selectedTileIndex) && currentTool == Tool::Pencil;

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

static void ToggleFullscreen(sf::RenderWindow& window, bool& isFullscreenNow,
                              sf::Vector2u& windowedSize, sf::Vector2i& windowedPosition) {
    if (!isFullscreenNow) {
        windowedSize = window.getSize();
        windowedPosition = window.getPosition();
        window.create(sf::VideoMode::getDesktopMode(), "Zero Theory - Map Editor",
                       sf::Style::None, sf::State::Fullscreen);
    } else {
        window.create(sf::VideoMode(windowedSize), "Zero Theory - Map Editor",
                       sf::Style::Titlebar | sf::Style::Resize | sf::Style::Close, sf::State::Windowed);
        window.setPosition(windowedPosition);
        window.setMinimumSize(sf::Vector2u(640, 360));
        window.setMaximumSize(sf::Vector2u(7680, 4320));
    }
    window.setFramerateLimit(60);
    isFullscreenNow = !isFullscreenNow;
}

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

    bool isFullscreenNow = false;
    sf::Vector2u windowedSize = window.getSize();
    sf::Vector2i windowedPosition = window.getPosition();

    // ---------------- Load UI Tool Icons ----------------
    sf::Texture texPointer, texPencil, texEraser, texFill, texMove;
    const std::string ICON_PATH = "main/assets/images/editor/UI/icons/";
    bool pointerLoaded = texPointer.loadFromFile(ICON_PATH + "mouse-pointer-icon.png");
    bool pencilLoaded = texPencil.loadFromFile(ICON_PATH + "pencil-icon.png");
    bool eraserLoaded = texEraser.loadFromFile(ICON_PATH + "eraser-icon.png");
    bool fillLoaded   = texFill.loadFromFile(ICON_PATH + "color-fill-tool-icon.png");
    bool moveLoaded   = texMove.loadFromFile(ICON_PATH + "move-drag-arrow-icon.png");

    sf::Texture texAddIcon, texImportIcon;
    bool addIconLoaded    = texAddIcon.loadFromFile(ICON_PATH + "add.png");
    bool importIconLoaded = texImportIcon.loadFromFile(ICON_PATH + "import-icon.png");

    sf::Texture editIconTexture;
    bool editIconLoaded = editIconTexture.loadFromFile("main/assets/images/UI/icons/edit.png");

    // ---------------- Editor state ----------------
    EditorState editor;

    // Auto-load the project's existing map, if any.
    if (std::filesystem::exists(SAVE_PATH)) {
        editor.loadFromFile(SAVE_PATH);
    }
    size_t savedUndoDepth = editor.undoDepth();
    bool tilesDirtySinceSave = false;
    auto isDirty = [&]() { return editor.undoDepth() != savedUndoDepth || tilesDirtySinceSave; };

    // ---------------- Tile layer / tile set state ----------------
    const std::string TILESET_META_PATH = "main/assets/map/" + projectName + "/tileset.meta";
    const std::string TILE_LAYER_PATH = "main/assets/map/" + projectName + "/tiles.layer";
    const std::string RAW_IMAGE_FOLDER = "main/assets/raw";

    EditorMode editorMode = EditorMode::TilePaint;
    TileSet tileSet;
    TileLayer tileLayer = isNewProject ? TileLayer(mapWidthTiles, mapHeightTiles) : TileLayer();
    int selectedTileIndex = 0;
    int selectedShapeId = 0; // which basic shape Pencil places when no tileset tile is selected (-1 = none)
    Tool currentTool = Tool::Pencil;
    TileImportState importState;

    // "Add" panel: create a hand-drawn tile, or import an image via the
    // native file picker.
    AddPanelStage addPanelStage = AddPanelStage::None;
    std::string addImportPath;
    int addImportTileW = 32;
    int addImportTileH = 32;
    ImportField addImportEditingField = ImportField::None;
    std::string addImportEditBuffer;

    constexpr int kCreateCanvasCells = 16;
    std::vector<sf::Color> createCanvasPixels(static_cast<size_t>(kCreateCanvasCells) * kCreateCanvasCells, sf::Color::Transparent);
    sf::Color createBrushColor = sf::Color(150, 170, 220);

    ShapeGrid shapeGrid;
    shapeGrid.resize(tileLayer.widthTiles(), tileLayer.heightTiles());
    const std::string SHAPE_LAYER_PATH = "main/assets/map/" + projectName + "/shapes.layer";

    // EditorState's own undo stack only tracks MapObjects. Tile/shape-grid
    // painting bypasses it entirely, so we keep a second, combined history
    // here: each entry is either "an object change happened" (defer to
    // EditorState::undo/redo) or a full tile+shape snapshot to restore.
    enum class HistKind { Object, Tile };
    struct HistEntry {
        HistKind kind;
        TileLayer tiles;   // only used when kind == Tile
        ShapeGrid shapes;  // only used when kind == Tile
    };
    std::vector<HistEntry> historyStack;
    std::vector<HistEntry> futureStack;

    auto markObjectChange = [&]() {
        historyStack.push_back(HistEntry{HistKind::Object, {}, {}});
        futureStack.clear();
    };
    auto beginTileStroke = [&]() {
        historyStack.push_back(HistEntry{HistKind::Tile, tileLayer, shapeGrid});
        futureStack.clear();
    };
    auto performUndo = [&]() {
        if (historyStack.empty()) return;
        HistEntry entry = historyStack.back();
        historyStack.pop_back();
        if (entry.kind == HistKind::Object) {
            futureStack.push_back(HistEntry{HistKind::Object, {}, {}});
            editor.undo();
        } else {
            futureStack.push_back(HistEntry{HistKind::Tile, tileLayer, shapeGrid});
            tileLayer = entry.tiles;
            shapeGrid = entry.shapes;
            tileLayer.rebuildVertices(tileSet);
        }
    };
    auto performRedo = [&]() {
        if (futureStack.empty()) return;
        HistEntry entry = futureStack.back();
        futureStack.pop_back();
        if (entry.kind == HistKind::Object) {
            historyStack.push_back(HistEntry{HistKind::Object, {}, {}});
            editor.redo();
        } else {
            historyStack.push_back(HistEntry{HistKind::Tile, tileLayer, shapeGrid});
            tileLayer = entry.tiles;
            shapeGrid = entry.shapes;
            tileLayer.rebuildVertices(tileSet);
        }
    };

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

    tileSet.loadMeta(TILESET_META_PATH); // restores the imported tileset, if any
    if (std::filesystem::exists(TILE_LAYER_PATH)) {
        tileLayer.load(TILE_LAYER_PATH); // restores the saved grid size + painted tiles
    }
    tileLayer.rebuildVertices(tileSet);
    if (std::filesystem::exists(SHAPE_LAYER_PATH)) {
        shapeGrid.load(SHAPE_LAYER_PATH);
    } else {
        shapeGrid.resize(tileLayer.widthTiles(), tileLayer.heightTiles());
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
        markObjectChange();
        editingField = InspectorField::None;
    };

    while (window.isOpen() && !returnToHome) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                if (exitPrompt == ExitPrompt::None) {
                    exitPrompt = ExitPrompt::ConfirmQuit;
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

            // ---- Fullscreen toggle: works regardless of any modal state ----
            if (const auto* fsKey = event->getIf<sf::Event::KeyPressed>()) {
                bool altHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt) ||
                               sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RAlt);
                if (fsKey->code == sf::Keyboard::Key::F11 ||
                    (altHeld && fsKey->code == sf::Keyboard::Key::Enter)) {
                    ToggleFullscreen(window, isFullscreenNow, windowedSize, windowedPosition);
                    WINDOW_WIDTH = window.getSize().x;
                    WINDOW_HEIGHT = window.getSize().y;
                    sf::FloatRect fsCanvasRect = ComputeCanvasRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                    float fsZoomRatio = camera.getSize().x / std::max(1.f, fsCanvasRect.size.x);
                    camera.setSize(fsCanvasRect.size);
                    camera.zoom(fsZoomRatio);
                    ApplyCanvasViewport(camera, WINDOW_WIDTH, WINDOW_HEIGHT);
                    continue;
                }
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

            // ---- "Add" panel keyboard handling ----
            if (addPanelStage != AddPanelStage::None) {
                if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                    if (addImportEditingField != ImportField::None) {
                        char32_t unicode = textEntered->unicode;
                        if (unicode == 8) {
                            if (!addImportEditBuffer.empty()) addImportEditBuffer.pop_back();
                        } else if (unicode >= '0' && unicode <= '9') {
                            addImportEditBuffer += static_cast<char>(unicode);
                        }
                    }
                    continue; // swallow text input while the Add panel is up
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        if (addImportEditingField != ImportField::None) addImportEditingField = ImportField::None;
                        else addPanelStage = AddPanelStage::None;
                    } else if (addPanelStage == AddPanelStage::ImportSize) {
                        if (keyPressed->code == sf::Keyboard::Key::Tab) {
                            addImportEditingField = (addImportEditingField == ImportField::TileW) ? ImportField::TileH : ImportField::TileW;
                            addImportEditBuffer = std::to_string(addImportEditingField == ImportField::TileW ? addImportTileW : addImportTileH);
                        } else if (keyPressed->code == sf::Keyboard::Key::Enter) {
                            if (addImportEditingField != ImportField::None) {
                                int v = ParseNumberOr(addImportEditBuffer, 32.f) > 0 ? static_cast<int>(ParseNumberOr(addImportEditBuffer, 32.f)) : 32;
                                if (addImportEditingField == ImportField::TileW) addImportTileW = v; else addImportTileH = v;
                                addImportEditingField = ImportField::None;
                            } else if (tileSet.loadFromImage(addImportPath, addImportTileW, addImportTileH)) {
                                tileSet.saveMeta(TILESET_META_PATH);
                                tileLayer.rebuildVertices(tileSet);
                                selectedTileIndex = 0;
                                paletteScrollX = 0.f;
                                editorMode = EditorMode::TilePaint;
                                addPanelStage = AddPanelStage::None;
                            }
                        }
                    }
                    continue; // swallow key presses while the Add panel is up
                }
                // any other event type (e.g. mouse clicks) falls through to the
                // mouse-handling block below, which does the actual button hit-testing
            }

            // ---- Exit / unsaved-changes modal takes priority over everything ----
            if (exitPrompt != ExitPrompt::None) {
                if (const auto* quitMousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (quitMousePressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2f qMousePos(static_cast<float>(quitMousePressed->position.x),
                                               static_cast<float>(quitMousePressed->position.y));
                        sf::FloatRect quitYesBounds({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                        sf::FloatRect quitNoBounds({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f}, {100.f, 36.f});
                        if (exitPrompt == ExitPrompt::ConfirmQuit) {
                            if (quitYesBounds.contains(qMousePos)) {
                                exitPrompt = isDirty() ? ExitPrompt::ConfirmSaveQuit : ExitPrompt::None;
                                if (exitPrompt == ExitPrompt::None) window.close();
                            } else if (quitNoBounds.contains(qMousePos)) {
                                exitPrompt = ExitPrompt::None;
                            }
                        } else if (exitPrompt == ExitPrompt::ConfirmSaveQuit) {
                            if (quitYesBounds.contains(qMousePos)) {
                                editor.saveToFile(SAVE_PATH, tileLayer.widthTiles(), tileLayer.heightTiles());
                                tileLayer.save(TILE_LAYER_PATH);
                                shapeGrid.save(SHAPE_LAYER_PATH);
                                tilesDirtySinceSave = false;
                                window.close();
                            } else if (quitNoBounds.contains(qMousePos)) {
                                window.close();
                            }
                        }
                    }
                    continue;
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    bool yes = (keyPressed->code == sf::Keyboard::Key::Y || keyPressed->code == sf::Keyboard::Key::Enter);
                    bool no = (keyPressed->code == sf::Keyboard::Key::N);
                    bool cancel = (keyPressed->code == sf::Keyboard::Key::Escape || keyPressed->code == sf::Keyboard::Key::C);

                    if (exitPrompt == ExitPrompt::ConfirmQuit) {
                        if (yes) {
                            exitPrompt = isDirty() ? ExitPrompt::ConfirmSaveQuit : ExitPrompt::None;
                            if (exitPrompt == ExitPrompt::None) window.close();
                        } else if (no || cancel) {
                            exitPrompt = ExitPrompt::None;
                        }
                    } else if (exitPrompt == ExitPrompt::ConfirmHome) {
                        if (yes) {
                            exitPrompt = isDirty() ? ExitPrompt::ConfirmSaveHome : ExitPrompt::None;
                            if (exitPrompt == ExitPrompt::None) returnToHome = true;
                        } else if (no || cancel) {
                            exitPrompt = ExitPrompt::None;
                        }
                    } else if (exitPrompt == ExitPrompt::ConfirmSaveHome) {
                        if (yes) {
                            editor.saveToFile(SAVE_PATH, tileLayer.widthTiles(), tileLayer.heightTiles());
                            tileLayer.save(TILE_LAYER_PATH);
                            shapeGrid.save(SHAPE_LAYER_PATH);
                            tilesDirtySinceSave = false;
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
                            editor.saveToFile(SAVE_PATH, tileLayer.widthTiles(), tileLayer.heightTiles());
                            tileLayer.save(TILE_LAYER_PATH);
                            shapeGrid.save(SHAPE_LAYER_PATH);
                            tilesDirtySinceSave = false;
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
                { std::ofstream dbg("debug_log.txt", std::ios::app);
                  dbg << "ANY click at (" << screenPos.x << ", " << screenPos.y
                      << ") addPanelStage=" << static_cast<int>(addPanelStage) << "\n"; }

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
                        markObjectChange();
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

                // ---- Undo / Redo button hit-test ----
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    if (ComputeUndoIconBounds().contains(screenPos)) {
                        performUndo();
                        continue;
                    }
                    if (ComputeRedoIconBounds().contains(screenPos)) {
                        performRedo();
                        continue;
                    }
                }

                // ---- Resize dialog is modal: swallow all clicks while open ----
                if (resizingMap) {
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        float cardX = kCanvasMarginSides + 40.f;
                        float cardY = 60.f;
                        float cardH = 40.f + (kResizePresetCount + (resizeConfirming ? 3 : 1)) * 34.f + 20.f;
                        sf::FloatRect cardBounds({cardX, cardY}, {480.f, cardH});
                        if (!cardBounds.contains(screenPos)) {
                            resizingMap = false;
                            resizeConfirming = false;
                            continue;
                        }
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
                            ShapeGrid resizedShapes;
                            resizedShapes.resize(resizePendingW, resizePendingH);
                            int copyW = std::min(resizePendingW, tileLayer.widthTiles());
                            int copyH = std::min(resizePendingH, tileLayer.heightTiles());
                            for (int ty = 0; ty < copyH; ++ty) {
                                for (int tx = 0; tx < copyW; ++tx) {
                                    resized.set(tx, ty, tileLayer.get(tx, ty));
                                    resizedShapes.set(tx, ty, shapeGrid.get(tx, ty));
                                }
                            }
                            tileLayer = resized;
                            shapeGrid = resizedShapes;
                            tileLayer.rebuildVertices(tileSet);
                            tileLayer.save(TILE_LAYER_PATH);
                            shapeGrid.save(SHAPE_LAYER_PATH);
                            resizingMap = false;
                            resizeConfirming = false;
                        } else if (resizeConfirming && cancelRect.contains(screenPos)) {
                            resizeConfirming = false;
                        }
                    }
                    continue;
                }

                // ---- "Add" button hit-test ----
                if (mousePressed->button == sf::Mouse::Button::Left && addPanelStage == AddPanelStage::None) {
                    if (ComputeAddButtonBounds(WINDOW_WIDTH, WINDOW_HEIGHT).contains(screenPos)) {
                        addPanelStage = AddPanelStage::ChooseAction;
                        continue;
                    }
                }

                // ---- "Add" panel is modal: swallow clicks while it's open ----
                if (addPanelStage != AddPanelStage::None) {
                    { std::ofstream dbg("debug_log.txt", std::ios::app);
                      dbg << "Add-panel click at (" << screenPos.x << ", " << screenPos.y
                          << ") stage=" << static_cast<int>(addPanelStage) << "\n"; }
                    if (addPanelStage == AddPanelStage::ChooseAction) {
                        float boxW = 360.f, boxH = 160.f;
                        float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
                        float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
                        if (!sf::FloatRect({boxX, boxY}, {boxW, boxH}).contains(screenPos)) {
                            addPanelStage = AddPanelStage::None;
                            continue;
                        }
                        sf::FloatRect createBtn({boxX + 30.f, boxY + 70.f}, {140.f, 60.f});
                        sf::FloatRect importBtn2({boxX + boxW - 170.f, boxY + 70.f}, {140.f, 60.f});
                        if (createBtn.contains(screenPos)) {
                            addPanelStage = AddPanelStage::Create;
                            std::fill(createCanvasPixels.begin(), createCanvasPixels.end(), sf::Color::Transparent);
                        } else if (importBtn2.contains(screenPos)) {
                            std::string picked = OpenFileDialogForImage();
                            if (!picked.empty()) {
                                addImportPath = picked;
                                addPanelStage = AddPanelStage::ImportSize;
                            } else {
                                addPanelStage = AddPanelStage::None;
                            }
                        } else {
                            addPanelStage = AddPanelStage::None;
                        }
                    } else if (addPanelStage == AddPanelStage::ImportSize) {
                        float boxW = 360.f, boxH = 180.f;
                        float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
                        float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
                        if (!sf::FloatRect({boxX, boxY}, {boxW, boxH}).contains(screenPos)) {
                            addPanelStage = AddPanelStage::None;
                            continue;
                        }
                        sf::FloatRect wRow({boxX + 30.f, boxY + 60.f}, {boxW - 60.f, 28.f});
                        sf::FloatRect hRow({boxX + 30.f, boxY + 96.f}, {boxW - 60.f, 28.f});
                        sf::FloatRect confirmBtn({boxX + 30.f, boxY + boxH - 46.f}, {140.f, 32.f});
                        sf::FloatRect cancelBtn({boxX + boxW - 170.f, boxY + boxH - 46.f}, {140.f, 32.f});
                        if (wRow.contains(screenPos)) {
                            addImportEditingField = ImportField::TileW;
                            addImportEditBuffer = std::to_string(addImportTileW);
                        } else if (hRow.contains(screenPos)) {
                            addImportEditingField = ImportField::TileH;
                            addImportEditBuffer = std::to_string(addImportTileH);
                        } else if (confirmBtn.contains(screenPos)) {
                            if (tileSet.loadFromImage(addImportPath, addImportTileW, addImportTileH)) {
                                tileSet.saveMeta(TILESET_META_PATH);
                                tileLayer.rebuildVertices(tileSet);
                                selectedTileIndex = 0;
                                paletteScrollX = 0.f;
                                editorMode = EditorMode::TilePaint;
                            }
                            addPanelStage = AddPanelStage::None;
                        } else if (cancelBtn.contains(screenPos)) {
                            addPanelStage = AddPanelStage::None;
                        }
                    } else if (addPanelStage == AddPanelStage::Create) {
                        float boxW = 380.f, boxH = 460.f;
                        float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
                        float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
                        if (!sf::FloatRect({boxX, boxY}, {boxW, boxH}).contains(screenPos)) {
                            addPanelStage = AddPanelStage::None;
                            continue;
                        }
                        float gridX = boxX + 30.f, gridY = boxY + 60.f;
                        float cellSize = 20.f;
                        static const sf::Color kSwatches[6] = {
                            sf::Color::White, sf::Color::Black, sf::Color(220, 60, 60),
                            sf::Color(60, 180, 90), sf::Color(70, 120, 220), sf::Color(230, 200, 60)
                        };
                        sf::FloatRect gridBounds({gridX, gridY}, {cellSize * kCreateCanvasCells, cellSize * kCreateCanvasCells});
                        if (gridBounds.contains(screenPos)) {
                            int cx = static_cast<int>((screenPos.x - gridX) / cellSize);
                            int cy = static_cast<int>((screenPos.y - gridY) / cellSize);
                            if (cx >= 0 && cx < kCreateCanvasCells && cy >= 0 && cy < kCreateCanvasCells) {
                                createCanvasPixels[static_cast<size_t>(cy) * kCreateCanvasCells + cx] =
                                    (mousePressed->button == sf::Mouse::Button::Right) ? sf::Color::Transparent : createBrushColor;
                            }
                        } else {
                            float swatchY = gridY + cellSize * kCreateCanvasCells + 16.f;
                            for (int i = 0; i < 6; ++i) {
                                sf::FloatRect sw({gridX + i * 36.f, swatchY}, {28.f, 28.f});
                                if (sw.contains(screenPos)) createBrushColor = kSwatches[i];
                            }
                            sf::FloatRect saveBtn({boxX + 30.f, boxY + boxH - 46.f}, {140.f, 32.f});
                            sf::FloatRect cancelBtn({boxX + boxW - 170.f, boxY + boxH - 46.f}, {140.f, 32.f});
                            if (saveBtn.contains(screenPos)) {
                                sf::Image img;
                                img.resize({static_cast<unsigned int>(kCreateCanvasCells), static_cast<unsigned int>(kCreateCanvasCells)});
                                for (int y = 0; y < kCreateCanvasCells; ++y)
                                    for (int x = 0; x < kCreateCanvasCells; ++x)
                                        img.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                                                      createCanvasPixels[static_cast<size_t>(y) * kCreateCanvasCells + x]);
                                // TODO: hand `img` to TileSet once it exposes a way to add
                                // a tile from an in-memory image (needs TileSet.h to wire up).
                                addPanelStage = AddPanelStage::None;
                            } else if (cancelBtn.contains(screenPos)) {
                                addPanelStage = AddPanelStage::None;
                            }
                        }
                    }
                    continue;
                }

                // ---- Bottom palette hit-test: pick which tile/shape piece paints next ----
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    sf::FloatRect paletteRect = ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT);
                    if (paletteRect.contains(screenPos)) {
                        if (!tileSet.loaded || tileSet.tileCount() <= 0) {
                            float x = paletteRect.position.x + kPalettePad;
                            float y = paletteRect.position.y + 28.f;
                            for (int i = 0; i < kBasicShapeCount; ++i) {
                                sf::FloatRect slotBounds({x, y}, {kPaletteItemSize, kPaletteItemSize});
                                if (slotBounds.contains(screenPos)) {
                                    selectedShapeId = i;
                                    currentTool = Tool::Pencil;
                                    break;
                                }
                                x += kPaletteItemSize + kPalettePad;
                            }
                        } else {
                            for (const auto& item : ComputePaletteLayout(tileSet, paletteRect, paletteScrollX)) {
                                if (item.bounds.contains(screenPos)) {
                                    selectedTileIndex = item.tileIndex;
                                    selectedShapeId = -1; // a tileset tile is now active instead of a shape
                                    currentTool = Tool::Pencil;
                                    break;
                                }
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
                    if (currentTool == Tool::Pencil || currentTool == Tool::Eraser || currentTool == Tool::Fill) {
                        beginTileStroke();
                    }
                    if (currentTool == Tool::Pencil) {
                        if (selectedShapeId >= 0) {
                            shapeGrid.set(cell.x, cell.y, selectedShapeId);
                            tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                        } else {
                            tileLayer.set(cell.x, cell.y, selectedTileIndex);
                            shapeGrid.set(cell.x, cell.y, -1);
                        }
                    }
                    else if (currentTool == Tool::Eraser) {
                        tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                        shapeGrid.set(cell.x, cell.y, -1);
                    }
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
                    beginTileStroke();
                    tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                    shapeGrid.set(cell.x, cell.y, -1);
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
                            markObjectChange();
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
                        markObjectChange();
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
                            markObjectChange();
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
                        if (currentTool == Tool::Pencil) {
                            if (selectedShapeId >= 0) {
                                shapeGrid.set(cell.x, cell.y, selectedShapeId);
                                tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                            } else {
                                tileLayer.set(cell.x, cell.y, selectedTileIndex);
                                shapeGrid.set(cell.x, cell.y, -1);
                            }
                        }
                        else if (currentTool == Tool::Eraser) {
                            tileLayer.set(cell.x, cell.y, TileLayer::kEmpty);
                            shapeGrid.set(cell.x, cell.y, -1);
                        }
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
                    shapeGrid.save(SHAPE_LAYER_PATH);
                    tilesDirtySinceSave = false;
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
                        exitPrompt = ExitPrompt::ConfirmQuit;
                    }
                }
                else if (keyPressed->code == sf::Keyboard::Key::Delete ||
                         keyPressed->code == sf::Keyboard::Key::Backspace) {
                    editor.deleteSelected();
                    markObjectChange();
                }
                else if (keyPressed->code == sf::Keyboard::Key::R) {
                    editor.rotateSelectedBy(15.f);
                    markObjectChange();
                }
                else if (keyPressed->code == sf::Keyboard::Key::LBracket) {
                    editor.resizeSelectedBy(-5.f);
                    markObjectChange();
                }
                else if (keyPressed->code == sf::Keyboard::Key::RBracket) {
                    editor.resizeSelectedBy(5.f);
                    markObjectChange();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::S) {
                    editor.saveToFile(SAVE_PATH, tileLayer.widthTiles(), tileLayer.heightTiles());
                    tileLayer.save(TILE_LAYER_PATH);
                    shapeGrid.save(SHAPE_LAYER_PATH);
                    tilesDirtySinceSave = false;
                    savedUndoDepth = editor.undoDepth();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::O) {
                    editor.loadFromFile(SAVE_PATH);
                    tileLayer.load(TILE_LAYER_PATH);
                    tileLayer.rebuildVertices(tileSet);
                    shapeGrid.load(SHAPE_LAYER_PATH);
                    savedUndoDepth = editor.undoDepth();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::Z) {
                    performUndo();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::Y) {
                    performRedo();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::C) {
                    editor.copySelected();
                }
                else if (ctrlHeld && keyPressed->code == sf::Keyboard::Key::V) {
                    editor.paste(GRID_SIZE, GRID_SIZE);
                    markObjectChange();
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

        if (tileLayer.dirty) {
            tilesDirtySinceSave = true; // tile/shape paint happened; rebuildVertices() below clears the transient flag, not this one
            tileLayer.rebuildVertices(tileSet);
        }
        tileLayer.draw(window, tileSet); // background art, drawn beneath objects
        DrawShapeGrid(window, camera, tileLayer, shapeGrid); // placed Square/Circle/Triangle/Slope pieces
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

        // Undo button
        {
            sf::FloatRect undoBounds = ComputeUndoIconBounds();
            sf::RectangleShape undoBg(undoBounds.size);
            undoBg.setPosition(undoBounds.position);
            undoBg.setFillColor(sf::Color(45, 45, 52));
            undoBg.setOutlineThickness(1.f);
            undoBg.setOutlineColor(sf::Color(80, 80, 90));
            window.draw(undoBg);
            sf::Text undoText(uiFont, "<", 16);
            undoText.setFillColor(sf::Color(200, 200, 210));
            undoText.setPosition({undoBounds.position.x + 6.f, undoBounds.position.y - 1.f});
            window.draw(undoText);
        }

        // Redo button
        {
            sf::FloatRect redoBounds = ComputeRedoIconBounds();
            sf::RectangleShape redoBg(redoBounds.size);
            redoBg.setPosition(redoBounds.position);
            redoBg.setFillColor(sf::Color(45, 45, 52));
            redoBg.setOutlineThickness(1.f);
            redoBg.setOutlineColor(sf::Color(80, 80, 90));
            window.draw(redoBg);
            sf::Text redoText(uiFont, ">", 16);
            redoText.setFillColor(sf::Color(200, 200, 210));
            redoText.setPosition({redoBounds.position.x + 6.f, redoBounds.position.y - 1.f});
            window.draw(redoText);
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

        // Draw toolbar unconditionally so it is always visible
        DrawToolbar(window, uiFont, currentTool, texPencil, texEraser, texFill, texMove, texPointer);

        // Bottom tile-piece palette (pieces from the imported tileset: Box/Circle/Triangle/etc.)
        DrawPalette(window, uiFont, tileSet, ComputePaletteRect(WINDOW_WIDTH, WINDOW_HEIGHT), paletteScrollX, selectedTileIndex, selectedShapeId, currentTool);

        // ---- "Add" button ----
        {
            sf::FloatRect addBounds = ComputeAddButtonBounds(WINDOW_WIDTH, WINDOW_HEIGHT);
            sf::RectangleShape addBg(addBounds.size);
            addBg.setPosition(addBounds.position);
            addBg.setFillColor(sf::Color(45, 45, 52));
            addBg.setOutlineThickness(1.f);
            addBg.setOutlineColor(sf::Color(80, 80, 90));
            window.draw(addBg);
            if (addIconLoaded) {
                sf::Sprite addSprite(texAddIcon);
                sf::Vector2u texSize = texAddIcon.getSize();
                addSprite.setScale({addBounds.size.x / std::max(1u, texSize.x), addBounds.size.y / std::max(1u, texSize.y)});
                addSprite.setPosition(addBounds.position);
                window.draw(addSprite);
            } else {
                sf::Text addFallback(uiFont, "+", 18);
                addFallback.setFillColor(sf::Color(200, 200, 210));
                addFallback.setPosition({addBounds.position.x + 6.f, addBounds.position.y - 2.f});
                window.draw(addFallback);
            }
        }

        if (addPanelStage == AddPanelStage::ChooseAction) {
            sf::RectangleShape dim({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(dim);

            float boxW = 360.f, boxH = 160.f;
            float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
            float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
            sf::RectangleShape box({boxW, boxH});
            box.setPosition({boxX, boxY});
            box.setFillColor(sf::Color(38, 38, 44));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(sf::Color(90, 90, 240));
            window.draw(box);

            sf::Text title(uiFont, "Add a new tile", 18);
            title.setFillColor(sf::Color::White);
            title.setPosition({boxX + boxW / 2.f - title.getLocalBounds().size.x / 2.f, boxY + 16.f});
            window.draw(title);

            sf::FloatRect createBtn({boxX + 30.f, boxY + 70.f}, {140.f, 60.f});
            sf::FloatRect importBtn2({boxX + boxW - 170.f, boxY + 70.f}, {140.f, 60.f});

            sf::RectangleShape createBg(createBtn.size);
            createBg.setPosition(createBtn.position);
            createBg.setFillColor(sf::Color(50, 50, 60));
            createBg.setOutlineThickness(1.f);
            createBg.setOutlineColor(sf::Color(90, 90, 100));
            window.draw(createBg);
            if (addIconLoaded) {
                sf::Sprite s(texAddIcon);
                sf::Vector2u ts = texAddIcon.getSize();
                s.setScale({28.f / std::max(1u, ts.x), 28.f / std::max(1u, ts.y)});
                s.setPosition({createBtn.position.x + 10.f, createBtn.position.y + 6.f});
                window.draw(s);
            }
            sf::Text createLabel(uiFont, "Create", 14);
            createLabel.setFillColor(sf::Color::White);
            createLabel.setPosition({createBtn.position.x + 10.f, createBtn.position.y + 38.f});
            window.draw(createLabel);

            sf::RectangleShape importBg(importBtn2.size);
            importBg.setPosition(importBtn2.position);
            importBg.setFillColor(sf::Color(50, 50, 60));
            importBg.setOutlineThickness(1.f);
            importBg.setOutlineColor(sf::Color(90, 90, 100));
            window.draw(importBg);
            if (importIconLoaded) {
                sf::Sprite s(texImportIcon);
                sf::Vector2u ts = texImportIcon.getSize();
                s.setScale({28.f / std::max(1u, ts.x), 28.f / std::max(1u, ts.y)});
                s.setPosition({importBtn2.position.x + 10.f, importBtn2.position.y + 6.f});
                window.draw(s);
            }
            sf::Text importLabel(uiFont, "Import", 14);
            importLabel.setFillColor(sf::Color::White);
            importLabel.setPosition({importBtn2.position.x + 10.f, importBtn2.position.y + 38.f});
            window.draw(importLabel);
        }
        else if (addPanelStage == AddPanelStage::ImportSize) {
            sf::RectangleShape dim({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(dim);

            float boxW = 360.f, boxH = 180.f;
            float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
            float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
            sf::RectangleShape box({boxW, boxH});
            box.setPosition({boxX, boxY});
            box.setFillColor(sf::Color(38, 38, 44));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(sf::Color(90, 90, 240));
            window.draw(box);

            sf::Text title(uiFont, "Tile size ([Tab] switch, [Enter] confirm)", 15);
            title.setFillColor(sf::Color::White);
            title.setPosition({boxX + boxW / 2.f - title.getLocalBounds().size.x / 2.f, boxY + 14.f});
            window.draw(title);

            sf::FloatRect wRow({boxX + 30.f, boxY + 60.f}, {boxW - 60.f, 28.f});
            sf::FloatRect hRow({boxX + 30.f, boxY + 96.f}, {boxW - 60.f, 28.f});
            DrawInspectorRow(window, uiFont, wRow, "Tile W",
                              addImportEditingField == ImportField::TileW ? addImportEditBuffer : std::to_string(addImportTileW),
                              addImportEditingField == ImportField::TileW);
            DrawInspectorRow(window, uiFont, hRow, "Tile H",
                              addImportEditingField == ImportField::TileH ? addImportEditBuffer : std::to_string(addImportTileH),
                              addImportEditingField == ImportField::TileH);

            sf::FloatRect confirmBtn({boxX + 30.f, boxY + boxH - 46.f}, {140.f, 32.f});
            sf::FloatRect cancelBtn({boxX + boxW - 170.f, boxY + boxH - 46.f}, {140.f, 32.f});
            sf::RectangleShape confirmBg(confirmBtn.size);
            confirmBg.setPosition(confirmBtn.position);
            confirmBg.setFillColor(sf::Color(60, 120, 70));
            window.draw(confirmBg);
            sf::Text confirmText(uiFont, "Confirm", 14);
            confirmText.setPosition({confirmBtn.position.x + 30.f, confirmBtn.position.y + 8.f});
            window.draw(confirmText);

            sf::RectangleShape cancelBg(cancelBtn.size);
            cancelBg.setPosition(cancelBtn.position);
            cancelBg.setFillColor(sf::Color(120, 60, 60));
            window.draw(cancelBg);
            sf::Text cancelText(uiFont, "Cancel", 14);
            cancelText.setPosition({cancelBtn.position.x + 36.f, cancelBtn.position.y + 8.f});
            window.draw(cancelText);
        }
        else if (addPanelStage == AddPanelStage::Create) {
            sf::RectangleShape dim({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(dim);

            float boxW = 380.f, boxH = 460.f;
            float boxX = WINDOW_WIDTH / 2.f - boxW / 2.f;
            float boxY = WINDOW_HEIGHT / 2.f - boxH / 2.f;
            sf::RectangleShape box({boxW, boxH});
            box.setPosition({boxX, boxY});
            box.setFillColor(sf::Color(38, 38, 44));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(sf::Color(90, 90, 240));
            window.draw(box);

            sf::Text title(uiFont, "Draw your tile (Right-click erases)", 15);
            title.setFillColor(sf::Color::White);
            title.setPosition({boxX + boxW / 2.f - title.getLocalBounds().size.x / 2.f, boxY + 12.f});
            window.draw(title);

            float gridX = boxX + 30.f, gridY = boxY + 60.f;
            float cellSize = 20.f;
            for (int y = 0; y < kCreateCanvasCells; ++y) {
                for (int x = 0; x < kCreateCanvasCells; ++x) {
                    sf::RectangleShape cell({cellSize - 1.f, cellSize - 1.f});
                    cell.setPosition({gridX + x * cellSize, gridY + y * cellSize});
                    sf::Color px = createCanvasPixels[static_cast<size_t>(y) * kCreateCanvasCells + x];
                    cell.setFillColor(px.a == 0 ? sf::Color(30, 30, 34) : px);
                    window.draw(cell);
                }
            }

            static const sf::Color kSwatches[6] = {
                sf::Color::White, sf::Color::Black, sf::Color(220, 60, 60),
                sf::Color(60, 180, 90), sf::Color(70, 120, 220), sf::Color(230, 200, 60)
            };
            float swatchY = gridY + cellSize * kCreateCanvasCells + 16.f;
            for (int i = 0; i < 6; ++i) {
                sf::RectangleShape sw({28.f, 28.f});
                sw.setPosition({gridX + i * 36.f, swatchY});
                sw.setFillColor(kSwatches[i]);
                sw.setOutlineThickness(kSwatches[i] == createBrushColor ? 3.f : 1.f);
                sw.setOutlineColor(kSwatches[i] == createBrushColor ? sf::Color(120, 170, 255) : sf::Color(90, 90, 100));
                window.draw(sw);
            }

            sf::FloatRect saveBtn({boxX + 30.f, boxY + boxH - 46.f}, {140.f, 32.f});
            sf::FloatRect cancelBtn({boxX + boxW - 170.f, boxY + boxH - 46.f}, {140.f, 32.f});
            sf::RectangleShape saveBg(saveBtn.size);
            saveBg.setPosition(saveBtn.position);
            saveBg.setFillColor(sf::Color(60, 120, 70));
            window.draw(saveBg);
            sf::Text saveText(uiFont, "Save", 14);
            saveText.setPosition({saveBtn.position.x + 46.f, saveBtn.position.y + 8.f});
            window.draw(saveText);

            sf::RectangleShape cancelBg2(cancelBtn.size);
            cancelBg2.setPosition(cancelBtn.position);
            cancelBg2.setFillColor(sf::Color(120, 60, 60));
            window.draw(cancelBg2);
            sf::Text cancelText2(uiFont, "Cancel", 14);
            cancelText2.setPosition({cancelBtn.position.x + 36.f, cancelBtn.position.y + 8.f});
            window.draw(cancelText2);
        }

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

        if (exitPrompt == ExitPrompt::ConfirmQuit) {
            sf::RectangleShape quitOverlay({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            quitOverlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(quitOverlay);

            sf::RectangleShape quitDialog({340.f, 130.f});
            quitDialog.setPosition({WINDOW_WIDTH / 2.f - 170.f, WINDOW_HEIGHT / 2.f - 60.f});
            quitDialog.setFillColor(sf::Color(38, 38, 44));
            quitDialog.setOutlineThickness(2.f);
            quitDialog.setOutlineColor(sf::Color(230, 90, 90));
            window.draw(quitDialog);

            sf::Text quitQuestion(uiFont, "Are you sure?", 22);
            quitQuestion.setPosition({WINDOW_WIDTH / 2.f - quitQuestion.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f - 40.f});
            quitQuestion.setFillColor(sf::Color::White);
            window.draw(quitQuestion);

            sf::RectangleShape quitYesBtn({100.f, 36.f});
            quitYesBtn.setPosition({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f});
            quitYesBtn.setFillColor(sf::Color(230, 90, 90));
            window.draw(quitYesBtn);
            sf::Text quitYesLabel(uiFont, "Quit", 18);
            quitYesLabel.setFillColor(sf::Color::White);
            quitYesLabel.setPosition({WINDOW_WIDTH / 2.f - 110.f + 28.f, WINDOW_HEIGHT / 2.f + 26.f});
            window.draw(quitYesLabel);

            sf::RectangleShape quitNoBtn({100.f, 36.f});
            quitNoBtn.setPosition({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f});
            quitNoBtn.setFillColor(sf::Color(70, 70, 78));
            window.draw(quitNoBtn);
            sf::Text quitNoLabel(uiFont, "Cancel", 18);
            quitNoLabel.setFillColor(sf::Color::White);
            quitNoLabel.setPosition({WINDOW_WIDTH / 2.f + 10.f + 20.f, WINDOW_HEIGHT / 2.f + 26.f});
            window.draw(quitNoLabel);
        } else if (exitPrompt == ExitPrompt::ConfirmHome) {
            DrawConfirmModal(window, uiFont, WINDOW_WIDTH, WINDOW_HEIGHT,
                              "Return to Home page?", "Are you sure? [Y]es  /  [N]o");
        } else if (exitPrompt == ExitPrompt::ConfirmSaveHome) {
            DrawConfirmModal(window, uiFont, WINDOW_WIDTH, WINDOW_HEIGHT,
                              "You have unsaved changes!", "Save before leaving?  [Y]es  /  [N]o  /  [Esc] Cancel");
        } else if (exitPrompt == ExitPrompt::ConfirmSaveQuit) {
            sf::RectangleShape saveOverlay({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            saveOverlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(saveOverlay);

            sf::RectangleShape saveDialog({340.f, 130.f});
            saveDialog.setPosition({WINDOW_WIDTH / 2.f - 170.f, WINDOW_HEIGHT / 2.f - 60.f});
            saveDialog.setFillColor(sf::Color(38, 38, 44));
            saveDialog.setOutlineThickness(2.f);
            saveDialog.setOutlineColor(sf::Color(230, 90, 90));
            window.draw(saveDialog);

            sf::Text saveQuestion(uiFont, "Save last change?", 22);
            saveQuestion.setPosition({WINDOW_WIDTH / 2.f - saveQuestion.getLocalBounds().size.x / 2.f, WINDOW_HEIGHT / 2.f - 40.f});
            saveQuestion.setFillColor(sf::Color::White);
            window.draw(saveQuestion);

            sf::RectangleShape saveYesBtn({100.f, 36.f});
            saveYesBtn.setPosition({WINDOW_WIDTH / 2.f - 110.f, WINDOW_HEIGHT / 2.f + 20.f});
            saveYesBtn.setFillColor(sf::Color(90, 160, 90));
            window.draw(saveYesBtn);
            sf::Text saveYesLabel(uiFont, "Yes", 18);
            saveYesLabel.setFillColor(sf::Color::White);
            saveYesLabel.setPosition({WINDOW_WIDTH / 2.f - 110.f + 34.f, WINDOW_HEIGHT / 2.f + 26.f});
            window.draw(saveYesLabel);

            sf::RectangleShape saveNoBtn({100.f, 36.f});
            saveNoBtn.setPosition({WINDOW_WIDTH / 2.f + 10.f, WINDOW_HEIGHT / 2.f + 20.f});
            saveNoBtn.setFillColor(sf::Color(70, 70, 78));
            window.draw(saveNoBtn);
            sf::Text saveNoLabel(uiFont, "No", 18);
            saveNoLabel.setFillColor(sf::Color::White);
            saveNoLabel.setPosition({WINDOW_WIDTH / 2.f + 10.f + 38.f, WINDOW_HEIGHT / 2.f + 26.f});
            window.draw(saveNoLabel);
        }

        window.display();
    }

    return !returnToHome;
}
