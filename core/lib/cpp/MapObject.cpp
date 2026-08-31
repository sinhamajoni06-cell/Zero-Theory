#include "MapObject.h"
#include <cmath>
#include <algorithm>

std::string ObjectTypeToString(ObjectType type) {
    switch (type) {
        case ObjectType::Block:    return "block";
        case ObjectType::Spike:    return "spike";
        case ObjectType::Platform: return "platform";
        case ObjectType::Coin:     return "coin";
    }
    return "block";
}

ObjectType ObjectTypeFromString(const std::string& s) {
    if (s == "spike")    return ObjectType::Spike;
    if (s == "platform") return ObjectType::Platform;
    if (s == "coin")     return ObjectType::Coin;
    return ObjectType::Block;
}

sf::Color ObjectTypeColor(ObjectType type) {
    switch (type) {
        case ObjectType::Block:    return sf::Color(120, 120, 130);
        case ObjectType::Spike:    return sf::Color(220, 60, 60);
        case ObjectType::Platform: return sf::Color(90, 160, 220);
        case ObjectType::Coin:     return sf::Color(240, 200, 60);
    }
    return sf::Color::White;
}

float SnapValue(float value, float gridSize) {
    return std::round(value / gridSize) * gridSize;
}

sf::RectangleShape MakeShapeForObject(const MapObject& obj) {
    sf::RectangleShape shape({obj.w, obj.h});
    shape.setOrigin({obj.w / 2.f, obj.h / 2.f});
    shape.setPosition({obj.x, obj.y});
    shape.setRotation(sf::degrees(obj.rotation));
    shape.setFillColor(ObjectTypeColor(obj.type));
    shape.setOutlineThickness(1.f);
    shape.setOutlineColor(sf::Color(0, 0, 0, 180));
    return shape;
}

bool PointInObject(const MapObject& obj, sf::Vector2f point) {
    // Rotate the point into the object's local space around its center,
    // then do a simple AABB test. This makes selection correct even
    // when the object has been rotated.
    float rad = -obj.rotation * 3.14159265f / 180.f;
    float dx = point.x - obj.x;
    float dy = point.y - obj.y;
    float localX = dx * std::cos(rad) - dy * std::sin(rad);
    float localY = dx * std::sin(rad) + dy * std::cos(rad);

    return localX >= -obj.w / 2.f && localX <= obj.w / 2.f &&
           localY >= -obj.h / 2.f && localY <= obj.h / 2.f;
}

bool ObjectIntersectsRect(const MapObject& obj, const sf::FloatRect& rect) {
    // Cheap AABB-vs-AABB overlap (ignores rotation, used for box-select).
    float left = obj.x - obj.w / 2.f;
    float right = obj.x + obj.w / 2.f;
    float top = obj.y - obj.h / 2.f;
    float bottom = obj.y + obj.h / 2.f;

    float rLeft = rect.position.x;
    float rTop = rect.position.y;
    float rRight = rect.position.x + rect.size.x;
    float rBottom = rect.position.y + rect.size.y;

    return left <= rRight && right >= rLeft && top <= rBottom && bottom >= rTop;
}

JsonValue MapObject::toJson() const {
    JsonValue v = JsonValue::Object();
    v["type"] = ObjectTypeToString(type);
    v["x"] = x;
    v["y"] = y;
    v["w"] = w;
    v["h"] = h;
    v["rotation"] = rotation;
    return v;
}

MapObject MapObject::fromJson(const JsonValue& v) {
    MapObject obj;
    obj.type = ObjectTypeFromString(v.at("type").asString("block"));
    obj.x = v.at("x").asFloat();
    obj.y = v.at("y").asFloat();
    obj.w = v.at("w").asFloat(40.f);
    obj.h = v.at("h").asFloat(40.f);
    obj.rotation = v.at("rotation").asFloat();
    return obj;
}