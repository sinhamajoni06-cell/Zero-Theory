#include "TileLayer.h"
#include <fstream>
#include <cmath>

TileLayer::TileLayer(int widthTiles, int heightTiles, float tileSize)
    : width_(widthTiles), height_(heightTiles), tileSize_(tileSize) {
    tiles_.assign(static_cast<size_t>(width_) * height_, kEmpty);
}

int TileLayer::get(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return kEmpty;
    return tiles_[static_cast<size_t>(ty) * width_ + tx];
}

void TileLayer::set(int tx, int ty, int tileIndex) {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return;
    tiles_[static_cast<size_t>(ty) * width_ + tx] = tileIndex;
    dirty = true;
}

sf::Vector2i TileLayer::worldToTile(sf::Vector2f world) const {
    return sf::Vector2i(static_cast<int>(std::floor(world.x / tileSize_)),
                         static_cast<int>(std::floor(world.y / tileSize_)));
}

void TileLayer::rebuildVertices(const TileSet& tileset) {
    vertices_.clear();
    if (!tileset.loaded) { dirty = false; return; }

    sf::Vector2u texSize = tileset.texture.getSize();
    for (int ty = 0; ty < height_; ++ty) {
        for (int tx = 0; tx < width_; ++tx) {
            int idx = tiles_[static_cast<size_t>(ty) * width_ + tx];
            if (idx < 0 || idx >= tileset.tileCount()) continue;

            float px = tx * tileSize_;
            float py = ty * tileSize_;
            sf::IntRect r = tileset.tileRect(idx);
            float u0 = static_cast<float>(r.position.x);
            float v0 = static_cast<float>(r.position.y);
            float u1 = static_cast<float>(r.position.x + r.size.x);
            float v1 = static_cast<float>(r.position.y + r.size.y);
            (void)texSize;

            sf::Vertex v[6];
            v[0].position = {px, py};                         v[0].texCoords = {u0, v0};
            v[1].position = {px + tileSize_, py};              v[1].texCoords = {u1, v0};
            v[2].position = {px + tileSize_, py + tileSize_};  v[2].texCoords = {u1, v1};
            v[3].position = {px, py};                          v[3].texCoords = {u0, v0};
            v[4].position = {px + tileSize_, py + tileSize_};  v[4].texCoords = {u1, v1};
            v[5].position = {px, py + tileSize_};              v[5].texCoords = {u0, v1};
            for (auto& vert : v) vertices_.append(vert);
        }
    }
    dirty = false;
}

void TileLayer::draw(sf::RenderWindow& window, const TileSet& tileset) const {
    if (!tileset.loaded || vertices_.getVertexCount() == 0) return;
    sf::RenderStates states;
    states.texture = &tileset.texture;
    window.draw(vertices_, states);
}

bool TileLayer::save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << width_ << " " << height_ << " " << tileSize_ << "\n";
    for (int ty = 0; ty < height_; ++ty) {
        for (int tx = 0; tx < width_; ++tx) {
            out << tiles_[static_cast<size_t>(ty) * width_ + tx];
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
    std::vector<int> loadedTiles(static_cast<size_t>(w) * h, kEmpty);
    for (auto& v : loadedTiles) {
        if (!(in >> v)) return false;
    }
    width_ = w; height_ = h; tileSize_ = ts;
    tiles_ = std::move(loadedTiles);
    dirty = true;
    return true;
}