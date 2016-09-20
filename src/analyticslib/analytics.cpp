#include <iostream>
#include <cstdio>
#include <signal.h>
#include <unistd.h>

using namespace std;

enum bitmap {
  initialized = 0x1,
  nonzero     = 0x2,
  nonnegative = 0x4,
  increasing  = 0x8,
  decreasing  = 0x10,
  unchanged   = 0x20
};

void collectFeatures(int val, int* min, int* max, char* features) {
#ifdef DEBUG_ANALYTICS
  cout<<"collectFeatures "<<val<<" "<<*min<<" "<<*max<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  int newFeat = (int)(*features);

  if (newFeat == 0) {
    newFeat = initialized | increasing | decreasing | unchanged;

    if (val != 0) {
      newFeat = newFeat | nonzero;
    }

    if (val >= 0) {
      newFeat = newFeat | nonnegative;
    }

    *min = val;
    *max = val;
    *features = newFeat;

    return;
  }

  if ((newFeat & unchanged) && (val != *min)) {
    newFeat = newFeat & ~unchanged;
  }

  if ((newFeat & decreasing) && (val > *min)) {
    newFeat = newFeat & ~decreasing;
  }

  if ((newFeat & increasing) && (val < *max)) {
    newFeat = newFeat & ~increasing;
  }

  if ((newFeat & nonzero) && (val == 0)) {
    newFeat = newFeat & ~nonzero;
  }

  if ((newFeat & nonnegative) && (val < 0)) {
    newFeat = newFeat & ~nonnegative;
  }

  if (val < *min) {
    *min = val;
  }

  if (val > *max) {
    *max = val;
  }

  *features = (char)newFeat;
}

void updateInvariant(int val, int* orgVal, int* mods, char* initialized) {
#ifdef DEBUG_ANALYTICS
  cout<<"updateInvariant "<<val<<" "<<*orgVal<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  if (!*initialized) {
    *orgVal = val;
    *mods = ~0;

    *initialized = 1;
  } else {
    int newMods = (*mods) & ~(*orgVal ^ val);

    if (newMods != *mods) {
      cout << "Invariant relaxed: 0x" << hex << *mods << " -> 0x" << newMods << dec << endl;

      *mods = newMods;
    }
  }
}

void collectValue(int val, char* buffer, int* index, int* size) {
#ifdef DEBUG_ANALYTICS
  cout<<"collectValue "<<val<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  if (*index < (*size - sizeof(int))) {
    int* nextItem = (int*)&buffer[*index];
    int* lastItem = nextItem - 1;

    if ((*index == 0) || (*lastItem != val)){
      *nextItem = val;
      *index += sizeof(int);
    }
  }
}

void collectAnyValue(int val, char* buffer, int* index, int* size) {
#ifdef DEBUG_ANALYTICS
  cout<<"collectAnyValue "<<val<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  if (*index < (*size - sizeof(int))) {
    int* nextItem = (int*)&buffer[*index];

    *nextItem = val;
    *index += sizeof(int);
  }
}

void countInvocations(int* counter) {
#ifdef DEBUG_ANALYTICS
  cout<<"countInvocations "<<*counter<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  *counter += 1;
}

struct buckets {
  char* bitmap;
  int rangeStart;
  int rangeEnd;
  int count;
  int stepSize;
};

void setBit(int bit, char* bitmap) {
#ifdef DEBUG_ANALYTICS
  cout<<"setBit "<<bit<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  int index = bit / (sizeof(char) * 8);
  int mask = 1 << (bit & 0x7);

  bitmap[index] |= mask;
}

void bucketValues(int val, struct buckets* buckets) {
#ifdef DEBUG_ANALYTICS
  cout<<"bucketValues "<<val<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  if (val < buckets->rangeStart) {
    setBit(buckets->count, buckets->bitmap);
  } else if (val >= buckets->rangeEnd) {
    setBit(buckets->count + 1, buckets->bitmap);
  } else {
    int bitNr = (val - buckets->rangeStart) / buckets->stepSize;
    setBit(bitNr, buckets->bitmap);
  }
}

void addToAverage(double value, double* average, double *count) {
#ifdef DEBUG_ANALYTICS
  cout<<"addToAverage "<<value<<" "<<*average<<" "<<*count<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  // Count is initialized to one, average to zero
  *average += (value - *average) / *count;
  *count += 1.0;
}

void average(double* value, double* lastValue, double* average, double* count) {
#ifdef DEBUG_ANALYTICS
  cout<<"average "<<*value<<" "<<*lastValue<<" "<<*average<<" "<<*count<<" "<<__FILE__<<" "<<__LINE__<<endl;
#endif
  if ((*count == 1.0) || (*lastValue != *value)) {
    addToAverage(*value, average, count);

    *lastValue = *value;
  }
}

