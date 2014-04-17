#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <random>
#include <assert.h>
#include "util.hpp"
#include "icf.hpp"
#include "path.hpp"

using namespace std;

namespace detail {
  char INCLUDE[] = "#include";
  char GROUPDEF[] = "#groupdef";
  char ENDGROUPDEF[] = "#endgroupdef";
  std::map<char *,size_t> sharps
        = { {INCLUDE,     sizeof(INCLUDE)-1},  // sizeof includes \0
            {GROUPDEF,    sizeof(GROUPDEF)-1},
            {ENDGROUPDEF, sizeof(ENDGROUPDEF)-1} };
  string trim(const string &line, bool sharpen=false)
  {
    size_t start = line.find_first_not_of(" \t\n\r");
    for (auto& kv : sharps) {
      //std::cout << " looking for " << kv.first << " (" << sizeof(kv.first) << ',' << kv.second << ") in: " << line << '\n';
      if (line.find(kv.first, start) != string::npos) {
        char next = line[start + kv.second];
//        if (next == ' ' or next == '\t') {
        return line.substr(start);  // right not trimmed, but ok
//        } else { return ""; }
      }
    }
    if (start == string::npos || (sharpen && line[start] == '#') || (sharpen && line[start] == '/' && line[start+1] == '/')) {
      return "";
    } // here we must have something in the line/string
    size_t stop = sharpen ? line.find_first_of("#") // returns either # pos or npos
                          : string::npos;
    if (sharpen) {
      auto ss = line.find_first_of("//"); // returns either # pos or npos
      if (ss != string::npos && ss < stop)
        stop = ss;
    }
    if (stop != string::npos) {
      stop--;
    }
    stop = line.find_last_not_of(" \t\n\r", stop);
    if (stop == string::npos) { return ""; } // this cannot happen
    return line.substr(start, stop+1-start);
  }
}

std::vector<std::string> getGrpNamCombs()
{
  static std::vector<std::string> gnc;
  if (gnc.size() > 0) { return gnc; }
  std::vector<std::string> adjs = { "FAT", "BAD", "RED", "GREEN", "BLUE", "RED", "MAD", "HAPPY", "SAD", "DRY", };
  std::vector<std::string> noun = { "CAT", "DOG", "COW", "APPLE", "DATE", "MOON", "SUN", "MAN", "BOY", "GIRL", };
  for (auto& a : adjs) for (auto& n : noun) {
    gnc.push_back(a + "_" + n);  // GRP@4_MAD_COW[_1]
  }
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::shuffle(begin(gnc), end(gnc), gen);
  return gnc;
}

Icf::Icf(const char* fn, const std::set<std::string> &ancestors, std::shared_ptr<PathFinder> pf)
{
  if (not pf.get()) {
    pf_.reset(new PathFinder(fn));
  } else {
    pf_ = pf;
  }
  if (pf_->ignore(fn)) { return; }
  std::string fname = pf_->locate(fn);
  auto fitr = ancestors.find(string(fname));
  if (fitr != ancestors.end()) { std::cerr << " --- bad icf with circular include: " << fname << std::endl; exit(-1); }
  if (ancestors.size() > 100) { std::cerr << " --- suspicious icf include depth: " << ancestors.size() << std::endl; }

  ifstream infile(fname);
  // XXX look for file in other paths defined in env{ICFPATH}; ancestors logic may need change to use canonical path; also update fname to be more exact?
  if (infile.fail()) { std::cerr << " --- cannot read file: " << fname << std::endl; exit(-1); }

  unsigned lineno(0);

  std::string ingroupdef, line;
  while (getline(infile, line))
  {
    lineno++;
    string trimline = detail::trim(line, true);
    if (trimline.empty()) { continue; }
//    std::cout << "---- on #" << lineno << ": " << trimline << std::endl;

    if (trimline[0] == '#' and trimline[1] == 'i') { // including a .icf file
        if (not ingroupdef.empty()) { std::cerr << "-- unexpected #include inside groupdef in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        string inc = trimline.substr(sizeof(detail::INCLUDE));  // start from the char right after first ' ' or '\t'
        inc = detail::trim(inc);
        if (inc.empty()) { std::cerr << "-- empty include in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        std::set<std::string> ans = ancestors;
        ans.insert(string(fname));
        Icf imported(inc.c_str(), ans, pf_);
//      auto itr = imported.store_.begin();
//      for (; itr != imported.store_.end(); ++itr) {
//        store_[itr->first] = itr->second; // XXX this needs update, simply replacing is not right, we need to merge
//      }
        for (auto& kv : imported.groups_) { groups_[kv.first] = kv.second; }
        mergeStore(imported.store_);  // do after groups_ updated as it can be affected by groups_
        for (auto& i : imported.icfSections_) { icfSections_.insert(i); }
    } else if  (trimline[0] == '#' and trimline[1] == 'g') { // start groupdef
        if (not ingroupdef.empty()) { std::cerr << "-- unexpected #groupdef (with def of group '" << ingroupdef << "' in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        string inc = trimline.substr(sizeof(detail::GROUPDEF));  // start from the char right after first ' ' or '\t'
        ingroupdef = detail::trim(inc);
        auto parts = sophoi::split(ingroupdef);
        if (parts.size() > 1) { std::cerr << "-- #groupdef with more than 1 words in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
    } else if  (trimline[0] == '#' and trimline[1] == 'e') { // end groupdef
        if (ingroupdef.empty()) { std::cerr << "-- unexpected #endgroupdef in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        ingroupdef = "";
    } else if (not ingroupdef.empty()) {
        auto parts = sophoi::split(trimline);
        if (parts.size() > 1) { std::cerr << "-- #groupdef '" << ingroupdef << "' with more than 1 elements in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        if (not groups_[ingroupdef].emplace(trimline).second) {
          std::cerr << "-- #groupdef '" << ingroupdef << "' with duplicate element in " << fname << ':' << lineno << ": " << line << std::endl;
        }
    } else {
        auto parts = sophoi::split(trimline);
        if (parts.size() < 3) { std::cerr << "-- bad icf line with less than 3 parts in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        auto sections = parts[0];
        auto groupdesc = parts[1];  // groupdesc may not be #groupdefed, but rather be either symbol (list) or #groupdef combined
        parts.erase(parts.begin(), parts.begin()+2);
        for (auto& param : parts) {
            auto kv = sophoi::split(param, "=");
            if (kv.size() != 2) { std::cerr << "-- bad kv pair definition '" << param << "' in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
            IcfKey k = make_pair(sections, kv[0]);
            icfSections_.emplace(sections);
            // XXX bad: need to keep original group name here too to help find dups
            // if (groups_.find(k) != groups.end()) { std::cerr << "-- dup kv pair definition '" << sections << ':' << kv.first << "' in " << fname << ':' << lineno << ": " << line << std::endl; }
            auto symbols = setByName(groupdesc);
            if (symbols.empty()) { symbols.insert(groupdesc); } // single symbol XXX extend to comma (,) separated symbols?
            for (auto symbol : symbols) {
                record(k, symbol, kv[1], groupdesc);
            }
        }
    }
  }
  
  trickleDown();
  combineSets();
  for (auto& sections : icfSections_) {  // header:p1,p3,p2 becomes header => { p1,p3,p2 : [ p1, p2, p3 ] }
    auto hp = sophoi::split(sections,":");
    if (hp.size() != 2) { continue; }
    auto ps = sophoi::split(hp[1],",");
    std::sort(begin(ps),end(ps));
    icfSets_[hp[0]][hp[1]] = ps;
  }

  grpNamCombs_ = getGrpNamCombs();
}

std::string findPrefix(std::string str1, std::string str2)
{
  size_t s1 = str1.length(), s2 = str2.length();
  size_t i, minlen = s1 < s2 ? s1 : s2;
  for (i = 0; i < minlen && str1[i] == str2[i]; ++i) {}
  if (i > 3) {  // XXX this can be configurable
    return str1.substr(0, i);
  } else {
    return "";
  }
}

/* online                         MY_GROUP_1      Venues=ARCA enable=true id=1
 * online:account=3,strategy=2    MY_GROUP_OTC    Venues=BATS
 * should be tricked down to
 * online:account=3,strategy=2    MY_GROUP_1      enable=true id=1
 */
void Icf::trickleDown()
{
}

// not a good idea to use combinations; heuristics using seen header:sections in both files
std::vector<Icf::IcfKey> Icf::subkeys(IcfKey k, const SectionSets& aset) const
{
    using IK = Icf::IcfKey;
    std::vector<IK> ret;
    auto sections = k.first;
    auto param = k.second;
    auto hp = sophoi::split(sections,":");  // header:p1=1,p2=2
    if (hp.size() != 2) return ret;
    auto ps = sophoi::split(hp[1],",");
    std::sort(begin(ps), end(ps));
    const SectionSets * both[] = { &icfSets_, &aset };
    for (auto icfS = begin(both); icfS != end(both); ++icfS) {
      auto icfset = (*icfS)->find(hp[0]);
      if (icfset != (*icfS)->end()) {
        for (auto& ts : icfset->second) {
          auto text = ts.first;
          auto secs = ts.second;
          if (text == hp[1]) { continue; }
          if (std::includes(begin(ps), end(ps), begin(secs), end(secs))) {
            ret.push_back(make_pair(hp[0] + ":" + text, param));
          }
        }
      }
    }
    auto commas = [](std::string s){ return std::count(begin(s), end(s), ','); };
    sort(begin(ret), end(ret), [&commas](IK a, IK b)  // sort by descending # of ,
                               { return commas(a.first) > commas(b.first); });
    ret.push_back(make_pair(hp[0], param));
    return ret;
}

// http://stackoverflow.com/questions/16182958/how-to-compare-two-stdset
void Icf::combineSets()
{
  Set dftGrp;
  for (auto& kv : groups_) {
    dftGrp.insert(begin(kv.second), end(kv.second));
  }

  std::map<std::string, std::set<std::string> > prefixes; // prefix -> group names
  // intersection combinations of 2 pairs -- I don't think differences or 3+ combinations are useful
  for (auto& kv1 : groups_) {
    for (auto& kv2 : groups_) {
      if (kv1.first < kv2.first) {
        Set common;
        std::set_intersection(begin(kv1.second), end(kv1.second), begin(kv2.second), end(kv2.second), inserter(common, begin(common)));
        if (common.empty()) {
          std::set_union(begin(kv1.second), end(kv1.second), begin(kv2.second), end(kv2.second), inserter(common, begin(common)));
          if (common != dftGrp) { extraGroups_[kv1.first + "#" + kv2.first] = common; }
        }

        std::string pre = findPrefix(kv1.first, kv2.first);
        if (not pre.empty()) {
          prefixes[pre].insert(kv1.first);
          prefixes[pre].insert(kv2.first);
        }
      }
    }
  }

  groups_["DEFAULT"] = dftGrp;

  // exhaustive group combination is exponential, we instead do group name common prefix
  for (auto& kv : prefixes) {
    Set all;
    for (auto& s : kv.second) {
      auto& g = groups_[s];
      all.insert(begin(g), end(g));
    }
    if (all != dftGrp) { extraGroups_[kv.first + "*"] = all; }
  }
}

void Icf::record(const IcfKey& k, std::string sym, std::string value, std::string env) 
{
  auto& valueRecords = storeHelper_[k][sym];
  if (not valueRecords.empty()) {
    std::string val, env;
    std::tie(val, env) = valueRecords.back();
    store_[k][val].erase(sym);  // doesn't matter if val is same as value, we may need to update with new context anyway
  }
  valueRecords.push_back(make_pair(value, env));

  store_[k][value][sym] = env;
}

Icf::IcfKey Icf::prek(const IcfKey& k, std::string prefix) const
{
  IcfKey ret = { k.first, prefix+k.second };
  return ret;
}

void Icf::mergeStore(const Store& other)
{
  // Store: key -> value  -> { symbol : context }
  for (auto& kv : other) {
    for (auto& vs : kv.second) {
      for (auto &se : vs.second) {
        record(kv.first, se.first, vs.first, se.second);
      }
    }
  }
}

// *predictable* nearest desc of Set: a defined set name, or one with minor fixup
std::string Icf::groupDesc(const Set& s, const Set& gdesc) const
{
  for (auto& kv : groups_) {    // exact match first
    if (s == kv.second) { return kv.first; }
  }
  for (auto& kv :seenGroups_) { // combined groups, seen before
    if (s == kv.second) { return kv.first; }
  }
  Set gdc; // gdesc combined
  Set gdcNames;
  for (auto& g : gdesc) {
    Groups::const_iterator itr = groups_.find(g);
    if (itr != groups_.end()) {
      if (itr->second.size() > s.size()) { // s cannot be A++B because of size
        gdc.clear(); gdcNames.clear(); break;
      }
      gdcNames.insert(g);
      for (const auto& sym : itr->second) {
        gdc.insert(sym);
      }
      if (gdc.size() > s.size()) {
        gdc.clear(); gdcNames.clear(); break;
      }
    } // XXX else "unexpected error?"
  }
  // gdesc combined is checked twice: maybe GROUP_* look better than GROUP_1++GROUP_2++GROUP_3++GROUP_4
  if (! gdc.empty() && gdcNames.size() < 4 && s == gdc) {
    auto newname = sophoi::join("++", begin(gdcNames), end(gdcNames));
    seenGroups_[newname] = s;
    return newname;
  }
  for (auto& kv : extraGroups_) {
    if (s == kv.second) { seenGroups_[kv.first] = s; return kv.first; }
  }
  for (auto& kv : groups_) {    // with small diffs
    string desc = kv.first;
    Set myExtra, grExtra;
    std::set_difference(begin(s), end(s), begin(kv.second), end(kv.second), inserter(myExtra, begin(myExtra)));
    std::set_difference(begin(kv.second), end(kv.second), begin(s), end(s), inserter(grExtra, begin(grExtra)));
    if (myExtra.size() == 0 && grExtra.size() < 3) {
       for (auto& e : grExtra) { desc += "-" + e; }
       seenGroups_[desc] = s;
       return desc;
    }
    if (grExtra.size() == 0 && myExtra.size() < 3) {
       for (auto& e : myExtra) { desc += "+" + e; }
       seenGroups_[desc] = s;
       return desc;
    }
  }
  if (! gdc.empty() && gdcNames.size() >= 4 && s == gdc) {
    auto newname = sophoi::join("++", begin(gdcNames), end(gdcNames));
    seenGroups_[newname] = s;
    return newname;
  }

  if (s.size() < 4) { return sophoi::join(",", begin(s), end(s)); }

  auto grpnam = nextGrpName(s.size());
  seenGroups_[grpnam] = s;
  return grpnam;
}

Icf Icf::diff(const Icf& newicf, bool reverse) const
{
  Icf cmp;
  cmp.grpNamCombs_ = getGrpNamCombs();
  auto& old = storeHelper_;
  auto& neu = newicf.storeHelper_;
  std::string ind = reverse ? "+" : "-";
  // Store: key -> value  -> { symbol : context }
  // StoreHelper: key -> symbol -> [ value : context ]
  for (auto& ks : old) {
    auto k2 = neu.find(ks.first);
    if (k2 == neu.end()) {  // no such key in neu
      auto subs = subkeys(ks.first, newicf.icfSets_);
      std::set<std::string> foundSyms;
      for (auto& sub : subs) {
        auto k3 = neu.find(sub);
        if (k3 == neu.end()) continue;  // not even this sub-key
        for (auto& sv : ks.second) {
          auto s3 = k3->second.find(sv.first);
          if (s3 == k3->second.end()) continue; // not this sub-key for this symbol
          auto fs = foundSyms.find(sv.first);
          if (fs != foundSyms.end()) continue; // already found with longer sub-keys
          if (not foundSyms.insert(sv.first).second) std::cerr << "!! symbol found many times in diff sub-key lookup: " << sv.first << std::endl;
          auto& oldvec = sv.second; auto& neuvec = s3->second;
          assert (not oldvec.empty() and not neuvec.empty());
          auto& oldv = oldvec.back().first; auto& neuv = neuvec.back().first;
          if (oldv != neuv) cmp.record(ks.first, sv.first, reverse ? neuv + "<-*>" + oldv
                                                                   : oldv + "<-*>" + neuv, oldvec.back().second);
        }
      }
      for (auto& sv : ks.second) {
          assert (not sv.second.empty());
          auto fs = foundSyms.find(sv.first);
          if (fs != foundSyms.end()) continue;
          cmp.record(prek(ks.first,ind), sv.first, sv.second.back().first, sv.second.back().second);
      }
    } else {
      for (auto& sv : ks.second) {
        auto s2 = k2->second.find(sv.first);
        if (s2 == k2->second.end()) { // no symbol in neu with such key
          assert (not sv.second.empty());
          cmp.record(prek(ks.first,ind), sv.first, sv.second.back().first, sv.second.back().second);
        } else if (not reverse) {
          auto& oldvec = sv.second;
          auto& neuvec = s2->second;
          assert (not oldvec.empty() and not neuvec.empty());
          auto& oldv = oldvec.back().first;
          auto& neuv = neuvec.back().first;
          if (oldv != neuv) {
              cmp.record(ks.first, sv.first, oldv + "<->" + neuv, oldvec.back().second); // maybe using neuv's context?
          }
        }
      }
    }
  }
  cmp.groups_ = groups_;  // using old group_
  return cmp;
}

std::string Icf::nextGrpName(unsigned sz) const
{
  unsigned q = grpNamCounter_ / grpNamCombs_.size();
  auto nam = "GRP@" + std::to_string(sz) + "_" + grpNamCombs_[grpNamCounter_ % grpNamCombs_.size()];
  grpNamCounter_ ++;
  if (q > 0) { nam += "_" + std::to_string(q); }
  custGrpNames_.insert(nam);
  return nam;
}

void Icf::output_to(std::ostream& output) const
{
  char buf[512];
//  typedef std::map<IcfKey, std::map<std::string, Set>, [](const IcfKey& l, const IcfKey& r) { return l.first < r.first or l.first == r.first and l.second < r.second }> SortedStore;
  typedef std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> SortedStore;
  SortedStore ss;
  for (auto& kv : store_) {
    for (auto& vs : kv.second) {
      Set syms, groupdescs;
      for (auto& se : vs.second) {
        syms.insert(se.first);
        groupdescs.insert(se.second);
      }
      ss[kv.first.first][groupDesc(syms, groupdescs)][kv.first.second] = vs.first;
// piecemeal printout
//      snprintf(buf, sizeof(buf), "%-30s  %-16s  %s=%s\n", kv.first.first.c_str(), groupDesc(syms, groupdescs).c_str(), kv.first.second.c_str(), vs.first.c_str());
//      output << buf;

//      output << '[' << kv.first.first << ':' << kv.first.second << "] = " << vs.first << " : " << groupDesc(syms, groupdescs) << '\n';
    }
  }
  for (auto& kg : ss) {
    for (auto& gv : kg.second) {
      snprintf(buf, sizeof(buf), "%-30s  %-16s", kg.first.c_str(), gv.first.c_str()); output << buf;
      for (auto& vs : gv.second) {
        snprintf(buf, sizeof(buf), "  %s=%s", vs.first.c_str(), vs.second.c_str()); output << buf;
      }
      output << std::endl;
    }
  }

  if (custGrpNames_.size() > 0) { output << std::endl; }
  for (auto& grp : custGrpNames_) {
    if (seenGroups_.find(grp) == seenGroups_.end()) {
      std::cerr << "custGrpName '" << grp << " is not set yet used?" << std::endl;
      continue;
    }
    auto& s = seenGroups_[grp];
    output << "> '" << grp << "': " << sophoi::join(",",begin(s),end(s)) << std::endl;
  }
}

std::ostream& operator<< (std::ostream& o, const Icf& c) {
  c.output_to(o);
  return o;
}

