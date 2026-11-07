#pragma once

#include <string>
#include <vector>

std::vector<std::string> splitWords(const std::string& s);

// truncate to at most n UTF-8 codepoints, appending an ellipsis (n <= 0: unchanged)
std::string truncateUtf8(const std::string& s, int n);

std::string readFirstLine(const std::string& path);
