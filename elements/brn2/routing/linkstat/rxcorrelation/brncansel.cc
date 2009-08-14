#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brncansel.hh"
CLICK_DECLS

#define LPPACKVERSION

extern "C" {
  static int send_per_sorter(const void *va, const void *vb) {
      BRNCandidateSelector::CandidateInfo **a = (BRNCandidateSelector::CandidateInfo **)va;
      BRNCandidateSelector::CandidateInfo **b = (BRNCandidateSelector::CandidateInfo **)vb;

      if ((*a)->send_per < (*b)->send_per) return -1;
      if ((*a)->send_per > (*b)->send_per) return 1;
      return (0);
  }
}

/*
-Linkprobenummber
-Number of Neighbors (n)
-For each (n) neighbor
  >>Number of linkprobes (x)
  >>1.linkprobes
  >>(x-1) offsets
*/

/*
TODO
-remove linkprobeid-overflows ( "just" unsigned int ): add_recv_linkprobe,...
-send cand hints what you know (_he_knows_what_i_know)
-remove candidates which are not available anymore
*/

BRNCandidateSelector::BRNCandidateSelector()
{
}

BRNCandidateSelector::~BRNCandidateSelector()
{
}

int
BRNCandidateSelector::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, /*"ether address",*/ &_me,
      "DEBUG", cpkP+cpkM, cpInteger, /*"debug",*/ &_debug,
      "MAXPPPL", cpkP+cpkM, cpInteger, /*"maxlpperpacket",*/ &_max_lp_per_packet,
      "NOTE_LP", cpkP+cpkM, cpInteger, /*"notelinkprobe",*/ &_note_lp,
      cpEnd) < 0)
       return -1;
       
  click_chatter("My Address: %s",_me.unparse().c_str());
  return 0;
}

int
BRNCandidateSelector::initialize(ErrorHandler *)
{
  _debug = 0;
  _linkprobe_id = 0;
  _max_lp_per_packet = 20;
  _note_lp = 20;
  return 0;
}

int BRNCandidateSelector::add_recv_linkprobe(EtherAddress &_addr, unsigned int lp_id)
{
  int i;

  for ( i = 0; i < _cand.size(); i++ )
  {
    if ( _cand[i]->_addr == _addr )
    {
      _cand[i]->add_recv_linkprobe(lp_id, _linkprobe_id);
      break;
    }
  }

  if ( i == _cand.size() )                              // node not in list, so
  {
    CandidateInfo *_new_ci = new CandidateInfo(_addr, _note_lp, _linkprobe_id - 1);  // create new one
    _new_ci->add_recv_linkprobe(lp_id, _linkprobe_id);                 // add linkprobe_id
    _cand.push_back(_new_ci);                           // add to list
  }

  return 0;
}

/**
 * add linkprobeid which was recived by other nodes and sent by me
 * @param ptr 
 * @param dht_payload_size 
 * @return 
 */
int BRNCandidateSelector::add_mention_linkprobe(EtherAddress &node, unsigned int lp_id)
{
  int i;

  if ( lp_id > _linkprobe_id ) return 0; // bigger than last, have a restart since that linkprobe

  for ( i = 0; i < _cand.size(); i++ )
  {
    if ( _cand[i]->_addr == node )                                 //
    {
      _cand[i]->add_mention_linkprobe(lp_id);
      break;
    }
  }

  if ( i == _cand.size() )
  {
    CandidateInfo *_new_ci = new CandidateInfo(node, _note_lp, _linkprobe_id - 1);  // create new one
    _new_ci->add_mention_linkprobe(lp_id);                                          // add linkprobe_id
    _cand.push_back(_new_ci);                                                       // add to list
  }

  return 0;
}

unsigned int BRNCandidateSelector::writeCsPayloadToLinkProbe(uint8_t *ptr, unsigned int payload_size)
{
  uint16_t size;
  uint32_t val_32;
  uint16_t val_16;
  uint8_t  val_8;
  uint16_t lp_list_index;
  unsigned int first_lp;

//  click_chatter("-----------------------------------------Pack linkprobe info");

  _linkprobe_id++;
  size = 0;

  memcpy(ptr + size, &_linkprobe_id, sizeof(_linkprobe_id));                      // insert linkprobe-id
  size += sizeof(_linkprobe_id);

  val_8 = _cand.size();                                                           // insert number of candidates
  memcpy(ptr+size, &val_8, sizeof(val_8));
  size += sizeof(val_8);

  //click_chatter("node: %s can: %d",_me.unparse().c_str(),val_8);

  for( int i = 0; i < _cand.size(); i++ )
  {

#ifdef LPPACKVERSION
    /* detect which linkprobes id he sends me i will send him */

    if ( _cand[i]->_i_got_from_him.size() > _max_lp_per_packet )                   // not more than _max_lp_per_packet
      lp_list_index = _cand[i]->_i_got_from_him.size() - _max_lp_per_packet;
    else
      lp_list_index = 0;

    for ( ; lp_list_index < _cand[i]->_i_got_from_him.size(); lp_list_index++)      // not linkprobe-id he knows that i got them
    {
      if ( _cand[i]->_i_got_from_him[lp_list_index] > _cand[i]->_he_knows_what_i_got ) break;
    }

    memcpy(ptr + size, _cand[i]->_addr.data(), 6);                                  // MAC of the Candidate
    size += 6;

    if ( _cand[i]->_he_got_from_me.size() > 0 )
      val_32 = _cand[i]->_he_got_from_me[(_cand[i]->_he_got_from_me.size() - 1)];   // write him the highest id you know that he got
    else
      val_32 = 0;                                                                   // 0 if you know nothing

    memcpy(ptr + size, &val_32, sizeof(val_32));
    size += sizeof(val_32);

    val_16 = _cand[i]->_i_got_from_him.size() - lp_list_index;                      // number of linkprobes
    memcpy(ptr + size, &val_16, sizeof(val_16));
    size += sizeof(val_16);

    if ( lp_list_index < _cand[i]->_i_got_from_him.size() )
    {
      first_lp = _cand[i]->_i_got_from_him[lp_list_index];
      memcpy(ptr + size, &first_lp, sizeof(first_lp));
      size += sizeof(first_lp);
      lp_list_index++;                                                           // copy the rest

      for ( ;lp_list_index < _cand[i]->_i_got_from_him.size(); lp_list_index++)
      {
        val_8 = _cand[i]->_i_got_from_him[lp_list_index] - first_lp;             // offset only
        memcpy(ptr + size, &val_8, sizeof(val_8));
        size += sizeof(val_8);
        
      }
    }
#else  //new packversion for linkprobe to be able to put more information into one lp
  if ( ( _cand[i]->_i_got_from_him[_cand[i]->_i_got_from_him.size() - 1] - _cand[i]->_i_got_from_him[_cand[0] ) > _max_lp_per_packet )
  {
  }
  else
  {
  }

#endif
    if ( size > payload_size )
    {
      click_chatter("----------------------+++++++++++++++too big");
    }

  }
//  click_chatter("---------------------------------payload_size: %d",size);

  return(size);
}

void BRNCandidateSelector::readCsPayloadFromLinkProbe(EtherAddress &source, uint8_t *ptr, unsigned int /*payload_size*/)
{
  uint16_t size;
  uint32_t val_32;
  uint16_t val_16;
  uint8_t  val_8;
  unsigned int first_lp;
  unsigned int src_id;
  unsigned int cand_size;
  EtherAddress cand_addr;
  unsigned int count_lp;
  unsigned int he_knows;
  int cand_i;

  //click_chatter("---------------------------------------------------Unpack linkprobeinfo");

  size = 0;
  memcpy(&src_id, ptr + size, sizeof(src_id));
  size += sizeof(src_id);

  click_chatter("LINKPROBEID: ME: %s SRC: %s id: %d",_me.unparse().c_str(),source.unparse().c_str(),src_id);

  add_recv_linkprobe(source, src_id);

  memcpy(&val_8, ptr + size, sizeof(val_8));
  size += sizeof(val_8);
  cand_size = val_8;

  //click_chatter("----------------------------node %s: Linkprobe from %s with %d Candidates",_me.unparse().c_str(),source.unparse().c_str(),cand_size);

  for( unsigned int i = 0; i < cand_size; i++ )
  {

    cand_addr = EtherAddress(ptr + size);
    size += 6;

    memcpy(&val_32, ptr + size, sizeof(val_32));
    size += sizeof(val_32);
    he_knows = val_32;

    memcpy(&val_16, ptr + size, sizeof(val_16));
    size += sizeof(val_16);
    count_lp = val_16;

    //click_chatter("----------------------------node %s: candidate %s with %d Linkprobes",_me.unparse().c_str(),cand_addr.unparse().c_str(),count_lp);

    if ( cand_addr == _me )
    {
      for ( cand_i = 0; cand_i < _cand.size(); cand_i++ )
      {
        if ( _cand[cand_i]->_addr == source )
        {
          if ( _cand[cand_i]->_he_knows_what_i_got <= he_knows ) _cand[cand_i]->_he_knows_what_i_got = he_knows;
          else click_chatter("BRNCandidateSelector::readCsPayloadFromLinkProbe: I thought he knows more");
          break;
        }
      }

      if ( cand_i == _cand.size() )
      {
        CandidateInfo *_new_ci = new CandidateInfo(source, _note_lp, _linkprobe_id - 1);  // create new one
        _new_ci->_he_knows_what_i_got = he_knows;
        _cand.push_back(_new_ci);                           // add to list
      }

      if ( count_lp > 0 )
      {

        //click_chatter("---------------------------node %s: thats me. First LP: %d",_me.unparse().c_str(),first_lp);

        memcpy(&first_lp, ptr + size, sizeof(first_lp));
        size += sizeof(first_lp);

        add_mention_linkprobe(source, first_lp);

        for ( unsigned int i = 1; i < count_lp; i++)
        {
          memcpy(&val_8, ptr + size, sizeof(val_8));
          size += sizeof(val_8);

          add_mention_linkprobe(source, first_lp + val_8);
        }
      }
    }
    else
    {
      if ( count_lp > 0 )
        size += sizeof(unsigned int) + ( (count_lp - 1) * sizeof(uint8_t));
    }
  }

  return;
}

uint8_t BRNCandidateSelector::has_lp(Vector<unsigned int> &lp_list, int lp_id)
{
  for( int i = 0; i < lp_list.size(); i++ )
  {
    if ( lp_id == (int)lp_list[i] ) return(1);
    if ( lp_id < (int)lp_list[i] ) return(0);
  }

  return(0);
}

int BRNCandidateSelector::calculatedRecvPER(CandidateInfo *cand)
{
  return calculatedRecvPER(cand,_note_lp);
}

int BRNCandidateSelector::calculatedRecvPER(CandidateInfo *cand, int number_of_calc_lp)
{
  int highest_lp_id, lowest_lp_id, start_lp_id, get_lp,j,count_missed_lp;
  struct timeval _timenow;
  long timediff_in_ms;

  _timenow = Timestamp::now().timeval();
  timediff_in_ms = cand->diff_in_ms(_timenow,cand->_last_seen);
  count_missed_lp = timediff_in_ms / cand->_lp_interval_in_ms;  //calculate missed lp ( not received but send

  if ( cand->_i_got_from_him.size() > 0 )
  {
    highest_lp_id = cand->_i_got_from_him[ ( cand->_i_got_from_him.size() ) - 1];    // highest LP_is
    highest_lp_id += count_missed_lp;                                                // at missed lp since last lp
    lowest_lp_id = cand->_i_got_from_him[0];                                         // lowest LP_is

      if ( lowest_lp_id > ( highest_lp_id - number_of_calc_lp ) ) start_lp_id = lowest_lp_id;
      else start_lp_id = highest_lp_id - number_of_calc_lp;

      for (get_lp = 0, j = 0; j <= highest_lp_id - start_lp_id; j ++)
      {
          get_lp += has_lp(cand->_i_got_from_him, start_lp_id + j);
      }

      return( 100 - ( ( get_lp * 100 ) / ( highest_lp_id - start_lp_id + 1 ) ) );

  }
  else
    return(100);
}

int BRNCandidateSelector::calculatedPER(CandidateInfo *cand)
{
  return calculatedPER(cand,_note_lp);
}

int BRNCandidateSelector::calculatedPER(CandidateInfo *cand, int number_of_calc_lp)
{
  int highest_lp_id, lowest_lp_id, start_lp_id, get_lp,j;

  if ( cand->_he_got_from_me.size() > 0 )
  {
    highest_lp_id = cand->_he_got_from_me[ ( cand->_he_got_from_me.size() ) - 1];    // highest LP_is
    if ( (int)(cand->_last_lp_he_was_able_to_hear) > highest_lp_id )                        // my last lp he could hear if it
      highest_lp_id = cand->_last_lp_he_was_able_to_hear;                            // higher(should be every time)
    lowest_lp_id = cand->_he_got_from_me[0];                                         // highest LP_is

      if ( lowest_lp_id > ( highest_lp_id - number_of_calc_lp ) ) start_lp_id = lowest_lp_id;
      else start_lp_id = highest_lp_id - number_of_calc_lp;

      for (get_lp = 0, j = 0; j <= highest_lp_id - start_lp_id; j ++)
      {
          get_lp += has_lp(cand->_he_got_from_me, start_lp_id + j);
      }

      return( 100 - ( ( get_lp * 100 ) / ( highest_lp_id - start_lp_id + 1 ) ) );

  }
  else
    return(100);
}

/**
 * 
 * @param sel_cand list of candidates used for the calculation
 * @param number_of_calc_lp number of linkprobes used for the calculation
 * @return 
 */
int BRNCandidateSelector::calculatedCombinedPER(Vector<CandidateInfo*> &sel_cand, int number_of_calc_lp)
{
  unsigned int highest_common_lp_id;
  unsigned int lowest_common_lp_id;
  unsigned int start_common_lp_id;
  int j;
  int get_lp;

  uint8_t *common_lps;
  unsigned int cand_highest_lp;

  if ( sel_cand.size() > 0 )
  {

    if ( sel_cand[0]->_he_got_from_me.size() == 0 ) return(100);

    highest_common_lp_id = sel_cand[0]->_he_got_from_me[ ( sel_cand[0]->_he_got_from_me.size() ) - 1];    // highest LP_is of the first cand
    if ( highest_common_lp_id < sel_cand[0]->_last_lp_he_was_able_to_hear ) highest_common_lp_id = sel_cand[0]->_last_lp_he_was_able_to_hear;
    lowest_common_lp_id = sel_cand[0]->_he_got_from_me[0];                                               // highest LP_is of the first cand

    for( int i = 1; i < sel_cand.size(); i++ )                                                           // no for the others
    {
      if ( sel_cand[i]->_he_got_from_me.size() == 0 ) return(100);

      cand_highest_lp = sel_cand[i]->_he_got_from_me[ ( sel_cand[i]->_he_got_from_me.size() ) - 1 ];
      if ( cand_highest_lp < sel_cand[i]->_last_lp_he_was_able_to_hear ) cand_highest_lp = sel_cand[i]->_last_lp_he_was_able_to_hear;

      if ( cand_highest_lp < highest_common_lp_id ) highest_common_lp_id = cand_highest_lp;

      if ( sel_cand[i]->_he_got_from_me[0] > lowest_common_lp_id )
        lowest_common_lp_id = sel_cand[i]->_he_got_from_me[0];
    }

  //  click_chatter("lid %d hid %d",lowest_common_lp_id, highest_common_lp_id );
  
    if ( lowest_common_lp_id > highest_common_lp_id )
    {
      click_chatter("not able to calcuate Combined PER ( com lowest id > com highest id)");
    }
    else
    {
      if ( lowest_common_lp_id > ( highest_common_lp_id - number_of_calc_lp ) ) start_common_lp_id = lowest_common_lp_id;
      else start_common_lp_id = highest_common_lp_id - number_of_calc_lp;

      common_lps = new uint8_t[highest_common_lp_id - start_common_lp_id + 1];
      memset(common_lps, 0, highest_common_lp_id - start_common_lp_id + 1);

      for( int i = 0; i < sel_cand.size(); i++)
      {
        for ( j = 0; j <= ((int)highest_common_lp_id - (int)start_common_lp_id); j ++)
        {
          common_lps[ j ] = common_lps[ j ] || has_lp(sel_cand[i]->_he_got_from_me, start_common_lp_id + j);
        }
      }

      for ( get_lp = 0, j = 0; j <= ((int)highest_common_lp_id - (int)start_common_lp_id); j ++)
      {
        get_lp += common_lps[ j ];
    //    click_chatter("---: %d %d %d",has_lp(sel_cand[0]->_he_got_from_me, start_common_lp_id + j),has_lp(sel_cand[1]->_he_got_from_me, start_common_lp_id + j),common_lps[ j ]);
      }

      delete common_lps;

      return( 100 - ( ( get_lp * 100 ) / ( highest_common_lp_id - start_common_lp_id + 1 ) ) );

    }

  }
  else
  {
    return 100;
  }

  return 100;
}

/****************************************Algorithm A**********************************/
/* Algorithm takes the best candidates with the lowest combined PER (Packet Error Rate.
   Therefore it combines the possible Candidates to Sets of x and calculates the Combined PER.
   In the end, it takes the best set.
*/

int BRNCandidateSelector::getAndTestPossibleCS(Vector<CandidateInfo*> *can,int cssize, int *per, int ac_pos, Vector<CandidateInfo*> *best_can, int number_of_calc_lp)
{
  int ac_per;

  
  if ( can->size() == cssize )
  { // calculate and set

      ac_per = calculatedCombinedPER(*can, number_of_calc_lp);

      if ( ac_per < *per )
      {
        best_can->clear();
        for( int i = 0; i < can->size(); i++)
        {
          best_can->push_back((*can)[i]);
        }

        *per = ac_per;
      }
  }
  else
  {
    for( int i = ac_pos; i < _cand.size(); i++ )
    {
      //test next (recursive)
      can->push_back(_cand[i]);
      getAndTestPossibleCS(can, cssize, per, /*ac_pos + 1*/i + 1,best_can, number_of_calc_lp);
      can->erase(can->begin() + i);
    }
  }

  return 0;
}

int BRNCandidateSelector::getCandidateSet(Vector<CandidateInfo*> *can, int cssize, int *per)
{
  Vector<CandidateInfo*> cs_try;

  *per = 101;                                                                               // set higher as possible for start#

  if  ( cssize <=_cand.size() )
  {
    getAndTestPossibleCS(&cs_try, cssize, per, 0, can, _note_lp);

    return(0);
  }

  return(-1);
}

/****************************************Algorithm B**********************************/
/* Algorithm takes the best candidates with the lowest PER (Packet Error Rate.
   Therefore is sorts the list by PER and takes the first X one.
*/

int BRNCandidateSelector::getBestCandidates(CandidateInfo **cand, int number_of_cand)
{
  for( int i = 0; i < _cand.size(); i++ ) _cand[i]->send_per = calculatedPER(_cand[i]);

  click_qsort(_cand.begin(), _cand.size(), sizeof(CandidateInfo *), send_per_sorter);

  for( int i = 0; i < number_of_cand; i++ ) cand[i] = _cand[i];

  return(0);
}

/****************************************Algorithm C**********************************/
/* Algorithm is very complex. it also takes the PER between the candidates into account.
   it now allows only 2 Candidates.
   the values are the recive-probability so i have to use (100 - per)
*/
/*
int BRNCandidateSelector::getBestCandidates_C(CandidateInfo **cand, int number_of_cand)
{
  int calc_per;
  int best_can[2];
      // channel matrix
      //   | A  B  C
      // -----------
      // A | 1  1  1
      // B | 1  1  1
      // C | 1  1  1
      //c = [
      //    1     0.8   0.5;
      //    0.8   1     0.9;
      //    0.5   0.9   1
      //    ];

      //x1 = AB*AC * CB*(1-CA) * (1-BA)

  for( int i = 0; i < _cand.size(); i++ )
  {
    _cand[i]->send_per = calculatedPER(_cand[i]);
    _cand[i]->send_per = calculatedRecvPER(_cand[i]);
  }

  for( int i = 0; i < ( _cand.size() - 1 ); i++ )
  {
    for( int j = ( i + 1 ); j < _cand.size(); j++ )
    {
      //x1 = AB*AC * CB*(1-CA) * (1-BA)  // c[0][1]*c[0][2] * c[2][1]*(1-c[2][0]) * (1-c[1][0]);

      int x1 = ( ( ( 100 - _cand[i]->send_per ) * ( 100 - _cand[j]->send_per ) ) / 100 ) * ( ( 100  * ( cand[j]->recv_per ) * ( _cand[i]->recv_per ) ) / 10000 );

      //x2 = AB*AC * (1-CB)*(1-CA)*(1-BA)  // double x2 = c[0][1]*c[0][2] * (1-c[2][1])*(1-c[2][0]) * (1-c[1][0]);

      int x2 = 0; // since ( 1 - CB = 0 )

      //x3 = AB*(1-AC) * (1-BA)

      double x3 = c[0][1]*(1-c[0][2]) * (1-c[1][0]);

      //x4 = (1-AB)*AC * CB*(1-CA) * (1-BA)

      double x4 = (1-c[0][1])*c[0][2] * c[2][1]*(1-c[2][0]) * (1-c[1][0]);

      //x5 = (1-AB)*AC * (1-CB)*(1-CA)//   double x5 = (1-c[0][1])*c[0][2] * (1-c[2][1])*(1-c[2][0]);

      int x5 = 0; // since ( 1 - CB = 0 )

      //x6 = (1-AB)*(1-AC)

      double x6 = (1-c[0][1])*(1-c[0][2]);

      calc_per = x1 + x2 + x3 + x4 + x5 + x6;

      if ( calc_per < best_per )
      {
        best_can[0] = i;
        best_can[1] = j;
      }

    }
  }

  return (x1 + x2 + x3 + x4 + x5 + x6);

}

*/


String BRNCandidateSelector::printCandidateInfo()
{
  StringAccum sa;
  CandidateInfo *_ci;

  Vector<CandidateInfo*> sel_cand;
  int per,j;

  click_chatter("PrintInfo");

  sa << "CandidateInfo ( Node: " << _me.unparse() << " )\n";

  for (int i = 0; i < _cand.size(); i++ )
  {
    _ci = _cand[i];
    sa << "Candidate: " << _ci->_addr.unparse() << "\n";
    sa << " i got beacon (" << _ci->_i_got_from_him.size() << "): ";
    for ( j = 0; j < _ci->_i_got_from_him.size(); j++ )
    {
      sa << _ci->_i_got_from_him[j];
      if ( j < ( _ci->_i_got_from_him.size() - 1 ) )
        sa << ", ";
      else
        sa << "\n";
    }

    sa << " he got beacon (" << _ci->_he_got_from_me.size() << "): ";
    for ( j = 0; j < _ci->_he_got_from_me.size(); j++ )
    {
      sa << _ci->_he_got_from_me[j];
      if ( j < ( _ci->_he_got_from_me.size() - 1 ) )
        sa << ", ";
      else
        sa << "\n";
    }
  }

  for (int i = 0; i < _cand.size(); i++ )
  {
    for( j = 0; j < _cand.size(); j++ )
    {
      if ( j != i )
      {
        sel_cand.push_back(_cand[i]);
        sel_cand.push_back(_cand[j]);
        per = calculatedCombinedPER(sel_cand, _note_lp);
        sel_cand.clear();
        sa << "Combi PER : " << _cand[i]->_addr.unparse() << " <-> " << _cand[j]->_addr.unparse() << " : " << per << "\n";
      }
    }
  }

  return sa.take_string();
}

String BRNCandidateSelector::printPerInfo()
{
  StringAccum sa;
  //CandidateInfo *_ci;

  Vector<CandidateInfo*> sel_cand;
  //int per,j;

  for (int i = 0; i < _cand.size(); i++ )
  {
    sa << _cand[i]->_addr.unparse() << " -> ";
    sa << calculatedPER(_cand[i]) << "\t";
  }
  
  sa << "\n";

  return sa.take_string();
}

String BRNCandidateSelector::printRecvPerInfo()
{
  StringAccum sa;
 // CandidateInfo *_ci;

  Vector<CandidateInfo*> sel_cand;
 // int per,j;

  for (int i = 0; i < _cand.size(); i++ )
  {
    sa << _cand[i]->_addr.unparse() << " -> ";
    sa << calculatedRecvPER(_cand[i]) << "\t";
  }
  
  sa << "\n";

  return sa.take_string();
}

static String
read_handler_candidates(Element *e, void *)
{
  return ((BRNCandidateSelector*)e)->printCandidateInfo();
}

static String
read_handler_per(Element *e, void *)
{
  return ((BRNCandidateSelector*)e)->printPerInfo();
}

static String
read_handler_recv_per(Element *e, void *)
{
  return ((BRNCandidateSelector*)e)->printRecvPerInfo();
}

static int 
write_handler_debug(const String &in_s, Element *e, void *,ErrorHandler *errh)
{
  BRNCandidateSelector *brncansec = (BRNCandidateSelector*)e;
  String s = cp_uncomment(in_s);

  int debug;

  if (!cp_integer(s, &debug))
    return errh->error("Debug is Integer");

  brncansec->_debug = debug;

  return 0;
}

/**
 * Read the debugvalue
 * @param e 
 * @param  
 * @return 
 */
static String
read_handler_debug(Element *e, void *)
{
  BRNCandidateSelector *brn2debug = (BRNCandidateSelector*)e;

  return String(brn2debug->_debug);
}

void
BRNCandidateSelector::add_handlers()
{
  add_read_handler("candidates", read_handler_candidates, 0);
  add_read_handler("per", read_handler_per, 0);
  add_read_handler("recvper", read_handler_recv_per, 0);

  add_read_handler("debug", read_handler_debug, 0);
  add_write_handler("debug", write_handler_debug, 0);
}

#include <click/vector.cc>
template class Vector<BRNCandidateSelector::CandidateInfo*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNCandidateSelector)
