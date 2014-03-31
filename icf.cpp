#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <assert.h>
#include "icf.hpp"

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
    if (start == string::npos || (sharpen && line[start] == '#')) {
      return "";
    } // here we must have something in the line/string
    size_t stop = sharpen ? line.find_last_of("#") // returns either # pos or npos
                          : string::npos;
    if (stop != string::npos) {
      stop--;
    }
    stop = line.find_last_not_of(" \t\n\r", stop);
    if (stop == string::npos) { return ""; } // this cannot happen
    return line.substr(start, stop+1-start);
  }

  std::vector<std::string> tokenize(const std::string& haystack, const std::string& needles, unsigned maxSplit, bool awk)
  {
    std::vector<std::string> toks;
    size_t p0 = haystack.find_first_not_of(needles);
    size_t pn = haystack.find_first_of(needles, p0);
    if (p0 == std::string::npos || pn == std::string::npos) {
      toks.push_back(haystack);
      return toks;
    }
    while(1) {
    if (pn != p0 or ! awk) {  // contigous seps treated as 1 in awk mode
        toks.push_back(haystack.substr(p0,pn-p0));
    }
    if (maxSplit and toks.size() >= maxSplit) { break; }
      if (pn >= haystack.size())                { break; }
      p0 = pn + 1;
      pn = haystack.find_first_of(needles, p0);
      if (pn == std::string::npos)
        pn = haystack.size();
    }
    return toks; // efficiency per NRVO
  }

  std::vector<std::string> split(const std::string& str, const std::string& needles = " ") {
    return tokenize(str, needles, 0, true);
  }

  template <typename Forward>
  std::string join(std::string sep, Forward beg, Forward end) {
    std::string res;
    for (auto itr = beg; itr != end; ++itr) {
      if (not res.empty()) { res += sep; }
      res += *itr;
    }
    return res;
  }
}

Icf::Icf(const char* fname, const std::set<std::string> &ancestors)
{
  auto fitr = ancestors.find(string(fname));
  if (fitr != ancestors.end()) { std::cerr << " --- bad icf with circular include: " << fname << std::endl; exit(-1); }

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
        Icf imported(inc.c_str(), ans);
//      auto itr = imported.store_.begin();
//      for (; itr != imported.store_.end(); ++itr) {
//        store_[itr->first] = itr->second; // XXX this needs update, simply replacing is not right, we need to merge
//      }
        for (auto& kv : imported.groups_) {
          groups_[kv.first] = kv.second;
        }
        mergeStore(imported.store_);  // do after groups_ updated as it can be affected by groups_
    } else if  (trimline[0] == '#' and trimline[1] == 'g') { // start groupdef
        if (not ingroupdef.empty()) { std::cerr << "-- unexpected #groupdef (with def of group '" << ingroupdef << "' in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        string inc = trimline.substr(sizeof(detail::GROUPDEF));  // start from the char right after first ' ' or '\t'
        ingroupdef = detail::trim(inc);
        auto parts = detail::split(ingroupdef);
        if (parts.size() > 1) { std::cerr << "-- #groupdef with more than 1 words in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
    } else if  (trimline[0] == '#' and trimline[1] == 'e') { // end groupdef
        if (ingroupdef.empty()) { std::cerr << "-- unexpected #endgroupdef in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        ingroupdef = "";
    } else if (not ingroupdef.empty()) {
        auto parts = detail::split(trimline);
        if (parts.size() > 1) { std::cerr << "-- #groupdef '" << ingroupdef << "' with more than 1 elements in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        if (not groups_[ingroupdef].emplace(trimline).second) { std::cerr << "-- #groupdef '" << ingroupdef << "' with duplicate element in " << fname << ':' << lineno << ": " << line << std::endl; }
    } else {
        auto parts = detail::split(trimline);
        if (parts.size() < 3) { std::cerr << "-- bad icf line with less than 3 parts in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
        auto sections = parts[0];
        auto groupdesc = parts[1];  // groupdesc may not be #groupdefed, but rather be either symbol (list) or #groupdef combined
        parts.erase(parts.begin(), parts.begin()+2);
        for (auto& param : parts) {
            auto kv = detail::split(param, "=");
            if (kv.size() != 2) { std::cerr << "-- bad kv pair definition '" << param << "' in " << fname << ':' << lineno << ": " << line << std::endl; exit(-1); }
            IcfKey k = make_pair(sections, kv[0]);
            // XXX bad: need to keep original group name here too to help find dups
            // if (groups_.find(k) != groups.end()) { std::cerr << "-- dup kv pair definition '" << sections << ':' << kv.first << "' in " << fname << ':' << lineno << ": " << line << std::endl; }
            auto symbols = setByName(groupdesc);
            if (symbols.empty()) { symbols.insert(groupdesc); } // single symbol XXX extend to comma (,) separated symbols?
            for (auto symbol : symbols) {
                //std::vector<WithEnv>& valueRecords = storeHelper_[k][symbol];
                record(k, symbol, kv[1], groupdesc);
                /*
                auto& valueRecords = storeHelper_[k][symbol];
                if (not valueRecords.empty()) {
                  std::string val, env;
                  std::tie(val, env) = valueRecords.back();
                  store_[k][val].erase(symbol);  // doesn't matter if val is same as kv[1], we may need to update with new context anyway
                }
                valueRecords.push_back(make_pair(kv[1], groupdesc));

                //store_[k][kv[1]].insert(make_pair(symbol, groupdesc));
                store_[k][kv[1]][symbol] = groupdesc;
                */
            }
        }
    }
  }
  
  combineSets();
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

void Icf::combineSets()
{
  std::map<std::string, std::set<std::string> > prefixes; // prefix -> group names
  // intersection combinations of 2 pairs -- I don't think differences or 3+ combinations are useful
  for (auto& kv1 : groups_) {
    for (auto& kv2 : groups_) {
      if (kv1.first < kv2.first) {
        Set common;
        std::set_intersection(begin(kv1.second), end(kv1.second), begin(kv2.second), end(kv2.second), inserter(common, begin(common)));
        if (common.empty()) {
          std::set_union(begin(kv1.second), end(kv1.second), begin(kv2.second), end(kv2.second), inserter(common, begin(common)));
          extraGroups_[kv1.first + "++" + kv2.first] = common;
        }

        std::string pre = findPrefix(kv1.first, kv2.first);
        if (not pre.empty()) {
          prefixes[pre].insert(kv1.first);
          prefixes[pre].insert(kv2.first);
        }
      }
    }
  }

  // exhaustive group combination is exponential, we instead do group name common prefix
  for (auto& kv : prefixes) {
    Set all;
    for (auto& s : kv.second) {
      auto& g = groups_[s];
      all.insert(begin(g), end(g));
    }
    extraGroups_[kv.first + "*"] = all;
  }
}

void Icf::record(const IcfKey k, std::string sym, std::string value, std::string env) 
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

void Icf::mergeStore(const Store& other)
{
  // Store: key -> value  -> { symbol : context }
  for (auto& kv : other) {
    for (auto& vs : kv.second) {
      for (auto &se : vs.second) {
        record(kv.first, se.first, vs.first, se.second);
        /*
        auto& valueRecords = storeHelper_[kv.first][se.first];
        if (not valueRecords.empty()) {
          std::string val, env;
          std::tie(val, env) = valueRecords.back();
          store_[kv.first][val].erase(se.first);  // doesn't matter if val is same as kv[1], we may need to update with new context anyway
        }
        valueRecords.push_back(make_pair(vs.first, se.second));

        store_[kv.first][vs.first][se.first] = se.second;
        */
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
  Set gdc; // gdesc combined
  Set gdcNames;
  for (auto& g : gdesc) {
    Groups::const_iterator itr = groups_.find(g);
    if (itr != groups_.end()) {
      gdcNames.insert(g);
      for (const auto& sym : itr->second) {
        gdc.insert(sym);
      }
    }
  }
  // gdesc combined is checked twice: maybe GROUP_* look better than GROUP_1++GROUP_2++GROUP_3++GROUP_4
  if (gdcNames.size() < 4 && s == gdc) {
    return detail::join("++", begin(gdcNames), end(gdcNames));
  }
  for (auto& kv : extraGroups_) {
    if (s == kv.second) { return kv.first; }
  }
  for (auto& kv : groups_) {    // with small diffs
    string desc = kv.first;
    Set myExtra, grExtra;
    std::set_difference(begin(s), end(s), begin(kv.second), end(kv.second), inserter(myExtra, begin(myExtra)));
    std::set_difference(begin(kv.second), end(kv.second), begin(s), end(s), inserter(grExtra, begin(grExtra)));
    if (myExtra.size() == 0 && grExtra.size() < 3) {
       for (auto& e : grExtra) { desc += "-" + e; }
       return desc;
    }
    if (grExtra.size() == 0 && myExtra.size() < 3) {
       for (auto& e : myExtra) { desc += "+" + e; }
       return desc;
    }
  }
  if (gdcNames.size() >= 4 && s == gdc) {
    return detail::join("++", begin(gdcNames), end(gdcNames));
  }

  std::string syms;
  for (auto& sym : s) {
    if (not syms.empty()) { syms += ","; }
    syms += sym;
  }
  return syms;
}

Icf Icf::diff(const Icf& newicf) const
{
  Icf cmp;
  auto& old = storeHelper_;
  auto& neu = newicf.storeHelper_;
  // Store: key -> value  -> { symbol : context }
  // StoreHelper: key -> symbol -> [ value : context ]
  for (auto& ks : old) {
    auto k2 = neu.find(ks.first);
    if (k2 == neu.end()) {  // no such key in neu
    } else {
      for (auto& sv : ks.second) {
        auto s2 = k2->second.find(sv.first);
        if (s2 == k2->second.end()) { // no symbol in neu with such key
        } else {
          auto& oldvec = sv.second;
          auto& neuvec = s2->second;
          assert (not oldvec.empty() and not neuvec.empty());
          auto& oldv = oldvec.back().first;
          auto& neuv = neuvec.back().first;
          if (oldv != neuv) {
              cmp.record(ks.first, sv.first, oldv + " <--> " + neuv, oldvec.back().second); // maybe using neuv's context?
          }
        }
      }
    }
  }
  cmp.groups_ = groups_;  // using old group_
  return cmp;
}

void Icf::output_to(std::ostream& output) const
{
  for (auto& kv : store_) {
    for (auto& vs : kv.second) {
      Set syms, groupdescs;
      for (auto& se : vs.second) {
        syms.insert(se.first);
        groupdescs.insert(se.second);
      }
      output << '[' << kv.first.first << ':' << kv.first.second << "] = " << vs.first << " : " << groupDesc(syms, groupdescs) << '\n';
    }
  }
  typedef std::unordered_map<IcfKey, std::map<std::string, Set>, Hasher, Equaler> Store;
}

std::ostream& operator<< (std::ostream& o, const Icf& c) {
  c.output_to(o);
  return o;
}

