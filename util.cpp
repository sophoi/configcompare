#include "util.hpp"

using namespace std;

namespace sophoi {
string trim(const string &line, bool sharpen) {
  size_t start = line.find_first_not_of(" \t\n\r");
  if (start == string::npos || (sharpen && line[start] == '#')) {
    return "";
  } // here we must have something in the line/string
  size_t stop = sharpen ? line.find_last_of("#") // returns either # pos or npos
                        : string::npos;
  if (stop != string::npos) {
    stop--;
  }
  stop = line.find_last_not_of(" \t\n\r", stop);
  if (stop == string::npos) {
    return "";
  } // this cannot happen
  return line.substr(start, stop + 1 - start);
}

std::vector<std::string> tokenize(const std::string &haystack,
                                  const std::string &needles, unsigned maxSplit,
                                  bool awk) {
  std::vector<std::string> toks;
  if (haystack.empty()) {
    return toks;
  }
  size_t p0 = haystack.find_first_not_of(needles);
  size_t pn = haystack.find_first_of(needles, p0);
  if (p0 == std::string::npos || pn == std::string::npos) {
    toks.push_back(haystack);
    return toks;
  }
  while (1) {
    if (pn != p0 or !awk) { // contigous seps treated as 1 in awk mode
      toks.push_back(haystack.substr(p0, pn - p0));
    }
    if (maxSplit and toks.size() >= maxSplit) {
      break;
    }
    if (pn >= haystack.size()) {
      break;
    }
    p0 = pn + 1;
    pn = haystack.find_first_of(needles, p0);
    if (pn == std::string::npos)
      pn = haystack.size();
  }
  return toks;
}

std::vector<std::string> split(const std::string &str,
                               const std::string &needles) {
  return tokenize(str, needles, 0, true);
}
}

