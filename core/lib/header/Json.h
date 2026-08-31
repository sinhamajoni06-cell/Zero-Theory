// ==========================================================
//  Json.h - minimal, dependency-free recursive JSON value.
//  Supports object / array / string / number / bool / null.
//  No external libraries required (pure STL).
// ==========================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <cctype>

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Type::Null) {}
    JsonValue(std::nullptr_t) : type_(Type::Null) {}
    JsonValue(bool b) : type_(Type::Bool), bool_(b) {}
    JsonValue(double n) : type_(Type::Number), num_(n) {}
    JsonValue(int n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    JsonValue(float n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    JsonValue(const std::string& s) : type_(Type::String), str_(s) {}
    JsonValue(const char* s) : type_(Type::String), str_(s) {}

    static JsonValue Array() { JsonValue v; v.type_ = Type::Array; return v; }
    static JsonValue Object() { JsonValue v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool isNull()   const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray()  const { return type_ == Type::Array; }

    // ---- Object access ----
    JsonValue& operator[](const std::string& key) {
        type_ = Type::Object;
        return obj_[key];
    }
    bool has(const std::string& key) const {
        return type_ == Type::Object && obj_.find(key) != obj_.end();
    }
    const JsonValue& at(const std::string& key) const {
        static JsonValue nullVal;
        auto it = obj_.find(key);
        return it != obj_.end() ? it->second : nullVal;
    }

    // ---- Array access ----
    void push_back(const JsonValue& v) {
        type_ = Type::Array;
        arr_.push_back(v);
    }
    const std::vector<JsonValue>& items() const { return arr_; }
    std::vector<JsonValue>& items() { type_ = Type::Array; return arr_; }

    // ---- Scalar getters (with safe defaults) ----
    double asDouble(double def = 0.0) const { return type_ == Type::Number ? num_ : def; }
    float  asFloat(float def = 0.f)   const { return type_ == Type::Number ? static_cast<float>(num_) : def; }
    int    asInt(int def = 0)         const { return type_ == Type::Number ? static_cast<int>(num_) : def; }
    bool   asBool(bool def = false)   const { return type_ == Type::Bool ? bool_ : def; }
    std::string asString(const std::string& def = "") const { return type_ == Type::String ? str_ : def; }

    // ---- Serialize ----
    std::string dump(int indent = 2) const { std::string out; dumpImpl(out, indent, 0); return out; }

    // ---- Parse ----
    static JsonValue parse(const std::string& text) {
        size_t i = 0;
        skipWs(text, i);
        JsonValue v = parseValue(text, i);
        return v;
    }

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<JsonValue> arr_;
    std::map<std::string, JsonValue> obj_;

    static void skipWs(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }

    static std::string parseRawString(const std::string& s, size_t& i) {
        // assumes s[i] == '"'
        std::string out;
        ++i; // skip opening quote
        while (i < s.size() && s[i] != '"') {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size()) {
                char n = s[i + 1];
                switch (n) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    default: out += n; break;
                }
                i += 2;
            } else {
                out += c;
                ++i;
            }
        }
        if (i < s.size()) ++i; // skip closing quote
        return out;
    }

    static JsonValue parseValue(const std::string& s, size_t& i) {
        skipWs(s, i);
        if (i >= s.size()) return JsonValue();

        char c = s[i];
        if (c == '{') return parseObject(s, i);
        if (c == '[') return parseArray(s, i);
        if (c == '"') return JsonValue(parseRawString(s, i));
        if (c == 't' && s.compare(i, 4, "true") == 0) { i += 4; return JsonValue(true); }
        if (c == 'f' && s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
        if (c == 'n' && s.compare(i, 4, "null") == 0) { i += 4; return JsonValue(nullptr); }

        // number
        size_t start = i;
        if (s[i] == '-' || s[i] == '+') ++i;
        while (i < s.size() && (isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
                                 s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        std::string numStr = s.substr(start, i - start);
        try { return JsonValue(std::stod(numStr)); } catch (...) { return JsonValue(); }
    }

    static JsonValue parseObject(const std::string& s, size_t& i) {
        JsonValue v = Object();
        ++i; // skip '{'
        skipWs(s, i);
        if (i < s.size() && s[i] == '}') { ++i; return v; }
        while (i < s.size()) {
            skipWs(s, i);
            if (i >= s.size() || s[i] != '"') break;
            std::string key = parseRawString(s, i);
            skipWs(s, i);
            if (i < s.size() && s[i] == ':') ++i;
            JsonValue val = parseValue(s, i);
            v.obj_[key] = val;
            skipWs(s, i);
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; break; }
            break;
        }
        return v;
    }

    static JsonValue parseArray(const std::string& s, size_t& i) {
        JsonValue v = Array();
        ++i; // skip '['
        skipWs(s, i);
        if (i < s.size() && s[i] == ']') { ++i; return v; }
        while (i < s.size()) {
            JsonValue val = parseValue(s, i);
            v.arr_.push_back(val);
            skipWs(s, i);
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; break; }
            break;
        }
        return v;
    }

    static void escapeInto(std::string& out, const std::string& s) {
        out += '"';
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                default: out += c; break;
            }
        }
        out += '"';
    }

    void dumpImpl(std::string& out, int indent, int depth) const {
        std::string pad(static_cast<size_t>(indent) * (depth + 1), ' ');
        std::string padEnd(static_cast<size_t>(indent) * depth, ' ');

        switch (type_) {
            case Type::Null: out += "null"; break;
            case Type::Bool: out += bool_ ? "true" : "false"; break;
            case Type::Number: {
                // Trim trailing zeros for clean output while keeping precision.
                std::ostringstream oss;
                oss << num_;
                out += oss.str();
                break;
            }
            case Type::String: escapeInto(out, str_); break;
            case Type::Array: {
                if (arr_.empty()) { out += "[]"; break; }
                out += "[\n";
                for (size_t k = 0; k < arr_.size(); ++k) {
                    out += pad;
                    arr_[k].dumpImpl(out, indent, depth + 1);
                    if (k + 1 < arr_.size()) out += ",";
                    out += "\n";
                }
                out += padEnd + "]";
                break;
            }
            case Type::Object: {
                if (obj_.empty()) { out += "{}"; break; }
                out += "{\n";
                size_t k = 0;
                for (const auto& [key, val] : obj_) {
                    out += pad;
                    escapeInto(out, key);
                    out += ": ";
                    val.dumpImpl(out, indent, depth + 1);
                    if (++k < obj_.size()) out += ",";
                    out += "\n";
                }
                out += padEnd + "}";
                break;
            }
        }
    }
};