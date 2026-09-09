// Import.cpp
// Tileset-import related types, split out of zero_editor_session.cpp.

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

enum class ImportField { None, TileW, TileH, Name };
enum class ImportMode { GridSize, CustomCut };

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

    // Custom cut mode: user drags a rectangle over the preview to define
    // a single tile of any size instead of an even W x H grid.
    ImportMode mode = ImportMode::GridSize;
    bool customCutDragging = false;
    sf::Vector2f customCutStart;   // drag start, in preview-image pixel space
    sf::Vector2f customCutEnd;     // drag end, in preview-image pixel space
    std::vector<sf::IntRect> customCuts; // confirmed custom-cut tiles for this image
};
