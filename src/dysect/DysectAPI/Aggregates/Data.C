#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

Value::Value(std::string fmt, void *lbuf) {
  int slen = fmt.size();
  const char *str = fmt.c_str();

  if((slen > 1) || (slen < 3)) {
    if(str[0] == '%') {
      if(str[1] == 'd') {
        len = sizeof(int);
        content = intType;

      } else if(str[1] == 'l') {
        len = sizeof(long);
        content = longType;

      } else {
        fprintf(stderr, "Unknown format: %s\n", fmt.c_str());
      }
    }
  }

  buf = lbuf;
}

Value::Value() : content(noType), len(0), buf(0) {}

Value::Value(float fval) : content(floatType) {}

Value::Value(double dval) : content(doubleType) {}

Value::Value(int ival) : content(intType), len(0), buf(0){
  populate<int>(ival);
}

Value::Value(long lval) : content(longType), len(0), buf(0){
  populate<long>(lval);
}

Value::Value(bool bval) : content(boolType), len(0), buf(0) {
  populate<bool>(bval);
}

void Value::copy(const Value& rhs) {
  if(this == &rhs) {
    return ;
  }

  if(rhs.len > len) {
    free(buf);
    buf = malloc(rhs.len);
    len = rhs.len;
  } else {
    memset(buf, '\0', len);
  }

  memcpy(buf, rhs.buf, rhs.len);

  len = rhs.len;
  content = rhs.content;
};

Value::content_t Value::getType() {
  return content;
}

int Value::compare(Value& c) {
  int ret = 0;
  content_t domType = Value::noType;

  if(c.content > content) {
    domType = c.content;
  } else {
    domType = content;
  }

  switch(content) {
    case boolType: {
                     bool op1 = getValue<bool>();
                     bool op2 = c.getValue<bool>();

                     if(op1 == op2)
                       return eq;
                   }
                   break;
    case intType: {

                    int op1 = getValue<int>();
                    int op2 = c.getValue<int>();

                    int diff = op1 - op2;

                    if(diff == 0)
                      return eq;

                    if(diff < 0)
                      return lt;

                    if(diff > 0) 
                      return gt;
                  }
                  break;
    case longType: {
                     long op1 = getValue<long>();
                     long op2 = c.getValue<long>();

                     long diff = op1 - op2;

                     if(diff == 0)
                       return eq;

                     if(diff < 0)
                       return lt;

                     if(diff > 0) 
                       return gt;
                   }
                   break;
    default:
                   cerr << "Type not yet supported and values cannot be compared" << endl;
                   break;
  }

  return 0;
}

bool Value::isEqual(Value& c) {
  int ret = compare(c);

  return((ret & eq) == eq);
}

bool Value::isNotEqual(Value& c) {
  int ret = compare(c);

  return((ret & eq) != eq);
}

bool Value::isLessThan(Value& c) {
  int ret = compare(c);

  return((ret & lt) == lt);
}

bool Value::isLessThanEqual(Value& c) {
  int ret = compare(c);

  return (((ret & lt) == gt) || ((ret & eq) == eq));
}

bool Value::isGreaterThan(Value& c) {
  int ret = compare(c);

  return((ret & gt) == gt);
}

bool Value::isGreaterThanEqual(Value& c) {
  int ret = compare(c);

  return (((ret & gt) == gt) || ((ret & eq) == eq));
}

bool Value::getStr(string& str) {
  char outBuf[32];

  // XXX: Replace with memory copy
  switch(content) {
    case intType:
      snprintf((char*)&outBuf, 32, "%d",  *(int*)buf);
      break;
    case longType:
      snprintf((char*)&outBuf, 32, "%ld",  *(long*)buf);
      break;

    default:
      break;
  }

  str = std::string((char*)&outBuf);

  return true;
}

Value& Value::operator=(Value& rhs) {
  copy(rhs);
  return rhs;
}

bool Value::operator<(Value& rhs) {
  return isLessThan(rhs);
}

bool Value::operator>(Value& rhs) {
  return isGreaterThan(rhs);
}
