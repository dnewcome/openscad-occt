#pragma once
// Runtime value in the SCAD language. Milestone 1 needs only numbers, bools,
// vectors and undef (no strings/ranges/functions yet).
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Value {
  enum class Type { Undef, Number, Bool, Vector, String, Range };
  Type type = Type::Undef;
  double num = 0.0;
  bool boolean = false;
  std::vector<Value> vec;
  std::string str;
  double rstart = 0.0, rstep = 1.0, rend = 0.0;  // when type == Range

  Value() = default;

  static Value makeNumber(double d) { Value v; v.type = Type::Number; v.num = d; return v; }
  static Value makeBool(bool b)     { Value v; v.type = Type::Bool;   v.boolean = b; return v; }
  static Value makeVector(std::vector<Value> xs) { Value v; v.type = Type::Vector; v.vec = std::move(xs); return v; }
  static Value makeString(std::string s) { Value v; v.type = Type::String; v.str = std::move(s); return v; }
  static Value makeRange(double a, double s, double b) { Value v; v.type = Type::Range; v.rstart = a; v.rstep = s; v.rend = b; return v; }

  bool isUndef()  const { return type == Type::Undef; }
  bool isNumber() const { return type == Type::Number; }
  bool isBool()   const { return type == Type::Bool; }
  bool isVector() const { return type == Type::Vector; }
  bool isString() const { return type == Type::String; }
  bool isRange()  const { return type == Type::Range; }

  // Structural equality (used by == / !=). Numbers and bools compare across types.
  bool equals(const Value& o) const {
    if (type != o.type) {
      if (isNumber() && o.isBool()) return num == o.asNumber();
      if (isBool() && o.isNumber()) return asNumber() == o.num;
      return false;
    }
    switch (type) {
      case Type::Number: return num == o.num;
      case Type::Bool:   return boolean == o.boolean;
      case Type::String: return str == o.str;
      case Type::Undef:  return true;
      case Type::Vector:
        if (vec.size() != o.vec.size()) return false;
        for (size_t i = 0; i < vec.size(); ++i) if (!vec[i].equals(o.vec[i])) return false;
        return true;
      case Type::Range:  return rstart == o.rstart && rstep == o.rstep && rend == o.rend;
    }
    return false;
  }

  std::string asString() const {
    if (type == Type::String) return str;
    throw std::runtime_error("expected a string, got " + typeName());
  }

  double asNumber() const {
    if (type == Type::Number) return num;
    if (type == Type::Bool) return boolean ? 1.0 : 0.0;
    throw std::runtime_error("expected a number, got " + typeName());
  }

  bool asBool() const {
    switch (type) {
      case Type::Bool: return boolean;
      case Type::Number: return num != 0.0;
      case Type::Vector: return !vec.empty();
      case Type::String: return !str.empty();
      case Type::Range: return true;
      case Type::Undef: return false;
    }
    return false;
  }

  // Read a vector as exactly n doubles. If the value is a scalar number it is
  // broadcast to all n components (so cube(5) == cube([5,5,5])).
  std::vector<double> asVecN(size_t n) const {
    std::vector<double> out(n, 0.0);
    if (isNumber()) { for (size_t i = 0; i < n; ++i) out[i] = num; return out; }
    if (isVector()) {
      for (size_t i = 0; i < n && i < vec.size(); ++i) out[i] = vec[i].asNumber();
      return out;
    }
    throw std::runtime_error("expected a number or vector, got " + typeName());
  }

  std::string typeName() const {
    switch (type) {
      case Type::Undef: return "undef";
      case Type::Number: return "number";
      case Type::Bool: return "bool";
      case Type::Vector: return "vector";
      case Type::String: return "string";
      case Type::Range: return "range";
    }
    return "?";
  }
};
