#include <iostream>
#include <queue>
#include <string>
using namespace std;

int main() {
  queue<string> tmp;
  tmp.emplace("ca");
  tmp.emplace("ta");
  cout << tmp.front().substr(1) << endl;
  return 0;
}
