#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

CmpAgg::CmpAgg(agg_type ltype, string fmt) : fmt_(fmt), AggregateFunction(ltype), curVal() {
}

CmpAgg::CmpAgg(agg_type ltype, int id, int count, std::string fmt, void* payload) : curVal(fmt, payload) {
  id_ = id;
  count_ = count;
  fmt_ = fmt;
  type = ltype;
}

bool CmpAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;
  
  if(agg->getId() != getId())
    return false;
  
  CmpAgg* aggInstance = dynamic_cast<CmpAgg*>(agg);
  
  Value val = aggInstance->getVal();

  if(compare(val, curVal)) {
    curVal = val;
  }
  
  count_ += agg->getCount();
  
  return true;
}

bool CmpAgg::print() {
  return true;
}

bool CmpAgg::getStr(string& str) {
  str = "<none>";
  
  if(curVal.getType() != Value::noType) {
    curVal.getStr(str);
  }

  return true;
}

int CmpAgg::getSize() {
  return sizeof(struct subPacket) - sizeof(void*) + maxFmt + curVal.getLen();
}

int CmpAgg::writeSubpacket(char *p) {
  struct subPacket* packet;
  
  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memcpy(&(packet->fmt), fmt_.c_str(), maxFmt);

  char* curpos = (char*)&(packet->payload);

  memcpy(curpos, curVal.getBuf(), curVal.getLen());

  return getSize();
}

bool CmpAgg::clear() {
  curVal.setType(Value::noType);
}

bool CmpAgg::isSynthetic() {
  return false;
}

bool CmpAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}

bool CmpAgg::fetchAggregates(Probe* probe) {
  return true;
}

Min::Min(int id, int count, std::string fmt, void* payload) : CmpAgg(minAgg, id, count, fmt, payload) {
}

bool Min::compare(Value& newVal, Value& oldVal) {
  return oldVal > newVal;
}


Max::Max(int id, int count, std::string fmt, void* payload) : CmpAgg(maxAgg, id, count, fmt, payload) {
}

bool Max::compare(Value& newVal, Value& oldVal) {
  return oldVal < newVal;
}
