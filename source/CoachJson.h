/*
 * File:   CoachJson.h
 *
 * Lightweight JSON serialization utilities for the Coaching Protocol.
 * Builds compact JSON strings from C++ data structures.
 * No external dependencies.
 */

#ifndef COACHJSON_H
#define COACHJSON_H

#include <string>
#include <vector>
#include <utility>

// Forward declarations — avoid pulling in heavy headers
struct PositionReport;
struct ComparisonReport;

namespace CoachJson {

// Primitive serializers
std::string to_json(int value);
std::string to_json(bool value);
std::string to_json(const std::string& value);  // escapes ", \, and control characters
std::string to_json(const char* value);          // delegates to string overload
std::string to_json_null();

// Array/object builders
std::string array(const std::vector<std::string>& elements);
std::string object(const std::vector<std::pair<std::string, std::string>>& fields);

// High-level serializers
std::string serialize_pong(const std::string& engine_name, const std::string& engine_version);
std::string serialize_error(const std::string& code, const std::string& message);
std::string serialize_position_report(const PositionReport& report);
std::string serialize_comparison_report(const ComparisonReport& report);

} // namespace CoachJson

#endif /* COACHJSON_H */
