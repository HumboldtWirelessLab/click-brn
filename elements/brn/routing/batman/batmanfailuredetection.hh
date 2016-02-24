#ifndef BATMANFAILUREDETECTIONELEMENT_HH
#define BATMANFAILUREDETECTIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"


CLICK_DECLS

#define BATMAN_PACKET_ID_BUFFER_SIZE 256

#define BATMAN_DEFAULT_LOOP_TIMEOUT 1000

#define BATMAN_ERROR_NONE      0
#define BATMAN_ERROR_DUPLICATE 1
#define BATMAN_ERROR_LOOP      2

#define BATMAN_FAILURE_DETECTION_INPORT_RX         0
#define BATMAN_FAILURE_DETECTION_INPORT_TXFEEDBACK 1

#define BATMAN_FAILURE_DETECTION_OUTPORT_RX         0
#define BATMAN_FAILURE_DETECTION_OUTPORT_ERROR      1
#define BATMAN_FAILURE_DETECTION_OUTPORT_TX_RETRY   2



class BatmanFailureDetection : public BRNElement {

 public:

   class BatmanSrcInfo {
     uint16_t _packet_ids[BATMAN_PACKET_ID_BUFFER_SIZE];
     Timestamp _packet_times[BATMAN_PACKET_ID_BUFFER_SIZE];
     uint8_t _packet_ttls[BATMAN_PACKET_ID_BUFFER_SIZE];
     uint8_t _packet_retries[BATMAN_PACKET_ID_BUFFER_SIZE];

    public:
     uint32_t _loop_detected;

     BatmanSrcInfo(): _loop_detected(0) {
       memset(_packet_ids,0, sizeof(_packet_ids));
       memset(_packet_ttls,0, sizeof(_packet_ttls));
       memset(_packet_retries,0, sizeof(_packet_retries));
     }

     int packet_error_type(uint16_t packet_id, Timestamp *now, int max_time_diff, uint8_t ttl, bool update = true) {
       int index = packet_id % BATMAN_PACKET_ID_BUFFER_SIZE;
       int error_code = BATMAN_ERROR_NONE;

       int time_diff = (*now - _packet_times[index]).msecval();
       if ( (_packet_ids[index] == packet_id ) && (time_diff < max_time_diff ) ) {
         if (_packet_ttls[index] > ttl) error_code = BATMAN_ERROR_LOOP;
         else if (_packet_ttls[index] == ttl) error_code = BATMAN_ERROR_DUPLICATE;
       }

       if ( update ) {
         if ( (_packet_ids[index] != packet_id) || (time_diff > max_time_diff) ) { //TODO: check whether we should consider the time
           _packet_retries[index] = 0;
         }
         _packet_ids[index] = packet_id;
         _packet_times[index] = *now;
         _packet_ttls[index] = ttl;
       }

       return error_code;
     }

     uint8_t packet_retry(uint16_t packet_id, Timestamp *now, int max_time_diff) {
       int index = packet_id % BATMAN_PACKET_ID_BUFFER_SIZE;

       if ( !((_packet_ids[index] == packet_id ) && ((*now - _packet_times[index]).msecval() < max_time_diff ))) {
         _packet_ids[index] = packet_id;
         _packet_retries[index] = 0;
       }

       _packet_times[index] = *now;

       return _packet_retries[index];
     }

     void inc_retry(uint16_t packet_id) {
       int index = packet_id % BATMAN_PACKET_ID_BUFFER_SIZE;
       _packet_retries[index]++;
     }

     inline void inc_loop_detected() { _loop_detected++; }

   };

   typedef HashMap<EtherAddress, BatmanSrcInfo> BatmanSrcInfoTable;
   typedef BatmanSrcInfoTable::const_iterator BatmanSrcInfoTableIter;
  //
  //methods
  //
  BatmanFailureDetection();
  ~BatmanFailureDetection();

  const char *class_name() const  { return "BatmanFailureDetection"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/3"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  String get_info();
 private:
  //
  //member
  //

  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;

  BatmanSrcInfoTable _bsit;

  bool _active;
  uint32_t _loop_timeout;

  uint32_t _detected_loops, _detected_duplicates, _no_retries, _no_failures;

};

CLICK_ENDDECLS
#endif
