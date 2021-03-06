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
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/standard/compression/lzw.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "dcluster.hh"
#include "dclusterprotocol.hh"

CLICK_DECLS

DCluster::DCluster()
  : _max_distance(0),
    _max_no_min_rounds(1),
    _max_no_max_rounds(1),
    _ac_min_round(0),
    _ac_max_round(0),
    _cluster_head(NULL),
    _max_round(NULL),
    _min_round(NULL),
    _delay(0)
{
  Clustering::init();
}

DCluster::~DCluster()
{
	delete[] _max_round;
	delete[] _min_round;
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
tx_handler(void *element, const EtherAddress */*ea*/, char *buffer, int size)
{
  DCluster *dcl = reinterpret_cast<DCluster*>(element);
  return dcl->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  DCluster *dcl = reinterpret_cast<DCluster*>(element);
  return dcl->lpReceiveHandler(ea, buffer, size);
}

int
DCluster::initialize(ErrorHandler *)
{
  init_cluster_node_info();

  _max_round = new ClusterNodeInfo[_max_distance];
  _min_round = new ClusterNodeInfo[_max_distance];

  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_DCLUSTER, &tx_handler, &rx_handler);

  return 0;
}

void
DCluster::init_cluster_node_info()
{
  BRN_INFO("Init Clusterhead: %u",_node_identity->getNodeID32());
  _my_info = ClusterNodeInfo(_node_identity->getMasterAddress(), _node_identity->getNodeID32(), 0);

  _cluster_head = &_my_info;
  _own_cluster = _cluster_head;

  readClusterInfo( _my_info._clusterhead, _my_info._cluster_id, _my_info._clusterhead );
}

int
DCluster::lpSendHandler(char *buffer, int size)
{
  struct dcluster_info info;

  memset(&info.min.etheraddr,0, sizeof(info.min.etheraddr));

  BRN_INFO("Linkprobe-Send: Min: %d Max: %d",_ac_min_round,_ac_max_round);

  //store myself
  // _my_info.storeStruct(&info.me);
  _cluster_head->storeStruct( &info.me );
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
DCluster::lpReceiveHandler(EtherAddress *src, char *buffer, int size)
{
  int len;
  struct dcluster_info info;

  len = DClusterProtocol::unpack(&info, buffer, size);
  if(len<=0) return len;

  if ( (info.max.hops < _max_distance) &&
       ( (_max_round[info.max.round]._distance == DCLUSTER_INVALID_ROUND) ||          //there is no valid entry, or
        ((_max_round[info.max.round]._distance != DCLUSTER_INVALID_ROUND) &&          //entry is valid and
           (( ntohl(info.max.id) > _max_round[info.max.round]._cluster_id ) ||               //new id bigger
            ((ntohl(info.max.id) == _max_round[info.max.round]._cluster_id ) &&              //o equal but lower hop count
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
       ( ntohl(info.min.id) < _min_round[info.min.round]._cluster_id ) ||
        (( ntohl(info.min.id) == _min_round[info.min.round]._cluster_id ) &&
         ((uint32_t)(info.min.hops + 1) < _min_round[info.min.round]._distance )))) {
    BRN_INFO("Got Min. Round: %d ID: %u",info.min.round, ntohl(info.min.id));
    _min_round[info.min.round].setInfo(info.min.etheraddr, ntohl(info.min.id), info.min.hops + 1);
    if ( _max_no_min_rounds == (info.min.round + 1) ) {
      _max_no_min_rounds++;
    }
  }

  if ( _max_no_min_rounds == _max_distance ) {
	  _cluster_head = selectClusterHead();
	  _own_cluster = _cluster_head;
	  readClusterInfo( _my_info._clusterhead, _cluster_head->_cluster_id, _cluster_head->_clusterhead );
  }

  StringAccum sa;
  	sa << "Time (" << len << "): " << Timestamp::now() << ",  Node: " << EtherAddress(info.me.etheraddr) << ", cID: " << info.me.id;
  	click_chatter("INPUT: %s", sa.c_str() );

  readClusterInfo( *src, ntohl(info.me.id), EtherAddress( info.me.etheraddr ) );

  return len;
}

DCluster::ClusterNodeInfo*
DCluster::selectClusterHead() {
  int i,j;
  //Rule 1.
  for ( i = 0; i < _max_distance; i++ )
    if ( _my_info._cluster_id == _min_round[i]._cluster_id ) return &_my_info;

  ClusterNodeInfo* minpair = NULL;
  uint32_t min = UINT_MAX; //4,294,967,295
  //Rule 2.
  for ( i = 0; i < _max_distance; i++ ) {
    for ( j = 0; j < _max_distance; j++ ) {
      if ( ( _min_round[i]._cluster_id == _max_round[j]._cluster_id ) && ( _min_round[i]._cluster_id < min ) ) {
        minpair = &_min_round[i];
        min = _min_round[i]._cluster_id;
      }
    }
  }

  if ( minpair != NULL ) return minpair;

  //Rule 3.
  ClusterNodeInfo* maxnode = &_max_round[0];

  for ( i = 1; i < _max_distance; i++ )
    if ( _max_round[i]._cluster_id > maxnode->_cluster_id )
        maxnode = &_max_round[i];

  return maxnode;

}

// TODO: remove it
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

  sa << "Node: " << _node_identity->getNodeName() << " ID: " << _my_info._cluster_id;
  sa << " Clusterhead: " << getClusterHead()->_clusterhead.unparse() << " CH-ID: " << getClusterHead()->_cluster_id << "\n";

  sa << "Mode\tRound\tAddress\t\t\tDist\tHighest ID\n";

  for( r = 0; r < _max_distance; r++ ) {
    sa << "MAX\t" << r << "\t" << _max_round[r]._clusterhead.unparse() << "\t" << _max_round[r]._distance << "\t" << _max_round[r]._cluster_id << "\n";
  }

  for( r = 0; r < _max_distance; r++ ) {
    sa << "MIN\t" << r <<  "\t" << _min_round[r]._clusterhead.unparse() << "\t" << _min_round[r]._distance << "\t" << _min_round[r]._cluster_id << "\n";
  }

  return sa.take_string();
}

static String
read_stats_param(Element *e, void *)
{
  DCluster *dc = reinterpret_cast<DCluster*>(e);

  return dc->get_info();
}

void
DCluster::add_handlers()
{
  Clustering::add_handlers();

  add_read_handler("stats", read_stats_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DCluster)
