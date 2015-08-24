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
  if (*index < (*size - sizeof(int))) {
    int* nextItem = (int*)&buffer[*index];
    
    *nextItem = val;
    *index += sizeof(int);
  }
}

void countInvocations(int* counter) {
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
  int index = bit / (sizeof(char) * 8);
  int mask = 1 << (bit & 0x7);

  bitmap[index] |= mask;
}

void bucketValues(int val, struct buckets* buckets) {
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
  // Count is initialized to one, average to zero
  *average += (value - *average) / *count;
  *count += 1.0;
}

void average(double* value, double* lastValue, double* average, double* count) {
  if ((*count == 1.0) || (*lastValue != *value)) {
    addToAverage(*value, average, count);

    *lastValue = *value;
  }
}

