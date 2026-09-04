#include "EditorState.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// ---------------- Selection ----------------

void EditorState::selectOnly(int index) {
    selection_.clear();
    if (index >= 0 && index < static_cast<int>(objects_.size())) selection_.insert(index);
}

void EditorState::toggleSelect(int index) {
    if (index < 0 || index >= static_cast<int>(objects_.size())) return;
    if (selection_.count(index)) selection_.erase(index);
    else selection_.insert(index);
}

void EditorState::addToSelection(int index) {
    if (index >= 0 && index < static_cast<int>(objects_.size())) selection_.insert(index);
}

void EditorState::selectInRect(const sf::FloatRect& rect, bool additive) {
    if (!additive) selection_.clear();
    for (int i = 0; i < static_cast<int>(objects_.size()); ++i) {
        if (ObjectIntersectsRect(objects_[i], rect)) selection_.insert(i);
    }
}

void EditorState::clearSelection() {
    selection_.clear();
}

// ---------------- Undo / redo plumbing ----------------

void EditorState::pushUndoSnapshot() {
    undoStack_.push_back(objects_);
    if (undoStack_.size() > kMaxHistory) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void EditorState::beginChange() {
    if (changeInProgress_) return; // don't clobber a change already in progress
    preChangeSnapshot_ = objects_;
    changeInProgress_ = true;
}

void EditorState::commitChange() {
    if (!changeInProgress_) return;
    changeInProgress_ = false;
    if (preChangeSnapshot_.size() != objects_.size() ||
        !std::equal(preChangeSnapshot_.begin(), preChangeSnapshot_.end(), objects_.begin(),
                    [](const MapObject& a, const MapObject& b) {
                        return a.type == b.type && a.x == b.x && a.y == b.y &&
                               a.w == b.w && a.h == b.h && a.rotation == b.rotation;
                    })) {
        undoStack_.push_back(preChangeSnapshot_);
        if (undoStack_.size() > kMaxHistory) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }
}

void EditorState::undo() {
    if (undoStack_.empty()) return;
    redoStack_.push_back(objects_);
    objects_ = undoStack_.back();
    undoStack_.pop_back();
    selection_.clear();
}

void EditorState::redo() {
    if (redoStack_.empty()) return;
    undoStack_.push_back(objects_);
    objects_ = redoStack_.back();
    redoStack_.pop_back();
    selection_.clear();
}

// ---------------- Mutations ----------------

int EditorState::addObject(const MapObject& obj) {
    pushUndoSnapshot();
    objects_.push_back(obj);
    int idx = static_cast<int>(objects_.size()) - 1;
    selectOnly(idx);
    return idx;
}

void EditorState::deleteSelected() {
    if (selection_.empty()) return;
    pushUndoSnapshot();
    // Erase from highest index to lowest so earlier indices stay valid.
    std::vector<int> indices(selection_.begin(), selection_.end());
    std::sort(indices.rbegin(), indices.rend());
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(objects_.size())) {
            objects_.erase(objects_.begin() + idx);
        }
    }
    selection_.clear();
}

void EditorState::moveSelectedBy(float dx, float dy) {
    for (int idx : selection_) {
        if (idx >= 0 && idx < static_cast<int>(objects_.size())) {
            objects_[idx].x += dx;
            objects_[idx].y += dy;
        }
    }
}

void EditorState::rotateSelectedBy(float degrees) {
    if (selection_.empty()) return;
    pushUndoSnapshot();
    for (int idx : selection_) {
        if (idx < 0 || idx >= static_cast<int>(objects_.size())) continue;
        float& r = objects_[idx].rotation;
        r += degrees;
        while (r >= 360.f) r -= 360.f;
        while (r < 0.f) r += 360.f;
    }
}

void EditorState::resizeSelectedBy(float delta) {
    if (selection_.empty()) return;
    pushUndoSnapshot();
    for (int idx : selection_) {
        if (idx < 0 || idx >= static_cast<int>(objects_.size())) continue;
        objects_[idx].w = std::max(10.f, objects_[idx].w + delta);
        objects_[idx].h = std::max(10.f, objects_[idx].h + delta);
    }
}

void EditorState::duplicateSelected(float offsetX, float offsetY) {
    if (selection_.empty()) return;
    pushUndoSnapshot();
    std::vector<int> indices(selection_.begin(), selection_.end());
    std::sort(indices.begin(), indices.end());
    std::set<int> newSelection;
    for (int idx : indices) {
        if (idx < 0 || idx >= static_cast<int>(objects_.size())) continue;
        MapObject copy = objects_[idx];
        copy.x += offsetX;
        copy.y += offsetY;
        objects_.push_back(copy);
        newSelection.insert(static_cast<int>(objects_.size()) - 1);
    }
    selection_ = newSelection;
}

// ---------------- Clipboard ----------------

void EditorState::copySelected() {
    clipboard_.clear();
    for (int idx : selection_) {
        if (idx >= 0 && idx < static_cast<int>(objects_.size())) {
            clipboard_.push_back(objects_[idx]);
        }
    }
}

void EditorState::paste(float offsetX, float offsetY) {
    if (clipboard_.empty()) return;
    pushUndoSnapshot();
    std::set<int> newSelection;
    for (const MapObject& obj : clipboard_) {
        MapObject copy = obj;
        copy.x += offsetX;
        copy.y += offsetY;
        objects_.push_back(copy);
        newSelection.insert(static_cast<int>(objects_.size()) - 1);
    }
    selection_ = newSelection;
}

// ---------------- Persistence ----------------

bool EditorState::saveToFile(const std::string& path, int mapWidthTiles, int mapHeightTiles) const {
    JsonValue root = JsonValue::Object();
    JsonValue arr = JsonValue::Array();
    for (const MapObject& obj : objects_) arr.push_back(obj.toJson());
    root["objects"] = arr;
    root["mapWidthTiles"] = mapWidthTiles;
    root["mapHeightTiles"] = mapHeightTiles;

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[MapEditor] Failed to open file for saving: " << path << std::endl;
        return false;
    }
    file << root.dump();
    file.close();
    std::cout << "[MapEditor] Saved " << objects_.size() << " objects to " << path << std::endl;
    return true;
}

bool EditorState::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[MapEditor] Failed to open file for loading: " << path << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    JsonValue root = JsonValue::parse(buffer.str());
    objects_.clear();
    if (root.has("objects")) {
        for (const JsonValue& item : root.at("objects").items()) {
            objects_.push_back(MapObject::fromJson(item));
        }
    }
    lastLoadedMapWidthTiles_ = root.has("mapWidthTiles") ? root.at("mapWidthTiles").asInt() : 0;
    lastLoadedMapHeightTiles_ = root.has("mapHeightTiles") ? root.at("mapHeightTiles").asInt() : 0;
    selection_.clear();
    undoStack_.clear();
    redoStack_.clear();
    std::cout << "[MapEditor] Loaded " << objects_.size() << " objects from " << path << std::endl;
    return true;
}
