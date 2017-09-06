#ifndef WIN32

#include <iostream>

#include "convert.hpp"

int main(int argc, char **args) {
  // read file
  if (argc != 2) {
    std::cerr << "usage: " << args[0] << " my_file.ptcop" << std::endl;
    return 0;
  }
  char *filename = args[1];
  FILE *file = fopen(args[1], "r");
  pxtnDescriptor desc;
  desc.set_file_r(file);
  pxtnService pxtn;
  pxtn.init();
  pxtn.read(&desc);
  fclose(file);

  pxtn_to_midi(pxtn).write(string(filename) + ".mid");
  return 0;
}

#endif // WIN32
