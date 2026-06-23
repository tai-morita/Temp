#pragma once

#include <cctype>
#include <istream>
#include <string>
#include <unordered_map>

namespace nlohmann {

// Sample-only minimal JSON class.
// Supports flat objects like {"key": 123, "name": "abc"}.
// Replace this file with the official json.hpp for production use.
class json {
public:
  int value(const std::string& key, int default_value) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) {
      return default_value;
    }
    try {
      return std::stoi(it->second);
    }
    catch (...) {
      return default_value;
    }
  }

  std::string value(const std::string& key, const std::string& default_value) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) {
      return default_value;
    }
    return it->second;
  }

  friend std::istream& operator>>(std::istream& is, json& j) {
    std::string text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    if (!j.parse(text)) {
      is.setstate(std::ios::failbit);
    }
    return is;
  }

private:
  std::unordered_map<std::string, std::string> m_values;

  static void skip_ws(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
  }

  static bool parse_quoted(const std::string& s, std::size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') {
      return false;
    }
    ++i;
    out.clear();
    while (i < s.size()) {
      const char c = s[i++];
      if (c == '"') {
        return true;
      }
      if (c == '\\' && i < s.size()) {
        out.push_back(s[i++]);
      }
      else {
        out.push_back(c);
      }
    }
    return false;
  }

  static bool parse_token(const std::string& s, std::size_t& i, std::string& out) {
    skip_ws(s, i);
    if (i >= s.size()) {
      return false;
    }

    if (s[i] == '"') {
      return parse_quoted(s, i, out);
    }

    const std::size_t start = i;
    while (i < s.size() && s[i] != ',' && s[i] != '}') {
      ++i;
    }

    std::size_t end = i;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
      --end;
    }

    if (end <= start) {
      return false;
    }

    out.assign(s, start, end - start);
    return true;
  }

  bool parse(const std::string& text) {
    m_values.clear();
    std::size_t i = 0;
    skip_ws(text, i);

    if (i >= text.size() || text[i] != '{') {
      return false;
    }
    ++i;

    for (;;) {
      skip_ws(text, i);
      if (i < text.size() && text[i] == '}') {
        ++i;
        return true;
      }

      std::string key;
      if (!parse_quoted(text, i, key)) {
        return false;
      }

      skip_ws(text, i);
      if (i >= text.size() || text[i] != ':') {
        return false;
      }
      ++i;

      std::string val;
      if (!parse_token(text, i, val)) {
        return false;
      }

      m_values[key] = val;

      skip_ws(text, i);
      if (i >= text.size()) {
        return false;
      }
      if (text[i] == ',') {
        ++i;
        continue;
      }
      if (text[i] == '}') {
        ++i;
        return true;
      }
      return false;
    }
  }
};

} // namespace nlohmann