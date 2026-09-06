// ==========================================================
//  TileLayer.h - a grid of tile cells painted from a project's
//  list of TileSets. Kept separate from MapObject/EditorState:
//  tiles are level art (background terrain), MapObjects stay
//  gameplay entities. The editor draws the tile layer first,
//  then MapObjects on top.
// ==========================================================
#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileSet.h"

// Which tileset (by index into the project's tileset list) and which tile
// within it a cell is painted with. -1/-1 means empty.
struct TileCell {
    int tilesetIndex = -1;
    int tileIndex = -1;
    bool empty() const { return tilesetIndex < 0 || tileIndex < 0; }
};

class TileLayer {
public:
    static constexpr int kEmpty = -1;

    TileLayer(int widthTiles = 200, int heightTiles = 120, float tileSize = 32.f);

    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    float tileSize() const { return tileSize_; }

    TileCell get(int tx, int ty) const;
    void set(int tx, int ty, int tilesetIndex, int tileIndex); // pass kEmpty, kEmpty to erase

    // World-space (pixels) -> tile coords, for turning a mouse click into a cell.
    sf::Vector2i worldToTile(sf::Vector2f world) const;

    // `tilesets` is the project's full tileset list; each cell's tilesetIndex
    // indexes into it. Rebuilds one vertex array per tileset (they can't
    // share a draw call since each uses a different texture).
    void rebuildVertices(const std::vector<TileSet>& tilesets);
    void draw(sf::RenderWindow& window, const std::vector<TileSet>& tilesets) const;

    // Text serialization: header line "W H TILESIZE", then W*H
    // space-separated "tilesetIndex,tileIndex" pairs. Also transparently
    // reads the old pre-multi-tileset format (bare indices, no comma),
    // treating them as belonging to tileset 0.
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    bool dirty = false; // caller sets true after set(); rebuildVertices() clears it

private:
    int width_, height_;
    float tileSize_;
    std::vector<TileCell> tiles_;
    std::vector<sf::VertexArray> vertexArraysByTileset_;
};
