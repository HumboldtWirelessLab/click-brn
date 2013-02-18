#ifndef CLICK_FOREIGNRXSTATS_HH
#define CLICK_FOREIGNRXSTATS_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_device.hh"

CLICK_DECLS

/*
 * =c
 * ForeignRxStats([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 *
 * Keyword arguments are:
 * *
 * =a
 * 
 */

#define FOREIGNRXSTATS_DEFAULT_ACK_TIMEOUT 5 /*ms*/

class ForeignRxStats : public BRNElement {

 public:
  class RxStats {
    public:
#define LIST_SIZE 32     
#define RXSTATS_FLAGS_RECEIVED       1
#define RXSTATS_FLAGS_RECEIVED_SHIFT 0
#define RXSTATS_FLAGS_OK             2
#define RXSTATS_FLAGS_OK_SHIFT       1
#define RXSTATS_FLAGS_ACKED    4
#define RXSTATS_FLAGS_RETRY    8
      EtherAddress _ea;
   
      uint16_t _seq[LIST_SIZE];
      uint8_t _flags[LIST_SIZE];

      Timestamp _last_index_inc;

      RxStats(EtherAddress ea) {
        _ea = ea;
		
        memset(_seq, 0, sizeof(_seq));
        memset(_flags, 0, sizeof(_flags));

        _last_index_inc = Timestamp::now();
      }

      ~RxStats() {
      }


      inline void add_seq(Timestamp */*ts*/, uint16_t seq, uint8_t ok, uint8_t retry ) {
	uint16_t index = seq % LIST_SIZE;
	_seq[index] = seq;
	_flags[index] = RXSTATS_FLAGS_RECEIVED | (ok << 1) | (retry << 3);
      }

      inline void add_ack(Timestamp */*ts*/, uint16_t /*seq*/) {
      }

      inline void is_acked(Timestamp */*ts*/, uint16_t /*seq*/) {
      }

  };
  
 public:
  typedef HashMap<EtherAddress, RxStats*> RxStatsTable;
  typedef RxStatsTable::const_iterator RxStatsTableIter;

  ForeignRxStats();
  ~ForeignRxStats();

  const char *class_name() const { return "ForeignRxStats"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  Packet *simple_action(Packet *);
  void add_handlers();

  void send_outstanding_packet(bool failed);

  BRN2Device* _device;
  Packet *last_packet;
  EtherAddress last_packet_src;
  
  Timer fwd_timer;
  uint16_t ack_timeout;
  
  String stats_handler();
  
  RxStatsTable rxs_tab;
};

CLICK_ENDDECLS
#endif
