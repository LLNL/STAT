#ifndef __PARSER_H
#define __PARSER_H

namespace DysectAPI {
  class Parser {
  public:
    static std::vector<std::string> tokenize(std::string input, char delim);
  };
}

#endif
