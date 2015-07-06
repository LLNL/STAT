/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Nielsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __DYSECTAPI_RANKBITMAP_CPP
#define __DYSECTAPI_RANKBITMAP_CPP

#include <string>

namespace DysectAPI {
  class RankBitmap {
  protected:
    class BitmapPart {
    protected:
      static const int bitsInLong = sizeof(long) * 8;
      static const int expandAmount = 2;
      static const int expandLimit = 128;

      int getArrIndex(int bitNr);
      int getBitIndex(int bitNr);
      int getStartBitNr(int bitNr);
      long getBitMask(int bitNr);
      int getLastBit();

    public:
      int startBitNr;
      int length;
      BitmapPart* next;
      long* bits;

      BitmapPart(BitmapPart* next = 0);
      BitmapPart(char* p);
      ~BitmapPart();

      void setBit(int bitNr);
      void expand();
      void expand(int amount);
      bool shouldMerge(int atLength);
      void tryMergeNext();
      void mergeNext();

      void merge(BitmapPart* other);

      int describe(std::string& res);
      int listSize();
      int serialize(char *p);
    };

    BitmapPart* bitmap;

  public:
    RankBitmap();
    RankBitmap(char *payload);
    ~RankBitmap();

    void addRank(int rank);

    int getSize();
    int serialize(char *p);

    void merge(RankBitmap* other);

    std::string toString();

  };
}

#endif // __DYSECTAPI_RANKBITMAP_CPP

