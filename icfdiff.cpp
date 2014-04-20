#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>
#include "icf.hpp"

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "expecting 1 or 2 arg as icf file" << std::endl;
    exit(-1);
  }
  std::string a1(argv[1]);
  std::map<std::string, std::string> params = {
    {"CFGPATH", R"(  default paths are to find include files not in cwd
  if the original file has postfix like .gz, included ones are to find .gz
    postfixed files in .gz paths before default; same goes for .new, .bz2, etc
  /default/path1;.new:/new/path1:/new/path2;.gz:/gz/path1:/gz/path2;/default/path2)"},
    {"EXCLUDE", "  some included .icfs aren't essential for validate/diff and if excluded speeds up"},
    {"IGNORED_ITEMS", "  (todo) some elements jump between groups, ignoring them make diff clearer"},
    {"KVSEPS", "  some kv pairs have values further splittable, configure by key(sep), or ALL(,)"},
    {"DEFAULT", R"(  naturally DEFAULT group includes everything, but we can override it to contain,
  say, "Pirarras,Munduruku,Parintintin")"},
    {"DISPLAY_PREFIX", "  simply prefix all output lines with a custom header"},
  };
  if (a1 == "-h") {
    std::cout << "$ icfdiff f1.icf           # validate\n"
              << "$ icfdiff f1.icf f2.icf    # diff\n\n";
    for (auto& kv : params) {
      std::string dft;
      char * env = getenv(kv.first.c_str());
      if (env) {
        dft = std::string(" = ") + env;
      }
      std::cout << kv.first << dft << std::endl << kv.second << std::endl;
    }
    exit(0);
  }
  if (argc == 2) {
    Icf icf(argv[1]);
    std::cout << icf << std::endl;
  } else if (argc == 3) {
    Icf old(argv[1]), neu(argv[2]);
    std::cout << old.diff(neu);
    std::cout << neu.diff(old, true);
  }
}
