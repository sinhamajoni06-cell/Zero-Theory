// ==========================================================
//  EditorState.h - owns the object list, selection, undo/redo
//  history and clipboard for the map editor. Keeping this out
//  of main() makes the editor testable and lets future tools
//  (game runtime, prefab browser, etc.) reuse the same state.
// ==========================================================
#pragma once

#include <vector>
#include <set>
#include <string>
#include "MapObject.h"

class EditorState {
public:
    // ---- Object access ----
    const std::vector<MapObject>& objects() const { return objects_; }
    std::vector<MapObject>& mutableObjects() { return objects_; } // use sparingly; prefer the mutating helpers below

    // ---- Selection (multi-select) ----
    const std::set<int>& selection() const { return selection_; }
    bool isSelected(int index) const { return selection_.count(index) > 0; }
    void selectOnly(int index);
    void toggleSelect(int index);
    void addToSelection(int index);
    void selectInRect(const sf::FloatRect& rect, bool additive);
    void clearSelection();
    bool hasSelection() const { return !selection_.empty(); }

    // ---- Mutations (each pushes undo history automatically) ----
    int addObject(const MapObject& obj);           // returns new index, selects it
    void deleteSelected();
    void moveSelectedBy(float dx, float dy);
    void rotateSelectedBy(float degrees);
    void resizeSelectedBy(float delta);             // grows/shrinks w & h, clamped to >= 10
    void duplicateSelected(float offsetX, float offsetY);

    // ---- Undo / redo ----
    void beginChange();   // call before a mutation you'll commit yourself (e.g. drag)
    void commitChange();  // call after, only if something actually changed
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

    // ---- Dirty-state tracking (for "unsaved changes" prompts) ----
    // Snapshot this after a save/load, then compare later: if it no longer
    // matches, something has changed since the last save.
    size_t undoDepth() const { return undoStack_.size(); }

    // ---- Clipboard ----
    void copySelected();
    void paste(float offsetX, float offsetY);
    bool hasClipboard() const { return !clipboard_.empty(); }

    // ---- Persistence ----
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path); // clears undo history on load

private:
    std::vector<MapObject> objects_;
    std::set<int> selection_;

    std::vector<std::vector<MapObject>> undoStack_;
    std::vector<std::vector<MapObject>> redoStack_;
    std::vector<MapObject> preChangeSnapshot_;
    bool changeInProgress_ = false;

    std::vector<MapObject> clipboard_;

    static constexpr size_t kMaxHistory = 100;

    void pushUndoSnapshot(); // snapshots current objects_ onto undoStack_
};
