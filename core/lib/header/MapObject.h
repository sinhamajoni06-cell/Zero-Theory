// ==========================================================
//  MapObject.h - core data model for the map editor.
// ==========================================================
#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Json.h"

enum class ObjectType {
    Block = 0,
    Spike,
    Platform,
    Coin
};

struct MapObject {
    ObjectType type = ObjectType::Block;
    float x = 0.f, y = 0.f;   // world position (center)
    float w = 40.f, h = 40.f; // size
    float rotation = 0.f;     // degrees

    JsonValue toJson() const;
    static MapObject fromJson(const JsonValue& v);
};

std::string ObjectTypeToString(ObjectType type);
ObjectType ObjectTypeFromString(const std::string& s);
sf::Color ObjectTypeColor(ObjectType type);

// Grid snapping helper.
float SnapValue(float value, float gridSize);

// Builds a drawable shape for an object (handles rotation + color + outline).
sf::RectangleShape MakeShapeForObject(const MapObject& obj);

// Rotation-aware hit test: transforms the point into the object's local
// (unrotated) space before doing the AABB check, so rotated objects can
// still be selected accurately.
bool PointInObject(const MapObject& obj, sf::Vector2f point);

// Whether an object's AABB (rotation ignored, for cheap box-select) overlaps a rect.
bool ObjectIntersectsRect(const MapObject& obj, const sf::FloatRect& rect);