#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include <iostream>
#include <vector>

using namespace std;

template<typename T1, typename T2, typename T3> class WeightList {
private:
    static int getSeed() {
      char hostname[256];
      
      struct timeval tv;
      struct timezone tz;
      struct tm *tm;
      gettimeofday(&tv, &tz);
      tm=localtime(&tv.tv_sec);
      
      int seed = 0;

      gethostname(hostname, 256);

      seed += tm->tm_hour;
      seed += tm->tm_min;
      seed += tm->tm_sec;
      seed += tv.tv_usec;

      int pos = 0;
      char c = 0;
      do {
        c = hostname[pos++];
        seed += (int)c;
      } while(c != '\0');
      
      return seed;
    }
public:
  static void gen(vector<pair<T1, vector<T2> > > &list) {
    srand(getSeed());

    const size_t max_list_size = 1024;
    const T2 max_weight = 1<<31;

    int n = (rand() % max_list_size) + 1;
    int m = (rand() % max_list_size) + 1;

    for(int i = 0; i < n; i++) {
      vector<T2> items;

      T1 weight = (T1)(rand() / rand());

      for(int j = 0; j < m; j++) {
        T2 item = (T2)(rand() / rand());
        items.push_back(item);
      }

      list.push_back(pair<T1, vector<T2> >(weight, items));
    }

    return ;
  }

  static T3 sum(vector<pair<T1, vector<T2> > > &list) {
    T3 result;
    typename vector<pair<T1, vector<T2> > >::iterator listIter = list.begin();

    for(; listIter != list.end(); listIter++) {
      T1 weight = (*listIter).first;

      vector<T2>& items = (*listIter).second;
      typename vector<T2>::iterator itemIter = items.begin();

      for(;itemIter != items.end(); itemIter++) {
        T2 item = *itemIter;
        result += (T3)(weight * item);
      }
    }

    return result;
  }
};

int main(int argc, char **argv) {

  vector<pair<int, vector<double> > > list;
  long result = 0;

  WeightList<int, double, long>::gen(list);

  result = WeightList<int, double, long>::sum(list);

  cout << "Computed weighted sum: " << result << endl;
 
  return EXIT_SUCCESS;
}
