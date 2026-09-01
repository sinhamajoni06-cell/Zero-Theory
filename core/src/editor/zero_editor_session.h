#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// Runs the main map-editing loop (object placement, selection, camera pan/zoom,
// undo/redo, save/load, etc.) for the given already-open project. This covers
// everything that used to live after the "creatingProject" home-page loop in
// zero_map.cpp, split out to keep that file from growing past 1000+ lines.
//
// Assumes the project's directory already exists and `window` is open with
// its current size equal to WINDOW_WIDTH x WINDOW_HEIGHT. Returns when the
// window is closed.
void RunMapEditorSession(sf::RenderWindow& window,
                          const std::string& projectName,
                          unsigned int WINDOW_WIDTH,
                          unsigned int WINDOW_HEIGHT);