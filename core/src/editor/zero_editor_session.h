#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// Runs the main map-editing loop (object placement, selection, camera pan/zoom,
// undo/redo, save/load, inspector panel, etc.) for the given already-open
// project. Split out of zero_map.cpp to keep that file from growing past
// 1000+ lines.
//
// Assumes the project's directory already exists and `window` is open with
// its current size equal to WINDOW_WIDTH x WINDOW_HEIGHT.
//
// Return value:
//   true  -> the whole application should quit (window was closed).
//   false -> the user chose "Return to Home"; the window is still open and
//            the caller should re-show the project picker.
bool RunMapEditorSession(sf::RenderWindow& window,
                          const std::string& projectName,
                          unsigned int WINDOW_WIDTH,
                          unsigned int WINDOW_HEIGHT,
                          sf::Font& uiFont);