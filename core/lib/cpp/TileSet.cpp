#include "TileSet.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

bool TileSet::loadFromImage(const std::string& path, int tileW, int tileH) {
    if (tileW <= 0 || tileH <= 0) return false;
    if (!texture.loadFromFile(path)) {
        loaded = false;
        return false;
    }
    imagePath = path;
    tileWidth = tileW;
    tileHeight = tileH;
    sf::Vector2u size = texture.getSize();
    columns = static_cast<int>(size.x) / tileWidth;
    rows = static_cast<int>(size.y) / tileHeight;
    loaded = columns > 0 && rows > 0;
    return loaded;
}

sf::IntRect TileSet::tileRect(int index) const {
    if (columns <= 0 || index < 0) return sf::IntRect({0, 0}, {tileWidth, tileHeight});
    int col = index % columns;
    int row = index / columns;
    return sf::IntRect({col * tileWidth, row * tileHeight}, {tileWidth, tileHeight});
}

bool TileSet::saveMeta(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << imagePath << "\n";
    out << tileWidth << " " << tileHeight << "\n";
    return true;
}

bool TileSet::loadMeta(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string storedPath;
    int tw = 0, th = 0;
    if (!std::getline(in, storedPath)) return false;
    in >> tw >> th;
    if (tw <= 0 || th <= 0) return false;
    return loadFromImage(storedPath, tw, th);
}

std::vector<std::string> ListImportableImages(const std::string& folder) {
    std::vector<std::string> result;
    std::filesystem::create_directories(folder);
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
            result.push_back(entry.path().filename().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}