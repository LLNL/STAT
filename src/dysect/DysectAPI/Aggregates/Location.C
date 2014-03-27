#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

FuncLocation::FuncLocation(int id, int count, string fmt, void* payload) : StrAgg(funcLocAgg, id, count, fmt, payload) {
}

FuncLocation::FuncLocation() : StrAgg(funcLocAgg) {
}

bool FuncLocation::print() {
  
  printf("Function name aggregate:\n");
  printf("\tid: %ld\n", getId());
  printf("\tcount: %d\n", getCount());

  map<string, int>::iterator countMapIter = countMap.begin();
  for(; countMapIter != countMap.end(); countMapIter++) {
    int count = countMapIter->second;
    string name = countMapIter->first;

    printf("\t\tcount %d\n", count);
    printf("\t\tname: %s\n", name.c_str());
  }

  printf("\n");
  
  return true;
}

FileLocation::FileLocation(int id, int count, string fmt, void* payload) : StrAgg(funcLocAgg, id, count, fmt, payload) {
}

FileLocation::FileLocation() : StrAgg(funcLocAgg) {
}

bool FileLocation::print() {
  return true;
}

FuncParamNames::FuncParamNames(int id, int count, string fmt, void* payload) : StrAgg(paramNamesAgg, id, count, fmt, payload) {
}

FuncParamNames::FuncParamNames() : StrAgg(paramNamesAgg) {
}

bool FuncParamNames::print() {
  return true;
}

StackTraces::StackTraces(int id, int count, string fmt, void* payload) : StrAgg(tracesAgg, id, count, fmt, payload) {
}

StackTraces::StackTraces() : StrAgg(tracesAgg) {
}

bool StackTraces::print() {
  return true;
}

StaticStr::StaticStr(int id, int count, string fmt, void* payload) : StrAgg(staticStrAgg, id, count, fmt, payload) {
}

StaticStr::StaticStr() : StrAgg(staticStrAgg) {
}

bool StaticStr::print() {
  return true;
}

bool StaticStr::collect(void* process, void* thread) {
  return true;
}

bool StaticStr::addStr(string str) {

  if(countMap.empty()) {
    countMap.insert(pair<string, int>(str, 1));
  } else {
    map<string, int>::iterator mapIter = countMap.find(str);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      countMap.erase(mapIter);
      count++;
    }

    countMap.insert(pair<string, int>(str, count));
  }
  return true;
}
