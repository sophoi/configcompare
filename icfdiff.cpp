#include <iostream>
#include "icf.hpp"

int main(int argc, char **argv) {
  if (argc != 2) { std::cerr << "expecting only 1 arg as icf file" << std::endl; exit(-1); }
  Icf icf(argv[1]);
  std::cout << icf << std::endl;
}
