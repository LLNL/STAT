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

#ifndef __ProcWait_H
#define __ProcWait_H

namespace DysectAPI {

  class ProcWait {
    int waiting;

  public:
    ProcWait();

    enum Events {
      nothing         = 0x00,
      probe           = 0x01,
      enableChildren  = 0x02,
      disable         = 0x04,
      detach          = 0x08,
      instrumentation = 0x10,
      analysis        = 0x20,
      globalResult    = 0x40
    };

    bool ready();
    bool waitFor(Events event);
    bool handled(Events event);
  };

}

#endif // __ProcWait_H
