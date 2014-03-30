#ifndef __ICF_HPP__
#define __ICF_HPP__

#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <map>

class Icf
{
  Icf();
  Icf(const Icf&);
  Icf& operator=(const Icf&);

public:
  typedef std::set<std::string> Set;
  // defined sets (and their intersections?)
  typedef std::map<std::string, Set> Groups;          // name -> set of symbols

  typedef std::pair<std::string, std::string> IcfKey;        // (sections, param key)
  struct Hasher { size_t operator()(const IcfKey& k) const { return std::hash<std::string>()(k.first) * 71 + std::hash<std::string>()(k.second); } };
  struct Equaler { size_t operator()(const IcfKey& a, const IcfKey& b) const { return a.first == b.first and a.second == b.second; } };
  typedef std::unordered_map<IcfKey, std::map<std::string, Set>, Hasher, Equaler> Store; // key -> value -> symbol set

  Icf(const char* fname, const std::set<std::string> &ancestors = std::set<std::string>());
  void combineSets();

  Set setByKeyValue(IcfKey k, std::string v);
  Set setByName(std::string name) { auto itr = groups_.find(name); if (itr != groups_.end()) { return itr->second; } else { return Set(); } }
  std::string groupDesc(Set) const;   // predictable nearest desc of Set: a defined set name, or one with minor fixup
  static std::tuple<Set,Set,Set> setRelation(Set l, Set r);   // (l-r, l^r, r-l)

  void output_to(std::ostream& output) const;

private:
  Store store_;
  Groups groups_;
  Groups extraGroups_;
};

std::ostream& operator<< (std::ostream&, const Icf&);

#endif
