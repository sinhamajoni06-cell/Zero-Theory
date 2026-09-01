// ==========================================================
//  TileLayer.h - a grid of tile indices painted from a TileSet.
//  Kept separate from MapObject/EditorState: tiles are level art
//  (background terrain), MapObjects stay gameplay entities. The
//  editor draws the tile layer first, then MapObjects on top.
// ==========================================================
#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileSet.h"

class TileLayer {
public:
    static constexpr int kEmpty = -1;

    TileLayer(int widthTiles = 200, int heightTiles = 120, float tileSize = 32.f);

    int widthTiles() const { return width_; }
    int heightTiles() const { return height_; }
    float tileSize() const { return tileSize_; }

    int get(int tx, int ty) const;
    void set(int tx, int ty, int tileIndex); // pass kEmpty to erase

    // World-space (pixels) -> tile coords, for turning a mouse click into a cell.
    sf::Vector2i worldToTile(sf::Vector2f world) const;

    // Rebuilds the internal vertex array; call once after any set() calls
    // for the frame (batched, not per-tile) for good performance on large maps.
    void rebuildVertices(const TileSet& tileset);
    void draw(sf::RenderWindow& window, const TileSet& tileset) const;

    // Simple text serialization: header line "W H TILESIZE", then W*H
    // space-separated indices. Independent of your Json.h.
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    bool dirty = false; // caller sets true after set(); rebuildVertices() clears it

private:
    int width_, height_;
    float tileSize_;
    std::vector<int> tiles_;
    sf::VertexArray vertices_{sf::PrimitiveType::Triangles};
};