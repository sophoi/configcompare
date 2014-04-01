#include <iostream>
#include "icf.hpp"

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) { std::cerr << "expecting 1 or 2 arg as icf file" << std::endl; exit(-1); }
  if (argc == 2) {
    Icf icf(argv[1]);
    std::cout << icf << std::endl;
  } else if (argc == 3) {
    Icf old(argv[1]), neu(argv[2]);
    std::cout << old.diff(neu);
    std::cout << neu.diff(old,true);
  }
}
