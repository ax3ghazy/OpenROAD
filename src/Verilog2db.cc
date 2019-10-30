// Resizer, LEF/DEF gate resizer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include "Machine.hh"
#include "Error.hh"
#include "Report.hh"
#include "Debug.hh"
#include "Vector.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "opendb/db.h"

namespace sta {

using odb::dbDatabase;
using odb::dbChip;
using odb::dbBlock;
using odb::dbTech;
using odb::dbLib;
using odb::dbMaster;
using odb::dbInst;
using odb::dbNet;
using odb::dbBTerm;
using odb::dbMTerm;
using odb::dbITerm;
using odb::dbSet;
using odb::dbIoType;

class Verilog2db
{
public:
  Verilog2db(Network *network,
	     dbDatabase *db);
  void makeBlock();
  void makeDbNetlist();

protected:
  void makeDbComponents();
  dbIoType staToDb(PortDirection *dir);
  void makeDbNets(const Instance *inst);
  bool hasTerminals(Net *net) const;
  uint metersToDbu(double dist) const;
  double dbuToMeters(uint dist) const;
  dbMaster *getMaster(Cell *cell);

  Network *network_;
  dbDatabase *db_;
  dbBlock *block_;
  std::map<Cell*, dbMaster*> master_map_;
};

void
verilog2db(Network *network,
	   dbDatabase *db)
{
  Verilog2db v2db(network, db);
  v2db.makeBlock();
  v2db.makeDbNetlist();
}

Verilog2db::Verilog2db(Network *network,
		       dbDatabase *db) :
  network_(network),
  db_(db),
  block_(nullptr)
{
}

void
Verilog2db::makeBlock()
{
  dbChip *chip = db_->getChip();
  if (chip == nullptr)
    chip = dbChip::create(db_);
  const char *design = network_->name(network_->cell(network_->topInstance()));
  block_ = dbBlock::create(chip, design, network_->pathDivider());
  block_->setBusDelimeters('[', ']');
}

void
Verilog2db::makeDbNetlist()
{
  makeDbComponents();
  makeDbNets(network_->topInstance());
}

void
Verilog2db::makeDbComponents()
{
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *inst = leaf_iter->next();
    const char *inst_name = network_->pathName(inst);
    Cell *cell = network_->cell(inst);
    dbMaster *master = getMaster(cell);
    dbInst::create(block_, master, inst_name);
  }
  delete leaf_iter;
}

dbIoType
Verilog2db::staToDb(PortDirection *dir)
{
  if (dir == PortDirection::input())
    return dbIoType::INPUT;
  else if (dir == PortDirection::output())
    return dbIoType::OUTPUT;
  else if (dir == PortDirection::bidirect())
    return dbIoType::INOUT;
  else if (dir == PortDirection::tristate())
    return dbIoType::OUTPUT;
  else
    return dbIoType::INOUT;
}

void
Verilog2db::makeDbNets(const Instance *inst)
{
  bool is_top = (inst == network_->topInstance());
  NetIterator *net_iter = network_->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if ((is_top || !hasTerminals(net))
	&& !network_->isGround(net)
	&& !network_->isPower(net)) {
      const char *net_name = network_->pathName(net);
      dbNet *db_net = dbNet::create(block_, net_name);
      
      NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
      while (pin_iter->hasNext()) {
	Pin *pin = pin_iter->next();
	if (network_->isTopLevelPort(pin)) {
	  const char *port_name = network_->portName(pin);
	  if (block_->findBTerm(port_name) == nullptr) {
	    dbBTerm *bterm = dbBTerm::create(db_net, port_name);
	    dbIoType io_type = staToDb(network_->direction(pin));
	    bterm->setIoType(io_type);
	  }
	}
	else if (network_->isLeaf(pin)) {
	  const char *port_name = network_->portName(pin);
	  Instance *inst = network_->instance(pin);
	  const char *inst_name = network_->pathName(inst);
	  dbInst *db_inst = block_->findInst(inst_name);
	  dbMaster *master = db_inst->getMaster();
	  dbMTerm *mterm = master->findMTerm(block_, port_name);
	  if (mterm)
	    dbITerm::connect(db_inst, db_net, mterm);
	}
      }
      delete pin_iter;
    }
  }
  delete net_iter;
}

bool
Verilog2db::hasTerminals(Net *net) const
{
  NetTermIterator *term_iter = network_->termIterator(net);
  bool has_terms = term_iter->hasNext();
  delete term_iter;
  return has_terms;
}

dbMaster *
Verilog2db::getMaster(Cell *cell)
{
  auto miter = master_map_.find(cell);
  if (miter != master_map_.end())
    return miter->second;
  else {
    const char *cell_name = network_->name(cell);
    for (dbLib *lib : db_->getLibs()) {
      dbMaster *master = lib->findMaster(cell_name);
      if (master) {
	master_map_[cell] = master;
	return master;
      }
    }
    master_map_[cell] = nullptr;
    return nullptr;
  }
}

// DBUs are nanometers.
uint
Verilog2db::metersToDbu(double dist) const
{
  dbTech *tech = db_->getTech();
  if (tech->hasManufacturingGrid()) {
    int grid = tech->getManufacturingGrid();
    return round(round(dist * 1e9 / grid) * grid);
  }
  else
    return round(dist * 1e9);
}

// DBUs are nanometers.
double
Verilog2db::dbuToMeters(uint dist) const
{
  return dist * 1E-9;
}

}
