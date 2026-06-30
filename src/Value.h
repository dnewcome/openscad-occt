#pragma once
// Runtime value in the SCAD language. Milestone 1 needs only numbers, bools,
// vectors and undef (no strings/ranges/functions yet).
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Value {
  enum class Type { Undef, Number, Bool, Vector };
  Type type = Type::Undef;
  double num = 0.0;
  bool boolean = false;
  std::vector<Value> vec;

  Value() = default;

  static Value makeNumber(double d) { Value v; v.type = Type::Number; v.num = d; return v; }
  static Value makeBool(bool b)     { Value v; v.type = Type::Bool;   v.boolean = b; return v; }
  static Value makeVector(std::vector<Value> xs) { Value v; v.type = Type::Vector; v.vec = std::move(xs); return v; }

  bool isUndef()  const { return type == Type::Undef; }
  bool isNumber() const { return type == Type::Number; }
  bool isBool()   const { return type == Type::Bool; }
  bool isVector() const { return type == Type::Vector; }

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
    }
    return "?";
  }
};
