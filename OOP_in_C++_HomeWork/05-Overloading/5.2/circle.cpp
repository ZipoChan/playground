#include "circle.hpp"
#include <iostream>

using std::cout;
using std::endl;

int main(int argc, char **argv) {
  Circle a, b;
  a.init();
  b.init();
  ++a.p;
  a.p++;
  --b.p;
  b.p--;
  if (a.isCross(b))
    cout << "Crossed." << endl;
  else
    cout << "Not Crossed." << endl;
  return 0;
}
