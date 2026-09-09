#include "TileLayer.h"
#include <fstream>
#include <cmath>
#include <cstdlib>

TileLayer::TileLayer(int widthTiles, int heightTiles, float tileSize)
    : width_(widthTiles), height_(heightTiles), tileSize_(tileSize) {
    tiles_.assign(static_cast<size_t>(width_) * height_, TileCell{});
}

TileCell TileLayer::get(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return TileCell{};
    return tiles_[static_cast<size_t>(ty) * width_ + tx];
}

void TileLayer::set(int tx, int ty, int tilesetIndex, int tileIndex) {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return;
    tiles_[static_cast<size_t>(ty) * width_ + tx] = TileCell{tilesetIndex, tileIndex};
    dirty = true;
}

sf::Vector2i TileLayer::worldToTile(sf::Vector2f world) const {
    return sf::Vector2i(static_cast<int>(std::floor(world.x / tileSize_)),
                         static_cast<int>(std::floor(world.y / tileSize_)));
}

void TileLayer::rebuildVertices(const std::vector<TileSet>& tilesets) {
    vertexArraysByTileset_.assign(tilesets.size(), sf::VertexArray(sf::PrimitiveType::Triangles));

    for (int ty = 0; ty < height_; ++ty) {
        for (int tx = 0; tx < width_; ++tx) {
            const TileCell& cell = tiles_[static_cast<size_t>(ty) * width_ + tx];
            if (cell.empty()) continue;
            if (cell.tilesetIndex < 0 || cell.tilesetIndex >= static_cast<int>(tilesets.size())) continue;
            const TileSet& ts = tilesets[cell.tilesetIndex];
            if (!ts.loaded || cell.tileIndex < 0 || cell.tileIndex >= ts.tileCount()) continue;

            float px = tx * tileSize_;
            float py = ty * tileSize_;
            sf::IntRect r = ts.tileRect(cell.tileIndex);
            float u0 = static_cast<float>(r.position.x);
            float v0 = static_cast<float>(r.position.y);
            float u1 = static_cast<float>(r.position.x + r.size.x);
            float v1 = static_cast<float>(r.position.y + r.size.y);

            sf::Vertex v[6];
            v[0].position = {px, py};                         v[0].texCoords = {u0, v0};
            v[1].position = {px + tileSize_, py};              v[1].texCoords = {u1, v0};
            v[2].position = {px + tileSize_, py + tileSize_};  v[2].texCoords = {u1, v1};
            v[3].position = {px, py};                          v[3].texCoords = {u0, v0};
            v[4].position = {px + tileSize_, py + tileSize_};  v[4].texCoords = {u1, v1};
            v[5].position = {px, py + tileSize_};              v[5].texCoords = {u0, v1};
            for (auto& vert : v) vertexArraysByTileset_[cell.tilesetIndex].append(vert);
        }
    }
    dirty = false;
}

void TileLayer::draw(sf::RenderWindow& window, const std::vector<TileSet>& tilesets) const {
    for (size_t i = 0; i < vertexArraysByTileset_.size() && i < tilesets.size(); ++i) {
        if (!tilesets[i].loaded || vertexArraysByTileset_[i].getVertexCount() == 0) continue;
        sf::RenderStates states;
        states.texture = &tilesets[i].texture;
        window.draw(vertexArraysByTileset_[i], states);
    }
}

bool TileLayer::save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << width_ << " " << height_ << " " << tileSize_ << "\n";
    for (int ty = 0; ty < height_; ++ty) {
        for (int tx = 0; tx < width_; ++tx) {
            const TileCell& cell = tiles_[static_cast<size_t>(ty) * width_ + tx];
            out << cell.tilesetIndex << "," << cell.tileIndex;
            if (tx + 1 < width_) out << " ";
        }
        out << "\n";
    }
    return true;
}

bool TileLayer::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    int w = 0, h = 0; float ts = 0.f;
    in >> w >> h >> ts;
    if (w <= 0 || h <= 0 || ts <= 0.f) return false;
    std::vector<TileCell> loadedTiles(static_cast<size_t>(w) * h, TileCell{});
    for (auto& cell : loadedTiles) {
        std::string token;
        if (!(in >> token)) return false;
        size_t comma = token.find(',');
        if (comma == std::string::npos) {
            // Legacy pre-multi-tileset format: a bare index into tileset 0.
            int idx = std::atoi(token.c_str());
            cell.tilesetIndex = (idx < 0) ? -1 : 0;
            cell.tileIndex = idx;
        } else {
            cell.tilesetIndex = std::atoi(token.substr(0, comma).c_str());
            cell.tileIndex = std::atoi(token.substr(comma + 1).c_str());
        }
    }
    width_ = w; height_ = h; tileSize_ = ts;
    tiles_ = std::move(loadedTiles);
    dirty = true;
    return true;
}
