#include "Json.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace lf {

namespace {

const JsonValue& NullValue() {
    static const JsonValue null;
    return null;
}

void AppendUtf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void DumpString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

class Parser {
public:
    Parser(const std::string& text) : text_(text) {}

    bool ParseDocument(JsonValue& out, std::string& error) {
        SkipWs();
        if (!ParseValue(out, 0)) {
            error = error_ + " (at offset " + std::to_string(pos_) + ")";
            return false;
        }
        SkipWs();
        if (pos_ != text_.size()) {
            error = "Unexpected trailing characters (at offset " + std::to_string(pos_) + ")";
            return false;
        }
        return true;
    }

private:
    void SkipWs() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool Fail(const std::string& message) {
        if (error_.empty()) error_ = message;
        return false;
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > 64) return Fail("Nesting too deep");
        if (pos_ >= text_.size()) return Fail("Unexpected end of input");
        char c = text_[pos_];
        switch (c) {
            case '{': return ParseObject(out, depth);
            case '[': return ParseArray(out, depth);
            case '"': {
                std::string s;
                if (!ParseString(s)) return false;
                out = JsonValue(s);
                return true;
            }
            case 't': {
                if (text_.compare(pos_, 4, "true") != 0) return Fail("Invalid literal");
                pos_ += 4;
                out = JsonValue(true);
                return true;
            }
            case 'f': {
                if (text_.compare(pos_, 5, "false") != 0) return Fail("Invalid literal");
                pos_ += 5;
                out = JsonValue(false);
                return true;
            }
            case 'n': {
                if (text_.compare(pos_, 4, "null") != 0) return Fail("Invalid literal");
                pos_ += 4;
                out = JsonValue(nullptr);
                return true;
            }
            default: return ParseNumber(out);
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        out = JsonValue::MakeObject();
        ++pos_; // {
        SkipWs();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipWs();
            if (pos_ >= text_.size() || text_[pos_] != '"') return Fail("Expected object key");
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (pos_ >= text_.size() || text_[pos_] != ':') return Fail("Expected ':'");
            ++pos_;
            SkipWs();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.Set(key, value);
            SkipWs();
            if (pos_ >= text_.size()) return Fail("Unterminated object");
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return Fail("Expected ',' or '}'");
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        out = JsonValue::MakeArray();
        ++pos_; // [
        SkipWs();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipWs();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.Push(value);
            SkipWs();
            if (pos_ >= text_.size()) return Fail("Unterminated array");
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return Fail("Expected ',' or ']'");
        }
    }

    bool ParseString(std::string& out) {
        if (pos_ >= text_.size() || text_[pos_] != '"') return Fail("Expected string");
        ++pos_;
        while (pos_ < text_.size()) {
            unsigned char c = static_cast<unsigned char>(text_[pos_]);
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c == '\\') {
                ++pos_;
                if (pos_ >= text_.size()) return Fail("Bad escape");
                char e = text_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned int cp = 0;
                        if (!ParseHex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (pos_ + 1 < text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                                pos_ += 2;
                                unsigned int low = 0;
                                if (!ParseHex4(low)) return false;
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    return Fail("Bad surrogate pair");
                                }
                            } else {
                                return Fail("Bad surrogate pair");
                            }
                        }
                        AppendUtf8(out, cp);
                        break;
                    }
                    default: return Fail("Bad escape sequence");
                }
                continue;
            }
            if (c < 0x20) return Fail("Control character in string");
            out.push_back(static_cast<char>(c));
            ++pos_;
        }
        return Fail("Unterminated string");
    }

    bool ParseHex4(unsigned int& out) {
        if (pos_ + 4 > text_.size()) return Fail("Bad unicode escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text_[pos_++];
            int v = -1;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            if (v < 0) return Fail("Bad unicode escape");
            out = (out << 4) | static_cast<unsigned>(v);
        }
        return true;
    }

    bool ParseNumber(JsonValue& out) {
        size_t start = pos_;
        if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
        bool isDouble = false;
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c >= '0' && c <= '9') {
                ++pos_;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                isDouble = true;
                ++pos_;
            } else {
                break;
            }
        }
        if (pos_ == start) return Fail("Invalid value");
        std::string token = text_.substr(start, pos_ - start);
        try {
            if (isDouble) {
                out = JsonValue(std::stod(token));
            } else {
                long long v = std::stoll(token);
                out = JsonValue(static_cast<int64_t>(v));
            }
        } catch (...) {
            return Fail("Invalid number");
        }
        return true;
    }

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;
};

} // namespace

JsonValue::JsonValue() = default;
JsonValue::JsonValue(std::nullptr_t) : type_(Type::Null) {}
JsonValue::JsonValue(bool v) : type_(Type::Bool), bool_(v) {}
JsonValue::JsonValue(int v) : type_(Type::Int), int_(v) {}
JsonValue::JsonValue(int64_t v) : type_(Type::Int), int_(v) {}
JsonValue::JsonValue(double v) : type_(Type::Double), double_(v) {}
JsonValue::JsonValue(const char* v) : type_(Type::String), string_(v ? v : "") {}
JsonValue::JsonValue(const std::string& v) : type_(Type::String), string_(v) {}

JsonValue JsonValue::MakeArray() {
    JsonValue v;
    v.type_ = Type::Array;
    return v;
}

JsonValue JsonValue::MakeObject() {
    JsonValue v;
    v.type_ = Type::Object;
    return v;
}

bool JsonValue::AsBool(bool def) const {
    if (type_ == Type::Bool) return bool_;
    if (type_ == Type::Int) return int_ != 0;
    return def;
}

int64_t JsonValue::AsInt(int64_t def) const {
    if (type_ == Type::Int) return int_;
    if (type_ == Type::Double) return static_cast<int64_t>(double_);
    if (type_ == Type::Bool) return bool_ ? 1 : 0;
    return def;
}

double JsonValue::AsDouble(double def) const {
    if (type_ == Type::Double) return double_;
    if (type_ == Type::Int) return static_cast<double>(int_);
    return def;
}

std::string JsonValue::AsString(const std::string& def) const {
    if (type_ == Type::String) return string_;
    return def;
}

size_t JsonValue::Size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return objectKeys_.size();
    return 0;
}

const JsonValue& JsonValue::At(size_t index) const {
    if (type_ == Type::Array) {
        if (index < array_.size()) return array_[index];
        return NullValue();
    }
    if (type_ == Type::Object) {
        if (index < objectValues_.size()) return objectValues_[index];
        return NullValue();
    }
    return NullValue();
}

JsonValue& JsonValue::At(size_t index) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    if (index >= array_.size()) array_.resize(index + 1);
    return array_[index];
}

void JsonValue::Push(JsonValue value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    array_.push_back(std::move(value));
}

bool JsonValue::Has(const std::string& key) const {
    return Find(key) != nullptr;
}

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (size_t i = 0; i < objectKeys_.size(); ++i) {
        if (objectKeys_[i] == key) return &objectValues_[i];
    }
    return nullptr;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectKeys_.clear();
        objectValues_.clear();
    }
    for (size_t i = 0; i < objectKeys_.size(); ++i) {
        if (objectKeys_[i] == key) return objectValues_[i];
    }
    objectKeys_.push_back(key);
    objectValues_.push_back(JsonValue());
    return objectValues_.back();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    const JsonValue* found = Find(key);
    return found ? *found : NullValue();
}

void JsonValue::Set(const std::string& key, JsonValue value) {
    (*this)[key] = std::move(value);
}

void JsonValue::Remove(const std::string& key) {
    if (type_ != Type::Object) return;
    for (size_t i = 0; i < objectKeys_.size(); ++i) {
        if (objectKeys_[i] == key) {
            objectKeys_.erase(objectKeys_.begin() + static_cast<std::ptrdiff_t>(i));
            objectValues_.erase(objectValues_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

void JsonValue::DumpTo(std::string& out) const {
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Int: out += std::to_string(int_); break;
        case Type::Double: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", double_);
            out += buf;
            break;
        }
        case Type::String: DumpString(string_, out); break;
        case Type::Array: {
            out.push_back('[');
            for (size_t i = 0; i < array_.size(); ++i) {
                if (i) out.push_back(',');
                array_[i].DumpTo(out);
            }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            for (size_t i = 0; i < objectKeys_.size(); ++i) {
                if (i) out.push_back(',');
                DumpString(objectKeys_[i], out);
                out.push_back(':');
                objectValues_[i].DumpTo(out);
            }
            out.push_back('}');
            break;
        }
    }
}

std::string JsonValue::Dump() const {
    std::string out;
    out.reserve(128);
    DumpTo(out);
    return out;
}

bool JsonValue::Parse(const std::string& text, JsonValue& out, std::string& error) {
    Parser parser(text);
    return parser.ParseDocument(out, error);
}

} // namespace lf