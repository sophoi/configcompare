#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <iostream>
#include <assert.h>
#include <libgen.h> // basename,dirname
#include "util.hpp"
#include "path.hpp"

bool endsWith(std::string heystack, std::string straw) {
  auto hl = heystack.length();
  auto sl = straw.length();
  if (hl < sl) { return false; }
  return heystack.substr(hl-sl) == straw;
}

/* find extra files (.new etc) and historical files (.gz etc) based on cwd
 * 1. if file has extra extension like .new or .nyc, and if not found in cwd, look under $CFGPATH.new $CFGPATH.nyc
 * 2. further look for file (without extra ext) under $CFGPATH
 * 3. for historical files look solely under $CFGPATH.gz with no falling back on default $CFGPATH
 * 4. another choice is CFGPATH=/path/to/default;/another/default/;new,nyc:/ext/path/1st:/ext/path/2nd;gz,bz2:/historical
 * 5. path utilities like yyyymmdd can be plugged in: /yyyymmdd.=>/2014/03/31/20140331.
 * 6. translation of ex. .icf included inside a .gz or .new file is called for in user code: PathFinder::find(withExt=.gz, withExtTranslator={yyyymmdd=>}
 */
PathFinder::PathFinder(std::string fname, const std::string& envstr)
{
//  std::cout << "--- PathFinder ctr(" << fname << ")\n";
  std::string env = envstr;
  if (env.empty()) {
    char * tmp = getenv("CFGPATH");
    if (tmp) { env = tmp; }
  }
  char pathbuf[MAXPATHLEN];
  auto envparts = sophoi::split(env, ";");
  for (auto& ep : envparts) {
    auto pathparts = sophoi::split(ep, ":");
//    std::cerr << pathparts.size() << pathparts[0] << "." << std::endl;
    if (pathparts.size() < 1) { std::cerr << "-- bad CFGPATH=" << env.c_str() << " -- " << pathparts.size() << " parts in '" << ep.c_str() << "'\n"; exit(-1); }
    if (pathparts.size() == 1) {
      auto p = pathparts[0];
      char *full = realpath(p.c_str(), pathbuf);
      if (full == NULL) { std::cerr << "-- bad path in CFGPATH=" << env.c_str() << " -- " << p << '\n'; exit(-1); }
      extPaths_["DEFAULT"].push_back(full);
    } else {
      auto exts = sophoi::split(pathparts[0]);
      assert(exts.size() > 0);
      pathparts.erase(pathparts.begin());
      for (auto& p : pathparts) {
        char *full = realpath(p.c_str(), pathbuf);
        if (full == NULL) { std::cerr << "-- bad path in CFGPATH=" << env.c_str() << " -- " << p << '\n'; exit(-1); }
        for (auto& ext : exts) {
          assert(! ext.empty());
          extPaths_[ext].push_back(full);
        }
      }
    }
  }
  char * fullpath = realpath(fname.c_str(), pathbuf);
  if (fullpath == NULL) { std::cerr << "-- bad path to initialize PathFinder: " << fname << '\n'; exit(-1); }
  fullpath[sizeof(pathbuf)-1] = '\0';
  for (auto& ep : extPaths_) {
    if (endsWith(fname, ep.first)) {
      extra_ = ep.first;
      break;
    }
  }
  //basename

  cwd_ = getcwd(pathbuf,sizeof(pathbuf));
}

std::string PathFinder::locate(std::string fname)
{
  if (fname.empty()) { return fname; }
  char pathbuf[MAXPATHLEN];
  char * fullpath = realpath(fname.c_str(), pathbuf);
  if (fullpath != NULL) {
    return fullpath;
  }
//  std::cerr << "-- not exist: " << fname << ", looking further\n";
  if (fname[0] != '/') {  // XXX deal with absolute path later
    if (not extra_.empty()) {
      auto pathitr = extPaths_.find(extra_);
      if (pathitr != extPaths_.end()) {
        std::string extfn = fname;
        if (! endsWith(fname, extra_)) { extfn = fname + extra_; }  // ext usu. starts with "."
        for (auto & path : pathitr->second) {
          std::string tryname = path + "/" + extfn;
          fullpath = realpath(tryname.c_str(), pathbuf);
          if (fullpath != NULL) { return fullpath; }
        }
      }
    }
    auto dftitr = extPaths_.find("DEFAULT");
    if (dftitr != extPaths_.end()) {
      for (auto & path : dftitr->second) {
          std::string tryname = path + "/" + fname;
          fullpath = realpath(tryname.c_str(), pathbuf);
          if (fullpath != NULL) { return fullpath; }
      }
      if (endsWith(fname, extra_)) {
        std::string barefn = fname.substr(0, fname.length() - extra_.length());
        for (auto & path : dftitr->second) {
          std::string tryname = path + "/" + barefn;
          fullpath = realpath(tryname.c_str(), pathbuf);
          if (fullpath != NULL) { return fullpath; }
        }
      }
    }
  }
  return fname;
}
