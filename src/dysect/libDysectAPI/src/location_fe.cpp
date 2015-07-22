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
#include <DysectAPI/Event.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace Stackwalker;
using namespace SymtabAPI;
using namespace ProcControlAPI;

//
// Location operations do not apply for front-end session
//

Location::Location(string locationExpr, bool pendingEnabled) : locationExpr(locationExpr), pendingEnabled(pendingEnabled) {
}

bool Location::enable() {
  assert(false);
  return true;
}

bool Location::disable() {
  assert(false);
  return true;
}

bool Location::disable(ProcessSet::ptr lprocset) {
  assert(false);
  return true;
}

bool Location::enable(ProcessSet::ptr lprocset) {
  assert(false);
  return true;
}

bool Location::prepare() {
  assert(false);
  return true;
}


bool Location::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}

bool Location::resolveExpression() {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::findSymbol(Dyninst::Stackwalker::Walker* proc, string name, vector<DysectAPI::CodeLocation*>& symbols) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::findSymbol(SymtabAPI::Symtab* symtab, string name, string libName, vector<DysectAPI::CodeLocation*>& symbols, bool isRegex) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::addProcLib(Walker* proc) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::getAddrs(Dyninst::Stackwalker::Walker* proc, std::vector<Dyninst::Address>& addrs) {
  assert(false);
  return true;
}

DysectAPI::CodeLocation::CodeLocation() {
}
