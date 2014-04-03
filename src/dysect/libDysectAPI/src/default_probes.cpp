/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <LibDysectAPI.h>

DysectStatus DysectAPI::defaultProbes() {

  // XXX: Check if probes, handling these events, already is in place

  Probe* exit = new Probe(Async::exit(), Domain::world(500), Acts(Act::trace("Process exited"), Act::detach()));
  Probe* segv = new Probe(Async::signal(SIGSEGV), Domain::world(500, true), Acts(Act::trace("Memory violation - segmentation fault"), Act::stackTrace(), Act::stat()));//, Act::detach()));
  Probe* sigbus = new Probe(Async::signal(SIGBUS), Domain::world(500), Act::trace("Memory violation - bus error in function @function()"));
  Probe* fpe = new Probe(Async::signal(SIGFPE), Domain::world(500), Act::trace("Floating point exception in function @function()"));

  ProbeTree::addRoot(fpe);
  ProbeTree::addRoot(exit);
  ProbeTree::addRoot(segv);
  ProbeTree::addRoot(sigbus);

  return DysectOK;
}
