#ifndef BRNCANDIDATESELECTOR_HH
#define BRNCANDIDATESELECTOR_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include "elements/brn/routing/linkstat/brnlinktable.hh"

CLICK_DECLS

/*
 * =c
 * BRNCandidateSelector()
 * =s
 * 
 * =d
 */

class BRNCandidateSelector : public Element {

public:

 class CandidateInfo
 {
   public:

   EtherAddress _addr;
   Vector<unsigned int> _i_got_from_him;
   Vector<unsigned int> _he_got_from_me;

   unsigned int _he_knows_what_i_got;        // highest linkprobenumber, he knows that i've got

   struct timeval _last_seen;
   unsigned int _last_lp;
   unsigned int _lp_interval_in_ms;

   unsigned int _note_lp;
   unsigned int _last_lp_he_was_able_to_hear;

   uint8_t  send_per;

   CandidateInfo(EtherAddress &_new_addr, unsigned int note_lp, int last_send_lp)
   {
      _addr = EtherAddress(_new_addr.data());
      _i_got_from_him.clear();
      _he_got_from_me.clear();
      _he_knows_what_i_got = 0;
      _lp_interval_in_ms = 0;
      _note_lp = note_lp;
      if ( last_send_lp < 0 ) _last_lp_he_was_able_to_hear = 0;
      else _last_lp_he_was_able_to_hear = last_send_lp;
      _last_seen = Timestamp::now().timeval();
   };

   ~CandidateInfo()
   {
   }

   long diff_in_ms(timeval t1, timeval t2)
   {
      long s, us, ms;

      while (t1.tv_usec < t2.tv_usec)
      {
        t1.tv_usec += 1000000;
        t1.tv_sec -= 1;
      }

      s = t1.tv_sec - t2.tv_sec;
      us = t1.tv_usec - t2.tv_usec;
      ms = s * 1000L + us / 1000L;
      return ms;
   }

   unsigned int get_last_possible_recv_lp()
   {
     struct timeval _time_now;
     long _time_diff_ms;

     _time_now = Timestamp::now().timeval();
     diff_in_ms(_time_now,_last_seen);

     if ( _lp_interval_in_ms == 0 )
       return (_last_lp);
     else
       return(_last_lp + ( _time_diff_ms / _lp_interval_in_ms ) );
   }

   void add_recv_linkprobe(unsigned int lp_id, unsigned int last_send_lp)
   {
     //int i;

     int j = _i_got_from_him.size();

     if ( j == 0 ) _i_got_from_him.push_back(lp_id);
     else
     {
                                                         // linkprobe_id is smaller or equal as what i got i the past??
       if ( _i_got_from_him[j-1] >= lp_id )              // yes ! uhh, looks like restart of other node so
       {
//       click_chatter("clear vector");
         _i_got_from_him.clear();             // clear all and start from scratch
         _i_got_from_him.push_back(lp_id);
         _he_got_from_me.clear();
         _he_knows_what_i_got = 0;
       }
       else                                             // no, so
       {
         if ( _i_got_from_him.size() > (int)( 2 * _note_lp) ) _i_got_from_him.erase(_i_got_from_him.begin());

         _i_got_from_him.push_back(lp_id);
       }
     }

     if ( _i_got_from_him.size() > 1 )   //need more than 1 llp to calculate the values
     {
        struct timeval _time_now;
        _time_now = Timestamp::now().timeval();

        _lp_interval_in_ms =  diff_in_ms(_time_now,_last_seen) / ( lp_id - _last_lp );
     }

     _last_seen = Timestamp::now().timeval();
     _last_lp = lp_id;
     _last_lp_he_was_able_to_hear = last_send_lp;

     return;
   }

   void add_mention_linkprobe(unsigned int id)
   {
     int i;

     for( i=0; (i < _he_got_from_me.size()) && (_he_got_from_me[i] != id); i++ );
     if ( i == _he_got_from_me.size() )
     {
        if ( _he_got_from_me.size() > (int)( 2 * _note_lp) ) _he_got_from_me.erase(_he_got_from_me.begin());

        _he_got_from_me.push_back(id);
     }

    return;
   }

 };

 public:
  BRNCandidateSelector();
  ~BRNCandidateSelector();
  //
  //methods
  //

  const char *class_name() const  { return "BRNCandidateSelector"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  /**
   * Result: size of used payload
   * @param ptr 
   * @param dht_payload_size 
   */
  unsigned int writeCsPayloadToLinkProbe(uint8_t *ptr,unsigned int dht_payload_size);

  void readCsPayloadFromLinkProbe(EtherAddress &source, uint8_t *ptr,unsigned int dht_payload_size);

  int _debug;

  int getCandidateSet(Vector<CandidateInfo*> *can, int cssize, int *per);

  String printCandidateInfo();
  String printPerInfo();
  String printRecvPerInfo();

  int calculatedPER(CandidateInfo *cand);
  int calculatedRecvPER(CandidateInfo *cand);
  int getBestCandidates(CandidateInfo **cand, int number_of_cand);

 private:

  int add_recv_linkprobe(EtherAddress &_addr, unsigned int lp_id);
  int add_mention_linkprobe(EtherAddress &node, unsigned int lp_id);
  int calculatedPER(CandidateInfo *cand, int number_of_calc_lp);
  int calculatedRecvPER(CandidateInfo *cand, int number_of_calc_lp);
  int calculatedCombinedPER(Vector<CandidateInfo*> &sel_cand, int number_of_calc_lp);
  int getAndTestPossibleCS(Vector<CandidateInfo*> *can,int cssize, int *per, int ac_pos, Vector<CandidateInfo*> *best_can, int number_of_calc_lp );
  uint8_t has_lp(Vector<unsigned int> &lp_list, int lp_id);

  Vector<CandidateInfo*> _cand;
  unsigned int _linkprobe_id;           //last linkprobe id
  unsigned int _note_lp;
  int _max_lp_per_packet;
  EtherAddress _me;

  BrnLinkTable *_brnlt;

};

CLICK_ENDDECLS
#endif
