#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <vector>

namespace sophoi {
std::string trim(const std::string &line, bool sharpen = false);
std::vector<std::string> split(const std::string &str,
                               const std::string &needles = " ");
template <typename Forward>
std::string join(std::string sep, Forward beg, Forward end) {
  std::string res;
  for (auto itr = beg; itr != end; ++itr) {
    if (not res.empty()) {
      res += sep;
    }
    res += *itr;
  }
  return res;
}
}

#endif
