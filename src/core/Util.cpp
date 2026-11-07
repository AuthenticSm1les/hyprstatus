#include "Util.hpp"

#include <fstream>
#include <sstream>

std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream       ss(s);
    std::string              w;
    while (ss >> w)
        out.push_back(w);
    return out;
}

std::string truncateUtf8(const std::string& s, int n) {
    if (n <= 0)
        return s;
    int    count = 0;
    size_t i     = 0;
    while (i < s.size()) {
        if (count == n)
            return s.substr(0, i) + "…";
        const unsigned char C = s[i];
        i += C < 0x80 ? 1 : (C >> 5) == 0x6 ? 2 : (C >> 4) == 0xE ? 3 : 4;
        count++;
    }
    return s;
}


std::string readFirstLine(const std::string& path) {
    std::ifstream f(path);
    std::string   line;
    if (f)
        std::getline(f, line);
    return line;
}
