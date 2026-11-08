#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lf {

// 极简 JSON 值类型：支持 null/bool/int/double/string/array/object。
// 对象用有序 vector 存储（字段很少，查找 O(n) 足够）。
class JsonValue {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool v);
    JsonValue(int v);
    JsonValue(int64_t v);
    JsonValue(double v);
    JsonValue(const char* v);
    JsonValue(const std::string& v);

    static JsonValue MakeArray();
    static JsonValue MakeObject();

    Type type() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsInt() const { return type_ == Type::Int; }
    bool IsDouble() const { return type_ == Type::Double; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }

    bool AsBool(bool def = false) const;
    int64_t AsInt(int64_t def = 0) const;
    double AsDouble(double def = 0.0) const;
    std::string AsString(const std::string& def = "") const;

    // 数组元素
    const std::vector<JsonValue>& Items() const { return array_; }

    size_t Size() const;
    const JsonValue& At(size_t index) const;
    JsonValue& At(size_t index);
    void Push(JsonValue value);

    bool Has(const std::string& key) const;
    const JsonValue* Find(const std::string& key) const;
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    void Set(const std::string& key, JsonValue value);
    void Remove(const std::string& key);

    std::string Dump() const;
    static bool Parse(const std::string& text, JsonValue& out, std::string& error);

private:
    void DumpTo(std::string& out) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    int64_t int_ = 0;
    double double_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::string> objectKeys_;
    std::vector<JsonValue> objectValues_;
};

} // namespace lf