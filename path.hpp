#ifndef __PATH_FINDER_HPP__
#define __PATH_FINDER_HPP__

#include <string>
#include <vector>
#include <map>
#include <unordered_set>

class PathFinder
{
  std::string ext_;
  std::string extra_;
  std::string cwd_;
  std::map<std::string, std::vector<std::string>> extPaths_;
  std::unordered_set<std::string> xlFiles_;
public:
  PathFinder(std::string path, const std::string& env = "");
  std::string locate(std::string fname);
  bool ignore(std::string fname);
};

#endif
