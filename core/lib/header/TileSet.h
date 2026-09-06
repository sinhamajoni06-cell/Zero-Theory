// ==========================================================
//  TileSet.h - an imported image sliced into a grid of tiles.
//  Independent of MapObject/EditorState so it can be reused by
//  the runtime game later.
// ==========================================================
#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

struct TileSet {
    sf::Texture texture;
    std::string imagePath;     // path the source image was loaded from
    std::string name;          // display name / folder name; not persisted by saveMeta (the folder name IS the name)
    int tileWidth  = 32;
    int tileHeight = 32;
    int columns = 0;
    int rows    = 0;
    bool loaded = false;

    // Slices `path` into a grid of tileW x tileH tiles (grid computed from
    // image size, no padding/spacing support yet).
    bool loadFromImage(const std::string& path, int tileW, int tileH);

    int tileCount() const { return columns * rows; }

    // Pixel-space rect of tile `index` within the source texture.
    sf::IntRect tileRect(int index) const;

    // Lightweight text metadata so a project remembers which image + tile
    // size it used, without needing your Json.h.
    //   line1: imagePath
    //   line2: tileWidth tileHeight
    bool saveMeta(const std::string& path) const;
    bool loadMeta(const std::string& path); // also reloads the texture
};

// Lists image files (.png/.jpg/.jpeg/.bmp) directly inside `folder`, for the
// import picker. Returns filenames only (not full paths), sorted.
std::vector<std::string> ListImportableImages(const std::string& folder);
