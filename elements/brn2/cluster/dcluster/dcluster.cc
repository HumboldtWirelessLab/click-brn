/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * dcluster.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/standard/compression/lzw.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "dcluster.hh"
#include "dclusterprotocol.hh"

CLICK_DECLS

DCluster::DCluster()
  : _max_no_min_rounds(1),
    _max_no_max_rounds(1),
    _ac_min_round(0),
    _ac_max_round(0),
    _cluster_head(NULL),
    _delay(0)
{
  Clustering::init();
}

DCluster::~DCluster()
{
}

int
DCluster::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM , cpElement, &_node_identity,
      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      "DISTANCE", cpkP+cpkM , cpInteger, &_max_distance,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

//  click_chatter("Start: %s",get_info().c_str());

  return 0;
}

static int
handler(void *element, EtherAddress */*ea*/, char *buffer, int size, bool direction)
{
  DCluster *dcl = (DCluster*)element;

  if ( direction )
    return dcl->lpSendHandler(buffer, size);
  else
    return dcl->lpReceiveHandler(buffer, size);
}

int
DCluster::initialize(ErrorHandler *)
{
  init_cluster_node_info();

  _max_round = new ClusterNodeInfo[_max_distance];
  _min_round = new ClusterNodeInfo[_max_distance];

  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_DCLUSTER, &handler);

  return 0;
}

void
DCluster::init_cluster_node_info()
{
  BRN_INFO("Init Clusterhead: %u",_node_identity->getNodeID32());
  _my_info = ClusterNodeInfo(_node_identity->getMasterAddress(), _node_identity->getNodeID32(), 0);
  _cluster_head = &_my_info;
}

int
DCluster::lpSendHandler(char *buffer, int size)
{
  struct dcluster_info info;

  BRN_INFO("Linkprobe-Send: Min: %d Max: %d",_ac_min_round,_ac_max_round);

  //store myself
  _my_info.storeStruct(&info.me);
  info.me.round=0;

  /*
   * store actual max which depends on the round *
   * first round is the node itself              *
   * next it is the max of the round before      *
   * number of max rounds is the number of hops  *
   * and so the cluster size
   */

  if ( _ac_max_round == 0 ) {        //in the first round it's me
    _my_info.storeStruct(&info.max);
    info.max.round=0;
  } else {
    _max_round[_ac_max_round - 1].storeStruct(&info.max); //take max off last round
    info.max.round = _ac_max_round;                       //for this round
  }

  BRN_INFO("AC maxround: %d Address: %s",_ac_max_round,EtherAddress(info.max.etheraddr).unparse().c_str());

  /* switch to next round if available or back to round zero */
  _ac_max_round = (_ac_max_round + 1) % _max_no_max_rounds;

  /*
   * if max round is finished ( currently we use a delay of 12 round   *
   * then start the min round.                                         *
   * In the first round the min is the max                             *
   * if max round not finished, store myself and mark entry as invalid *
   */

  BRN_INFO("Delay: %d", _delay);
  if ( _delay > 12 ) {  //delay TODO: remove
    if ( _ac_min_round == 0 ) {                                                    //round 0
      if ( _max_round[_max_distance - 1]._distance == DCLUSTER_INVALID_ROUND ) {   //use invalid entry if max-round not finished
        _my_info.storeStruct(&info.min);
        info.min.round=DCLUSTER_INVALID_ROUND;
        info.min.hops=0;
      } else {                                                                     //use max max-round
        BRN_INFO("Insert max Max as lowest Min");
        _max_round[_max_distance - 1].storeStruct(&info.min);
        info.min.round=0;
      }
    } else {
      BRN_INFO("Insert min %d Address: %s",_ac_min_round-1,EtherAddress(info.min.etheraddr).unparse().c_str());
      _min_round[_ac_min_round - 1].storeStruct(&info.min);                        //use min
      info.min.round = _ac_min_round;
    }

    _ac_min_round = (_ac_min_round + 1) % _max_no_min_rounds;

  } else {
    _my_info.storeStruct(&info.min);
    info.min.round=DCLUSTER_INVALID_ROUND;
    info.min.hops=0;
    _delay++;
  }

  return DClusterProtocol::pack(&info, buffer, size);
}

int
DCluster::lpReceiveHandler(char *buffer, int size)
{
  int len;
  struct dcluster_info info;

  len = DClusterProtocol::unpack(&info, buffer, size);

  if ( (info.max.hops < _max_distance) &&
       ( (_max_round[info.max.round]._distance == DCLUSTER_INVALID_ROUND) ||          //there is no valid entry, or
        ((_max_round[info.max.round]._distance != DCLUSTER_INVALID_ROUND) &&          //entry is valid and
           (( ntohl(info.max.id) > _max_round[info.max.round]._id ) ||               //new id bigger
            ((ntohl(info.max.id) == _max_round[info.max.round]._id ) &&              //o equal but lower hop count
            ((uint32_t)(info.max.hops + 1) < _max_round[info.max.round]._distance )))))) {

    _max_round[info.max.round].setInfo(info.max.etheraddr, ntohl(info.max.id), info.max.hops + 1);

    /* Increase number of max round if a info with higher max round is received */
    if ( _max_no_max_rounds == (info.max.round + 1) ) {
      BRN_WARN("Increase number of rounds");
      _max_no_max_rounds++;
    }
  }

  if ((( info.min.round < _max_distance ) && ( info.min.round != DCLUSTER_INVALID_ROUND ) ) &&
      (( _min_round[info.min.round]._distance == DCLUSTER_INVALID_ROUND ) ||
       ( ntohl(info.min.id) < _min_round[info.min.round]._id ) ||
        (( ntohl(info.min.id) == _min_round[info.min.round]._id ) &&
         ((uint32_t)(info.min.hops + 1) < _min_round[info.min.round]._distance )))) {
    BRN_INFO("Got Min. Round: %d ID: %u",info.min.round, ntohl(info.min.id));
    _min_round[info.min.round].setInfo(info.min.etheraddr, ntohl(info.min.id), info.min.hops + 1);
    if ( _max_no_min_rounds == (info.min.round + 1) ) {
      _max_no_min_rounds++;
    }
  }

  if ( _max_no_min_rounds == _max_distance ) _cluster_head = selectClusterHead();

  return len;
}

DCluster::ClusterNodeInfo*
DCluster::selectClusterHead() {
  int i,j;
  //Rule 1.
  for ( i = 0; i < _max_distance; i++ )
    if ( _my_info._id == _min_round[i]._id ) return &_my_info;

  ClusterNodeInfo* minpair = NULL;
  uint32_t min = UINT_MAX; //4,294,967,295
  //Rule 2.
  for ( i = 0; i < _max_distance; i++ ) {
    for ( j = 0; j < _max_distance; j++ ) {
      if ( ( _min_round[i]._id == _max_round[j]._id ) && ( _min_round[i]._id < min ) ) {
        minpair = &_min_round[i];
        min = _min_round[i]._id;
      }
    }
  }

  if ( minpair != NULL ) return minpair;

  //Rule 3.
  ClusterNodeInfo* maxnode = &_max_round[0];

  for ( i = 1; i < _max_distance; i++ )
    if ( _max_round[i]._id > maxnode->_id )
        maxnode = &_max_round[i];

  return maxnode;

}


void
DCluster::informClusterHead(DCluster::ClusterNodeInfo*) {


}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
DCluster::get_info()
{
  StringAccum sa;
  int r;

  sa << "Node: " << _node_identity->getNodeName() << " ID: " << _my_info._id;
  sa << " Clusterhead: " << getClusterHead()->_ether_addr.unparse() << " CH-ID: " << getClusterHead()->_id << "\n";

  sa << "Mode\tRound\tAddress\t\t\tDist\tHighest ID\n";

  for( r = 0; r < _max_distance; r++ ) {
    sa << "MAX\t" << r << "\t" << _max_round[r]._ether_addr.unparse() << "\t" << _max_round[r]._distance << "\t" << _max_round[r]._id << "\n";
  }

  for( r = 0; r < _max_distance; r++ ) {
    sa << "MIN\t" << r <<  "\t" << _min_round[r]._ether_addr.unparse() << "\t" << _min_round[r]._distance << "\t" << _min_round[r]._id << "\n";
  }

  return sa.take_string();
}

static String
read_stats_param(Element *e, void *)
{
  DCluster *dc = (DCluster *)e;

  return dc->get_info();
}

static String
read_debug_param(Element *e, void *)
{
  DCluster *fl = (DCluster *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  DCluster *fl = (DCluster *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
DCluster::add_handlers()
{
  Clustering::add_handlers();

  add_read_handler("debug", read_debug_param, 0);
  add_read_handler("stats", read_stats_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DCluster)
