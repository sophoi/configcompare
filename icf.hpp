#ifndef __ICF_HPP__
#define __ICF_HPP__

#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>

class PathFinder;
class Icf
{
  Icf() {}
  Icf& operator=(const Icf&) = delete;

public:
  typedef std::set<std::string> Set;
  typedef std::pair<std::string, std::string> WithEnv;
  typedef std::map<std::string, std::string> SetWithEnv;  // [symbol/value] and context of definition (group def for now)
  // defined sets (and their intersections?)
  typedef std::map<std::string, Set> Groups;          // name -> set of symbols

  typedef std::pair<std::string, std::string> IcfKey;        // (sections, param key)
  struct Hasher { size_t operator()(const IcfKey& k) const { return std::hash<std::string>()(k.first) * 71 + std::hash<std::string>()(k.second); } };
  struct Equaler { size_t operator()(const IcfKey& a, const IcfKey& b) const { return a.first == b.first and a.second == b.second; } };
  // context is the group defined; context can be further explored to even include change trace, then we need to mark a value as active or not, seems not wort it yet
  // IcfKey.symbol can have multiple values in key->value(s)->{}, what's the final value then? need to remove other value entries on new value
  // key -> symbol -> [ value  : context ]
  typedef std::unordered_map<IcfKey, std::map<std::string, std::vector<WithEnv> >, Hasher, Equaler> StoreHelper;
  // key -> value  -> { symbol : context }  ==> find set of symbols that have (key,value) ==> describe such symbols by predefined group names with help of context
  typedef std::unordered_map<IcfKey, std::map<std::string, SetWithEnv>, Hasher, Equaler> Store; // key -> value -> symbol set
  // header => { section_string : [ sorted sections ] }
  typedef std::map<std::string, std::map<std::string, std::vector<std::string>>> SectionSets;

  std::vector<IcfKey> subkeys(IcfKey k, const SectionSets& aset=SectionSets()) const;
  Icf(const char* fname, const std::set<std::string> &ancestors = std::set<std::string>(), std::shared_ptr<PathFinder> pf = NULL);
  void trickleDown();
  void combineSets();
  void mergeStore(const Store&);
  Icf diff(const Icf&, bool reverse=false) const;

  Set setByKeyValue(IcfKey k, std::string v);
  Set setByName(std::string name) { auto itr = groups_.find(name); if (itr != groups_.end()) { return itr->second; } else { return Set(); } }
  std::string groupDesc(const Set&, const Set &) const;   // predictable nearest desc of Set: a defined set name, or one with minor fixup
  static std::tuple<Set,Set,Set> setRelation(Set l, Set r);   // (l-r, l^r, r-l)

  void output_to(std::ostream& output) const;

private:
  void record(const IcfKey& k, std::string sym, std::string value, std::string env);
  IcfKey prek(const IcfKey& k, std::string prefix) const;

private:
  Store store_;
  StoreHelper storeHelper_;
  Groups groups_;
  Groups extraGroups_;
  std::string nextGrpName(unsigned sz) const;
  std::vector<std::string> grpNamCombs_;
  mutable unsigned grpNamCounter_ = 0;
  mutable Groups seenGroups_;
  mutable Set custGrpNames_;
  std::shared_ptr<PathFinder> pf_;
  Set icfSections_;
  SectionSets icfSets_;
};

std::ostream& operator<< (std::ostream&, const Icf&);

#endif
