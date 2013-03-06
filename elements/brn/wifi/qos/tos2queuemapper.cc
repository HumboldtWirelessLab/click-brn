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
 * tos2queuemapper.{cc,hh}
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "tos2queuemapper.hh"

CLICK_DECLS



//TODO: wie kommen die Werte zustande?
static uint32_t tos2frac[] = { 63, 70, 77, 85 };
/* Backoff-Matrix is  generated with the help of the birthday paradoxon with the following values (see matlab/backoff/script_backoff_nachbarn_aprox.m; "matrix_merken"
   neighbours_min = 0
   neighbours_max = 40
   packet_loss = [0.1,0.15,0.2,0.25, 0.30,0.35,0.4,0.45,0.50,0.55]

   _backoff_matrix[packet_loss][neighbours]
*/

static const uint32_t _backoff_packet_loss[10] = { 10, 15, 20, 25, 30, 35, 40, 45, 50, 55 };
static const uint32_t _backoff_matrix[10][40]={
  {0,1,10,30,59,97,145,202,269,345,431,526,631,745,869,1002,1145,1297,1459,1630,1810,2001,2200,2409,2628,2856,3094,3341,3597,3863,4139,4424,4719,5023,5336,5659,5992,6334,6685,7046},
    {0,1,7,20,39,64,95,132,175,225,281,342,410,485,565,651,744,843,948,1059,1176,1300,1429,1565,1707,1855,2009,2169,2336,2508,2687,2872,3063,3260,3464,3673,3889,4111,4339,4573}, 
    {0,1,5,15,29,47,70,97,129,165,205,251,300,354,413,476,543,615,692,773,858,948,1043,1142,1245,1353,1465,1582,1704,1829,1960,2095,2234,2378,2526,2678,2836,2997,3163,3334},
    {0,1,5,12,23,37,55,76,100,129,160,195,234,276,321,370,423,479,538,601,667,737,811,887,968,1052,1139,1229,1324,1421,1522,1627,1735,1847,1962,2080,2202,2328,2457,2589},
    {0,1,4,10,19,30,44,62,82,104,130,158,189,223,260,300,342,387,435,486,540,596,655,717,782,850,920,993,1069,1148,1230,1314,1402,1492,1585,1680,1779,1880,1984,2091}, 
    {0,1,3,8,16,25,37,51,68,87,108,132,158,186,216,249,284,322,362,404,448,495,544,595,649,705,763,824,887,953,1020,1090,1162,1237,1314,1393,1475,1559,1645,1734},
    {0,1,3,7,13,22,32,44,58,74,92,112,134,157,183,211,241,272,306,341,379,418,460,503,549,596,645,697,750,805,862,921,982,1045,1110,1177,1246,1316,1389,1464},
    {0,1,3,6,12,19,27,38,50,64,79,96,115,135,157,181,206,234,262,293,325,359,394,431,470,511,553,597,642,689,738,789,841,895,950,1007,1066,1127,1189,1253},
    {0,1,3,6,10,17,24,33,43,55,69,83,100,117,136,157,179,202,227,253,281,310,341,373,407,442,478,516,555,596,638,682,727,773,821,870,921,974,1027,1082}, 
    {0,1,2,5,9,15,21,29,38,49,60,73,87,102,119,137,156,176,198,221,245,270,297,325,354,384,416,449,483,519,555,593,632,673,714,757,801,847,893,941}};


Tos2QueueMapper::Tos2QueueMapper():
    _cst(NULL),//Channelstats-Element
    _colinf(NULL),//Collission-Information-Element
    pli(NULL)//PacketLossInformation-Element
{
	_bqs_strategy = BACKOFF_STRATEGY_ALWAYS_OFF;
}

Tos2QueueMapper::~Tos2QueueMapper()
{
}

int
Tos2QueueMapper::configure(Vector<String> &conf, ErrorHandler* errh)
{
	String s_cwmin = "";
	String s_cwmax = "";
	String s_aifs = "";
	int v;

	if (cp_va_kparse(conf, this, errh,
	      	"CWMIN", cpkP, cpString, &s_cwmin,
	      	"CWMAX", cpkP, cpString, &s_cwmax,
	      	"AIFS", cpkP, cpString, &s_aifs,
	      	"CHANNELSTATS", cpkP, cpElement, &_cst,
	      	"COLLISIONINFO", cpkP, cpElement, &_colinf,
	      	"PLI", cpkP, cpElement, &pli,
	      	"DEBUG", cpkP, cpInteger, &_debug,
	      	cpEnd) < 0) return -1;

	Vector<String> args;
	cp_spacevec(s_cwmin, args);
  	no_queues = args.size();
  	if ( no_queues > 0 ) {
		_cwmin = new uint16_t[no_queues];
	    	_cwmax = new uint16_t[no_queues];
	    	_aifs = new uint16_t[no_queues];

	#pragma message "TODO: Better check for params. Better Error handling."
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	      		_cwmin[i] = v;
        }
        args.clear();
		cp_spacevec(s_cwmax, args);
	    	if ( args.size() < no_queues ) no_queues = args.size();
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	      		_cwmax[i] = v;
	    	}
        args.clear();
	    	cp_spacevec(s_aifs, args);
	    	if ( args.size() < no_queues ) no_queues = args.size();
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	     		_aifs[i] = v;
	    	}
        args.clear();
	}
  	_queue_usage = new uint32_t[no_queues];
	reset_queue_usage();
  	return 0;
}

void Tos2QueueMapper::no_queues_set(uint8_t number)
{
	no_queues = number;
}


uint8_t Tos2QueueMapper::no_queues_get()
{
	return	no_queues;
}

void Tos2QueueMapper::queue_usage_set(uint8_t position, uint32_t value)
{
	if (position < no_queues) _queue_usage[position] = value;
}


uint32_t Tos2QueueMapper::queue_usage_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_queue_usage[position];
}

void Tos2QueueMapper::cwmin_set(uint8_t position, uint32_t value)
{
	if (position < no_queues) _cwmin[position] = value;
}


uint32_t Tos2QueueMapper::cwmin_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues -1;
	return	_cwmin[position];
}

void Tos2QueueMapper::cwmax_set(uint8_t position, uint32_t value)
{
	if (position <= no_queues) _cwmax[position] = value;
}


uint32_t Tos2QueueMapper::cwmax_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_cwmax[position];
}


void Tos2QueueMapper::backoff_strategy_set(uint16_t value)
{
	_bqs_strategy = value;
}


uint16_t Tos2QueueMapper::backoff_strategy_get()
{
	return _bqs_strategy;
}

int Tos2QueueMapper::backoff_strategy_neighbours_pli_aware(Packet *p,uint8_t tos)
{
    unsigned int fraction  = 0;
    int number_of_neighbours = 0;
    int packet_loss_index_max = 10;
    int number_of_neighbours_max = 40;
    // Get Destination Address from the current packet
    struct click_wifi *wh = (struct click_wifi *) p->data();
    EtherAddress src;
    EtherAddress dst;
    EtherAddress bssid;
    switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
        case WIFI_FC1_DIR_NODS:
    	    dst = EtherAddress(wh->i_addr1);
    		src = EtherAddress(wh->i_addr2);
    		bssid = EtherAddress(wh->i_addr3);
        	break;
        case WIFI_FC1_DIR_TODS:
            bssid = EtherAddress(wh->i_addr1);
            src = EtherAddress(wh->i_addr2);
            dst = EtherAddress(wh->i_addr3);
            break;
        case WIFI_FC1_DIR_FROMDS:
            dst = EtherAddress(wh->i_addr1);
            bssid = EtherAddress(wh->i_addr2);
            src = EtherAddress(wh->i_addr3);
            break;
        case WIFI_FC1_DIR_DSTODS:
            dst = EtherAddress(wh->i_addr1);
            src = EtherAddress(wh->i_addr2);
            bssid = EtherAddress(wh->i_addr3);
            break;
        default:
            BRN_DEBUG("Packet-Mode is unknown");
    }
    // Get Fraction for inrange collisions for the current Destination Address
    if(NULL != pli) {
        BRN_DEBUG("Before pli_graph");
        PacketLossInformation_Graph *pli_graph = pli->graph_get(dst);
        BRN_DEBUG("AFTER pli_graph");
        if(NULL != pli_graph) {
            BRN_DEBUG("There is not a Graph available for the DST-Address: %s", dst.unparse().c_str());
            PacketLossReason* pli_reason = pli_graph->reason_get("in_range");
            fraction = pli_reason->getFraction();                    
            BRN_DEBUG("In-Range-Fraction := %d", fraction);
        }
        else {
            fraction = _backoff_packet_loss[packet_loss_index_max];
        }
    }
    else {
            fraction = _backoff_packet_loss[packet_loss_index_max];
    }
    //Get Number of Neighbours from the Channelstats-Element (_cst)
    if ( NULL != _cst ) {
	    struct airtime_stats *as;
		as = _cst->get_latest_stats(); // _cst->get_stats(&as,0);//get airtime statisics
        number_of_neighbours = as->no_sources;
		BRN_DEBUG("Number of Neighbours %d",number_of_neighbours);  
	}
    else {
        number_of_neighbours = number_of_neighbours_max;
    }
    // Evaluate Statistics and elect a Queue
    int index = -1;
    // Get from the Backoff-Packet-Loss-Table the supported fractions
    for (int i = 0; i<= packet_loss_index_max; i++){
        if(fraction <= _backoff_packet_loss[i]) index = i;
    }
    if (index == -1) index = packet_loss_index_max;
    if(number_of_neighbours > number_of_neighbours_max) number_of_neighbours = number_of_neighbours_max;
    // Get from Backoff-Matrix-Table for the current Fraction and the current number of neighbours the backoff-value
    unsigned int backoff_value = _backoff_matrix[fraction][number_of_neighbours];
    //Search with the calculated backoff-value the queue for the packet  
    int opt_queue = no_queues_get(); //init with the worst case queue 
    for (int i = 0; i <= no_queues_get();i++) {
	    BRN_DEBUG("cwmin[%d] := %d",i,cwmin_get(i));
		BRN_DEBUG("cwmax[%d] := %d",i,cwmax_get(i));
        // Take the first queue, whose cw-interval is in the range of the backoff-value
        if(cwmax_get(i) <= backoff_value){
            opt_queue = i;
            break;
        }
    }
    //note the tos-value from the user, to get more packetlosses, but a higher throughput and priority
    if ((tos > opt_queue) && (opt_queue < no_queues_get())) { // if tos-value is higher than opt_queue then modify opt_queue
        opt_queue = opt_queue + 1;
    }
    return opt_queue;
}

Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
    //TOS-Value from an application or user
  	uint8_t tos = BRNPacketAnno::tos_anno(p);
  	int opt_queue = tos;

	switch (backoff_strategy_get()) {
		case BACKOFF_STRATEGY_ALWAYS_OFF:
		break;
		case BACKOFF_STRATEGY_NEIGHBOURS_PLI_AWARE: 
        opt_queue = backoff_strategy_neighbours_pli_aware(p,tos);

        break;
		case  BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE: 
			if ( _colinf != NULL ) {
				if ( _colinf->_global_rs != NULL ) {
					uint32_t target_frac = tos2frac[tos];
					//find queue with min. frac of target_frac
					int ofq = -1;
					for ( int i = 0; i < no_queues; i++ ) {
						if (ofq == -1) {
							BRN_DEBUG("Foo");
							BRN_DEBUG("%p",_colinf->_global_rs);
							BRN_DEBUG("queu: %d",_colinf->_global_rs->get_frac(i));
							//int foo = _colinf->_global_rs->get_frac(i);
							if (_colinf->_global_rs->get_frac(i) >= target_frac) {
								if ( i == 0 ) {
									ofq = i;
								} else if ( _colinf->_global_rs->get_frac(i-1) == 0 ) { //TODO: ERROR-RETURN-VALUE = -1 is missing
									ofq = i - 1;
								} else {
									ofq = i;
								}
								} else {
									if ( _colinf->_global_rs->get_frac(i) >=  ( (9 * target_frac) / 10 ) ) {
										if ( i < (no_queues - 2) ) {
											ofq = 1 + 1;
										}
									}
								}
						}
					}
					if ( ofq == -1 ) opt_queue = no_queues - 1;
					else opt_queue = ofq;
				}
			} else if ( _cst != NULL ) {
				struct airtime_stats as;
				_cst->get_stats(&as,0);

				int opt_cwmin = get_cwmin(as.frac_mac_busy, as.no_sources);
				opt_queue = find_queue(opt_cwmin);

				int diff_q = (no_queues / 2) - tos - 1;
				opt_queue -= diff_q;

				if ( opt_queue < 0 ) opt_queue = 0;
				else if ( opt_queue > no_queues ) opt_queue = no_queues;
			}
		break;
	
  	}	
  	//trunc overflow
  	if ( opt_queue >= no_queues ) opt_queue = no_queues - 1;

  	//set queue
 	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  	BrnWifi::setTxQueue(ceh, opt_queue);
  	//add stats
  	_queue_usage[opt_queue]++;
  	return p;
}

int
Tos2QueueMapper::get_cwmin(int /*busy*/, int /*nodes*/) {
  return 10;
}

int
Tos2QueueMapper::find_queue(int cwmin) {
  for ( int i = 0; i < no_queues; i++ ) {
    if ( _cwmin[i] > cwmin ) return i;
  }

  return no_queues - 1;
}

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_ALWAYS_OFF,H_TOS2QUEUEMAPPER_ALWAYS_NCSA, H_TOS2QUEUEMAPPER_PLIA};

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  	Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  	switch ((uintptr_t) thunk) {
    	case H_TOS2QUEUEMAPPER_STATS:
      		StringAccum sa;
      		sa << "<queueusage queues=\"" << (uint32_t)td->no_queues_get() << "\" >\n";
      		for ( int i = 0; i < td->no_queues_get(); i++) {
        		sa << "\t<queue index=\"" << i << "\" usage=\"" << td->queue_usage_get(i) << "\" />\n";
      		}
      		sa << "</queueusage>\n";
      		return sa.take_string();
      	break;
  	}
  return String();
}

static int Tos2QueueMapper_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler*)
{
  	Tos2QueueMapper *f = (Tos2QueueMapper *)e;
  	String s = cp_uncomment(in_s);
 	switch((intptr_t)vparam) {
    		case H_TOS2QUEUEMAPPER_RESET: 
      			f->reset_queue_usage();
      		break;
    		case H_TOS2QUEUEMAPPER_ALWAYS_OFF: 
			f->backoff_strategy_set(BACKOFF_STRATEGY_ALWAYS_OFF);
		break;
    		case H_TOS2QUEUEMAPPER_ALWAYS_NCSA: 
			f->backoff_strategy_set(BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE);
		break;
    		case H_TOS2QUEUEMAPPER_PLIA: 
			f->backoff_strategy_set(BACKOFF_STRATEGY_NEIGHBOURS_PLI_AWARE);
		break;
      	}
	return 0;
}

void Tos2QueueMapper::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_read_handler("queue_usage", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_STATS);//STATS:=statistics

  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_RESET, Handler::h_button);
  add_write_handler("backoff_strategy_always_off",Tos2QueueMapper_write_param,H_TOS2QUEUEMAPPER_ALWAYS_OFF);
  add_write_handler("backoff_strategy_ncsa",Tos2QueueMapper_write_param, H_TOS2QUEUEMAPPER_ALWAYS_NCSA);// ncsa:=BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE
  add_write_handler("backoff_strategy_plia", Tos2QueueMapper_write_param, H_TOS2QUEUEMAPPER_PLIA);//plia:=BACKOFF_STRATEGY_NEIGHBOURS_PacketLossInformation_AWARE
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
