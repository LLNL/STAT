#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;

vector<string> Parser::tokenize(string input, char delim) {
  vector<string> out;

  size_t prev = 0;
  size_t next = 0;

  while((next = input.find_first_of(delim, prev)) != string::npos) {
    if((next - prev) != 0) { // Ignore empty strings
      out.push_back(input.substr(prev, next - prev));
    }
    prev = next + 1;
  }

  if(prev < input.size()) { // Add trailing string
    out.push_back(input.substr(prev));
  }

  return out;
}
