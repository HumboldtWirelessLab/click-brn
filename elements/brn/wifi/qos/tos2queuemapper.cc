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

static const int32_t _backoff_matrix_tmt_backoff_4D[4][4][25][5]={{{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}}},{{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}}},{{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}}},{{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}},{{113,38,18,8,4},{358,120,59,27,15},{707,257,118,52,30},{1189,442,199,90,50},{1815,685,307,131,78},{2502,931,435,189,109},{2992,1229,587,249,148},{-1,1607,765,328,186},{-1,2053,964,410,241},{-1,2487,1128,507,287},{-1,2999,1376,605,354},{-1,3000,1694,730,416},{-1,-1,1917,818,481},{-1,-1,2187,975,555},{-1,-1,2561,1100,652},{-1,-1,2856,1255,730},{-1,-1,3000,1415,807},{-1,-1,-1,1620,906},{-1,-1,-1,1746,1008},{-1,-1,-1,1924,1118},{-1,-1,-1,2116,1225},{-1,-1,-1,2326,1347},{-1,-1,-1,2562,1467},{-1,-1,-1,2764,1628},{-1,-1,-1,2982,1743}}}};
static const int32_t _backoff_matrix_tmt_backoff_3D[4][4][25]={{{30,49,75,92,114,140,166,180,216,225,248,259,299,288,361,360,339,419,431,443,418,486,503,535,576},{57,73,127,149,216,235,245,291,324,370,394,440,507,502,563,531,679,701,673,766,741,848,800,900,821},{57,113,177,231,258,304,396,366,463,549,553,651,637,717,811,878,858,950,963,952,1057,1148,1080,1201,1283},{120,213,241,390,530,519,579,706,714,914,988,894,1055,1112,1181,1357,1336,1586,1558,1615,1571,1827,1780,1906,1992}},{{19,34,49,63,79,89,95,119,149,143,159,163,179,221,213,230,245,250,278,264,281,320,317,345,354},{30,49,75,106,127,140,166,180,216,225,248,259,299,288,361,360,339,419,431,456,523,486,539,535,576},{41,73,94,125,152,214,227,269,292,338,368,384,401,474,480,531,514,571,605,622,676,686,727,749,821},{57,113,177,231,258,304,396,366,463,549,520,651,637,717,811,878,858,806,963,952,1057,1039,1080,1201,1283}},{{9,19,28,31,44,50,60,60,79,89,96,96,106,122,124,134,132,150,152,171,178,178,190,192,202},{15,28,40,51,63,68,95,103,106,117,137,155,163,160,186,195,197,220,241,250,261,255,274,293,310},{19,36,60,79,79,89,112,145,149,170,178,211,226,221,252,248,260,290,278,310,352,364,368,394,420},{30,73,94,108,152,165,190,205,216,265,285,316,330,362,361,417,458,465,472,495,523,582,594,605,638}},{{7,13,22,31,30,41,46,49,61,63,73,81,83,96,93,106,103,119,124,131,139,153,144,162,171},{11,21,28,39,44,56,60,74,79,94,96,112,114,122,134,134,155,163,185,180,178,193,206,218,222},{15,28,40,49,63,68,95,94,106,117,137,155,163,160,167,195,197,213,220,250,232,255,274,293,310},{26,36,60,79,86,119,122,150,173,183,203,211,234,252,252,275,339,348,330,375,352,414,432,443,473}}};

static const int32_t _backoff_matrix_birthday_problem_intuitv[5][25]={{50,99,148,198,247,297,346,396,445,495,544,594,643,693,742,792,841,891,940,990,1039,1089,1138,1188,1237},{20,39,58,78,97,117,136,156,175,195,214,234,253,273,292,312,331,351,370,390,409,429,448,468,487},{9,19,28,38,47,57,66,76,85,95,104,114,123,133,142,152,161,171,180,190,199,209,218,228,237},{4,9,13,18,22,27,31,36,40,45,49,54,58,63,67,72,76,81,85,90,94,99,103,108,112},{3,6,8,11,14,17,20,22,25,28,31,34,36,39,42,45,48,50,53,56,59,62,64,67,70}};

static const int32_t _backoff_packet_loss[5]={2,5,10,20,30};

static const int32_t vector_msdu_sizes[4] = {500, 1500, 3000, 8000};
static const int32_t vector_rates_data[4] = {1,6,24,54};
static const int32_t vector_no_neighbours[25] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};

Tos2QueueMapper::Tos2QueueMapper():
    _cst(NULL),    //Channelstats-Element
    _colinf(NULL), //Collission-Information-Element
    pli(NULL),      //PacketLossInformation-Element
    _bqs_strategy(BACKOFF_STRATEGY_OFF)
{
  BRNElement::init();
  _likelihood_collison = -1;
  _rate = -1;
  _msdu_size = -1;
  _index_packet_loss_max = 5;
  _index_number_of_neighbours_max = 25;
  _index_msdu_size_max = 4;
  _index_rates_max = 4;
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
                "LIKELIHOODCOLLISON", cpkP, cpInteger, &_likelihood_collison,
                "RATE", cpkP, cpInteger, &_rate,
                "MSDUSIZE", cpkP, cpInteger, &_msdu_size,
		"STRATEGY", cpkP, cpInteger, &_bqs_strategy,
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
	
	
	
	for ( int i = 0; i < 25; i++) {
	  BRN_ERROR("N: %d backoff: %d",i,_backoff_matrix_tmt_backoff_3D[0 /*rate 1*/][1/*msdu 1500*/][i]);
	}
  	return 0;
}

uint8_t Tos2QueueMapper::no_queues_get()
{
	return	no_queues;
}

uint32_t Tos2QueueMapper::queue_usage_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_queue_usage[position];
}

uint32_t Tos2QueueMapper::cwmin_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues -1;
	return	_cwmin[position];
}

uint32_t Tos2QueueMapper::cwmax_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_cwmax[position];
}

int Tos2QueueMapper::backoff_strategy_neighbours_pli_aware(Packet *p,uint8_t tos)
{
    int32_t first_time = 0;
    //uint32_t fraction  = 0; veraltet
    int32_t number_of_neighbours = 0;
    int32_t  index_search_rate = -1;
    int32_t  index_search_msdu_size = -1;
    int32_t  index_search_likelihood_collision = -1;
    int32_t  index_no_neighbours = -1;
    uint32_t  backoff_window_size = 0;
    int backoff_window_size_2 = -1;
    int opt_queue = -1;
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
    // Get Fraction for inrange collisions for the current Destination Address  Code veraltet
    /*if(NULL != pli) {
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
    }*/
    //Get Number of Neighbours from the Channelstats-Element (_cst)
    if ( NULL != _cst ) {
	    struct airtime_stats *as;
		as = _cst->get_latest_stats(); // _cst->get_stats(&as,0);//get airtime statisics
        number_of_neighbours = as->no_sources;
		BRN_DEBUG("Number of Neighbours %d", number_of_neighbours);  
	}
    if (_rate > 0){
        for (int i = 0; i <= _index_rates_max;i++) {
            if (vector_rates_data[i] == _rate) {
                index_search_rate = i;
            }
        }
    }
    if (_msdu_size > 0){
        for (int i = 0; i <= _index_msdu_size_max;i++) {
            if (vector_msdu_sizes[i] == _msdu_size) {
                index_search_msdu_size = i;
            }
        }
    }
    if (_likelihood_collison > 0){
        for (int i = 0; i <= _index_packet_loss_max;i++) {
            if (_backoff_packet_loss[i] == _likelihood_collison) {
                index_search_likelihood_collision = i;
            }
        }
    }
    if (number_of_neighbours > 0){
        for (int i = 0; i <= _index_number_of_neighbours_max;i++) {
            if (vector_no_neighbours[i] == number_of_neighbours) {
                index_no_neighbours = i;
            }
        }
    }
    // Tests what is known
    if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_search_likelihood_collision >= 0 && index_no_neighbours >= 0 && first_time == 0) {
       backoff_window_size_2 = _backoff_matrix_tmt_backoff_4D[index_search_rate][index_search_msdu_size][index_no_neighbours][index_search_likelihood_collision];
    } else if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_no_neighbours >= 0 ) {
        //max Throughput
        //BRN_WARN("Search max tp");
        backoff_window_size = _backoff_matrix_tmt_backoff_3D[index_search_rate][index_search_msdu_size][index_no_neighbours];
    } else if ( index_search_likelihood_collision >= 0 && index_no_neighbours >= 0 ) {
      backoff_window_size = _backoff_matrix_birthday_problem_intuitv[index_search_likelihood_collision][index_no_neighbours];
    }

    if (backoff_window_size_2 > -1) backoff_window_size = backoff_window_size_2;

    if ( backoff_window_size != 0 ) {
        // Evaluate Statistics and elect a Queue
        //int index = -1; veraltet
        // Get from the Backoff-Packet-Loss-Table the supported fractions
        //for (int i = 0; i<= packet_loss_index_max; i++){ //veraltet
        //if(fraction <= _backoff_packet_loss[i]) index = i; //veraltet
        //} //veraltet
        //if (index == -1) index = packet_loss_index_max; //veraltet
        //if(number_of_neighbours > number_of_neighbours_max) number_of_neighbours = number_of_neighbours_max; // veraltet
        // Get from Backoff-Matrix-Table for the current Fraction and the current number of neighbours the backoff-value
        //unsigned int backoff_value = _backoff_matrix_birthday_problem_intuitv[fraction][number_of_neighbours];//veraltet
        //Search with the calculated backoff-value the queue for the packet  
        opt_queue = no_queues_get()-1; //init with the worst case queue 
        for (int i = 0; i <= no_queues_get()-1;i++) {
	        BRN_DEBUG("cwmin[%d] := %d",i,cwmin_get(i));
		BRN_DEBUG("cwmax[%d] := %d",i,cwmax_get(i));
            // Take the first queue, whose cw-interval is in the range of the backoff-value
            if (backoff_window_size >= cwmin_get(i) && backoff_window_size < cwmin_get(i+1)){
                opt_queue = i+1;
                break;
            }
        }
        // note the tos-value from the user, to get more packetlosses, but a higher throughput and priority
        // if tos-value is higher than opt_queue then modify opt_queue
        /*if ((tos > opt_queue) && (opt_queue < no_queues_get())) { 
            opt_queue = opt_queue + 1;
        }*/
        BRN_DEBUG("backoffwin: %d  Queue: %d", backoff_window_size, opt_queue);
    } else {
        if (tos > 0){
            if (tos == 1) {
                opt_queue = 0;
            }
            else if (tos == 2) {
                opt_queue = 2;
            }
            else if (tos == 3) {
                opt_queue = 3;
            }
        }
        else if (tos == 0) {
            opt_queue = 1;
        }
    }
    return opt_queue;
}

Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
    //TOS-Value from an application or user
  	uint8_t tos = BRNPacketAnno::tos_anno(p);
  	int opt_queue = tos;

	switch (_bqs_strategy) {
		case BACKOFF_STRATEGY_OFF:
		  return p;
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
							BRN_DEBUG("queue: %d",_colinf->_global_rs->get_frac(i));
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

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_STRATEGY};

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  	Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  	switch ((uintptr_t) thunk) {
    	case H_TOS2QUEUEMAPPER_STATS:
      		StringAccum sa;
      		sa << "<queueusage strategy=\"" << td->_bqs_strategy << "\" queues=\"" << (uint32_t)td->no_queues_get() << "\" >\n";
      		for ( int i = 0; i < td->no_queues_get(); i++) {
        		sa << "\t<queue index=\"" << i << "\" usage=\"" << td->queue_usage_get(i) << "\" />\n";
      		}
      		sa << "</queueusage>\n";
      		return sa.take_string();
      	break;
  	}
  return String();
}

static int Tos2QueueMapper_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  	Tos2QueueMapper *f = (Tos2QueueMapper *)e;
  	String s = cp_uncomment(in_s);
	Vector<String> args;
	cp_spacevec(s, args);

 	switch((intptr_t)vparam) {
    		case H_TOS2QUEUEMAPPER_RESET: 
      			f->reset_queue_usage();
      		        break;
    		case H_TOS2QUEUEMAPPER_STRATEGY: 
		        int st;
		        if (!cp_integer(args[0],&st)) return errh->error("strategy parameter must be integer");
			f->set_backoff_strategy(st);
		        break;
      	}
	return 0;
}

void Tos2QueueMapper::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_read_handler("queue_usage", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_STATS);//STATS:=statistics

  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_RESET, Handler::h_button);
  add_write_handler("strategy",Tos2QueueMapper_write_param, (void *)H_TOS2QUEUEMAPPER_STRATEGY);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
