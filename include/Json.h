#pragma once
// =====================================================================
// Json.h
// ---------------------------------------------------------------------
// A very small, beginner-friendly JSON reader/writer.
//
// WHY WE WROTE OUR OWN INSTEAD OF USING A LIBRARY:
// The most popular C++ JSON library (nlohmann/json) is usually installed
// with a package manager or downloaded from the internet. This project
// needs to compile with nothing but the standard C++ library, so this
// file implements just enough JSON support for OUR data files:
//   - arrays of flat objects        (users.json, accounts.json, ...)
//   - simple nested objects         (config/fraud_rules.json)
//
// It is NOT a general-purpose JSON library. It does not need to be -
// we only ever read/write the shapes of data this project uses.
// =====================================================================

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cctype>

// A JsonValue can hold ONE of these kinds of data at a time.
enum class JsonType { Null, Bool, Number, String, Array, Object };

class JsonValue
{
public:
    JsonType type = JsonType::Null;

    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    // We use a vector of pairs (not std::map) so that keys keep the
    // order they were written in the file - this makes saved files
    // easier for a human to read and compare.
    std::vector<std::pair<std::string, JsonValue>> objectValue;

    // ---- Convenience "getters" used all over the DataManager ----

    // Get a field from an object by key. Throws if the key is missing,
    // which is intentional: a missing required field in a data file is
    // a bug we want to notice immediately, not silently ignore.
    const JsonValue& at(const std::string& key) const
    {
        for (const auto& pair : objectValue)
        {
            if (pair.first == key) return pair.second;
        }
        throw std::runtime_error("JSON key not found: " + key);
    }

    bool hasKey(const std::string& key) const
    {
        for (const auto& pair : objectValue)
        {
            if (pair.first == key) return true;
        }
        return false;
    }

    std::string asString() const { return stringValue; }
    double asDouble() const { return numberValue; }
    int asInt() const { return static_cast<int>(numberValue); }
    bool asBool() const { return boolValue; }

    // ---- Convenience "builders" used when we SAVE data ----

    static JsonValue makeString(const std::string& s)
    {
        JsonValue v; v.type = JsonType::String; v.stringValue = s; return v;
    }
    static JsonValue makeNumber(double n)
    {
        JsonValue v; v.type = JsonType::Number; v.numberValue = n; return v;
    }
    static JsonValue makeObject()
    {
        JsonValue v; v.type = JsonType::Object; return v;
    }
    static JsonValue makeArray()
    {
        JsonValue v; v.type = JsonType::Array; return v;
    }

    void set(const std::string& key, const JsonValue& value)
    {
        for (auto& pair : objectValue)
        {
            if (pair.first == key) { pair.second = value; return; }
        }
        objectValue.push_back({ key, value });
    }

    // Turn this value back into JSON text, with simple indentation.
    std::string dump(int indent = 0) const
    {
        std::string pad(indent * 4, ' ');
        std::string padInner((indent + 1) * 4, ' ');

        switch (type)
        {
        case JsonType::Null:
            return "null";
        case JsonType::Bool:
            return boolValue ? "true" : "false";
        case JsonType::Number:
        {
            // Print whole numbers without a trailing ".0" and keep
            // decimals for everything else (money amounts).
            std::ostringstream out;
            if (numberValue == static_cast<long long>(numberValue))
                out << static_cast<long long>(numberValue);
            else
                out << numberValue;
            return out.str();
        }
        case JsonType::String:
            return "\"" + escape(stringValue) + "\"";
        case JsonType::Array:
        {
            if (arrayValue.empty()) return "[]";
            std::string out = "[\n";
            for (size_t i = 0; i < arrayValue.size(); ++i)
            {
                out += padInner + arrayValue[i].dump(indent + 1);
                if (i + 1 < arrayValue.size()) out += ",";
                out += "\n";
            }
            out += pad + "]";
            return out;
        }
        case JsonType::Object:
        {
            if (objectValue.empty()) return "{}";
            std::string out = "{\n";
            for (size_t i = 0; i < objectValue.size(); ++i)
            {
                out += padInner + "\"" + escape(objectValue[i].first) + "\": "
                     + objectValue[i].second.dump(indent + 1);
                if (i + 1 < objectValue.size()) out += ",";
                out += "\n";
            }
            out += pad + "}";
            return out;
        }
        }
        return "null";
    }

private:
    static std::string escape(const std::string& s)
    {
        std::string out;
        for (char c : s)
        {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }
};

// ---------------------------------------------------------------------
// JsonParser: turns JSON text into a JsonValue tree.
// This is a small "recursive descent" parser - a common, easy-to-follow
// technique where each function knows how to read one kind of thing
// (an object, an array, a string, a number) and calls the others as
// needed.
// ---------------------------------------------------------------------
class JsonParser
{
public:
    static JsonValue parse(const std::string& text)
    {
        JsonParser parser(text);
        parser.skipWhitespace();
        JsonValue result = parser.parseValue();
        return result;
    }

    static JsonValue parseFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open JSON file: " + path);
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }

private:
    const std::string& text;
    size_t pos = 0;

    explicit JsonParser(const std::string& t) : text(t) {}

    void skipWhitespace()
    {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) pos++;
    }

    char peek() { return pos < text.size() ? text[pos] : '\0'; }
    char get() { return text[pos++]; }

    JsonValue parseValue()
    {
        skipWhitespace();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { pos += 4; return JsonValue(); } // "null"
        return parseNumber();
    }

    JsonValue parseObject()
    {
        JsonValue obj; obj.type = JsonType::Object;
        get(); // consume '{'
        skipWhitespace();
        if (peek() == '}') { get(); return obj; }

        while (true)
        {
            skipWhitespace();
            JsonValue key = parseString();
            skipWhitespace();
            get(); // consume ':'
            JsonValue value = parseValue();
            obj.objectValue.push_back({ key.stringValue, value });
            skipWhitespace();
            if (peek() == ',') { get(); continue; }
            break;
        }
        skipWhitespace();
        get(); // consume '}'
        return obj;
    }

    JsonValue parseArray()
    {
        JsonValue arr; arr.type = JsonType::Array;
        get(); // consume '['
        skipWhitespace();
        if (peek() == ']') { get(); return arr; }

        while (true)
        {
            JsonValue value = parseValue();
            arr.arrayValue.push_back(value);
            skipWhitespace();
            if (peek() == ',') { get(); continue; }
            break;
        }
        skipWhitespace();
        get(); // consume ']'
        return arr;
    }

    JsonValue parseString()
    {
        JsonValue v; v.type = JsonType::String;
        get(); // consume opening quote
        std::string result;
        while (peek() != '"')
        {
            char c = get();
            if (c == '\\')
            {
                char next = get();
                if (next == 'n') result += '\n';
                else result += next; // handles \" and \\ and others simply
            }
            else
            {
                result += c;
            }
        }
        get(); // consume closing quote
        v.stringValue = result;
        return v;
    }

    JsonValue parseBool()
    {
        JsonValue v; v.type = JsonType::Bool;
        if (text.compare(pos, 4, "true") == 0) { v.boolValue = true; pos += 4; }
        else { v.boolValue = false; pos += 5; } // "false"
        return v;
    }

    JsonValue parseNumber()
    {
        JsonValue v; v.type = JsonType::Number;
        size_t start = pos;
        while (pos < text.size() &&
               (std::isdigit(static_cast<unsigned char>(text[pos])) ||
                text[pos] == '-' || text[pos] == '+' ||
                text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E'))
        {
            pos++;
        }
        v.numberValue = std::stod(text.substr(start, pos - start));
        return v;
    }
};
