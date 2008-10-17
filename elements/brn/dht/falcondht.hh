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

#ifndef CLICK_FALCONDHT_HH
#define CLICK_FALCONDHT_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>

//#include "dhtcommunication.hh"
#include "md5.h"
CLICK_DECLS

class BRNLinkStat;

#define MAX_HOPS 255
#define MAX_RETRANSMISSIONS 5
#define MAX_DHT_PACKET_SIZE 1200


#define BRN_FDHT_LOOKUP_TIMER_INTERVAL 100 

#define NODE_DETECTION_INTERVAL 10

#define RETRANSMISSION_TIME 1500

#define MAX_FINGERS 16
#define MAX_RANGE 65536


/*
 * FalconDHT stuff
 */

class FalconDHT : public Element {

 public:

  class DHTnode {

   public:
    EtherAddress ether_addr;
    IPAddress start_ip_range;

    uint8_t hops;

    long time_last_seen;

    md5_byte_t md5_digest[16];

    uint8_t _distance;

    DHTnode(EtherAddress addr,uint8_t num_hops, IPAddress *_net, IPAddress *_mask)
    {
      ether_addr = addr;
      hops = num_hops; 
      calculate_md5((const char*)convert_ether2hex(addr.data()).c_str(),
        strlen((const char*)convert_ether2hex(addr.data()).c_str()), md5_digest);
      
      int mask_shift = _mask->mask_to_prefix_len();
      int net32 = _net->addr();
      net32 = ntohl(net32);
//      click_chatter("net32: %u mask_shift: %u",net32,mask_shift);
      unsigned int *md5_int_p = (unsigned int*)&md5_digest[0];
      unsigned int md5_int = ntohl(*md5_int_p);
//      click_chatter("md5: %u",md5_int);
      md5_int = (md5_int >> mask_shift);
//      click_chatter("md5 shift : %u",md5_int);
      md5_int += net32;
      md5_int = htonl(md5_int);
//      click_chatter("md5 + net32: %u",md5_int);
      start_ip_range = IPAddress(md5_int);

      //BRN_DEBUG("DHT: Node: %s IP-Range: %s",ether_addr.unparse().c_str(),start_ip_range.unparse().c_str());
    }

    ~DHTnode()
    {
    }
		

    static String convert_ether2hex(unsigned char *p)
    {
      char buf[24];
      sprintf(buf, "%02x%02x%02x%02x%02x%02x",
              p[0], p[1], p[2], p[3], p[4], p[5]);
      return String(buf, 17);
    }

    static int hexcompare(const md5_byte_t *sa, const md5_byte_t *sb) {
      int len = sizeof(sa) > sizeof(sb) ? sizeof(sb): sizeof(sa);

      /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
        conversion errors but before numbers; sort them by internal
        bit-pattern, for lack of a more portable alternative.  */
      for (int i = 0; i < len; i++) {
        if (sa[i] > sb[i])
          return +1;
        if (sa[i] < sb[i])
          return -1;
      }
      return 0;
    }

    static void calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest)
    {
      md5_state_t state;

      MD5::md5_init(&state);
      MD5::md5_append(&state, (const md5_byte_t *)msg, msg_len); //strlen()
      MD5::md5_finish(&state, digest);
	
    }

    static void printDigest(md5_byte_t *md5, char *hex_output) {
      for (int di = 0; di < 16; ++di)
          sprintf(hex_output + di * 2, "%02x", md5[di]);
    }

    void printDigest(char *hex_output) {
      return printDigest(md5_digest, hex_output);
    }

  };

  class DHTentry {
   public:
    uint8_t key_type;
    uint8_t key_len;
    uint8_t *key_data;

    uint16_t value_len;
    uint8_t *value_data;

    md5_byte_t md5_digest[16];

    bool _is_relocated;
    uint8_t _relocated_id;

    bool _is_locked;
    bool _lock_hwa[6];
    int _lock_time;

    DHTentry()
    {
      key_len = 0;
      value_len = 0;
      key_data = NULL;
      value_data = NULL;
      _relocated_id = 0;
      _is_relocated = false;
      _is_locked = false;
    }
    
    ~DHTentry()
    {
    }

    static void calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest)
    {
      md5_state_t state;
      //char hex_output[16*2 + 1];
      //int di;

      MD5::md5_init(&state);
      MD5::md5_append(&state, (const md5_byte_t *)msg, msg_len); //strlen()
      MD5::md5_finish(&state, digest);
    }
  };

  class ForwardInfo {
   public:
    uint8_t  _sender;
    uint8_t  _id;

    struct timeval _send_time;

    Packet *_fwd_packet;
    EtherAddress *_ether_add;

    uint8_t _retry;

    ForwardInfo( uint8_t sender, uint8_t id, EtherAddress *rcv_node, Packet *fwd_packet )
    {
      _sender = sender;
      _id = id;
      _send_time = Timestamp::now().timeval();

      _retry = 0;

      _ether_add = new EtherAddress( rcv_node->data() );

      _fwd_packet = fwd_packet;
    }

    ~ForwardInfo()
    {
    }

  };

  class BufferedPacket
  {
    public:
     Packet *_p;
     struct timeval _send_time;

     BufferedPacket(Packet *p, int time_diff)
     {
       assert(p);
       _p=p;
       _send_time = Timestamp::now().timeval();
       _send_time.tv_sec += ( time_diff / 1000 );
       _send_time.tv_usec += ( ( time_diff % 1000 ) * 1000 );
       while( _send_time.tv_usec >= 1000000 )  //handle timeoverflow
       {
         _send_time.tv_usec -= 1000000;
         _send_time.tv_sec++;
       }
     }
     
     void check() const { assert(_p); }
  };

  typedef Vector<DHTnode> DHTnodeList;
  typedef Vector<BufferedPacket> SendBuffer;

  //
  //methods
  //
  FalconDHT();
  ~FalconDHT();

  const char *class_name() const  { return "FalconDHT"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "3/3"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);

  void push( int port, Packet *packet );

  void add_handlers();

// R O U T I N G 
  String routing_info(void);
  String table_info(void);

  static void static_queue_timer_hook(Timer *, void *);
  static void static_lookup_timer_hook(Timer *, void *);

  unsigned int getNextDhtEntries(uint8_t **value);
  void processDhtPayloadFromLinkProbe(uint8_t *ptr,unsigned int dht_payload_size);
  int _debug;

 public:

//Config
  EtherAddress _me;
  BRNLinkStat *_linkstat;

  IPAddress _net_address;
  IPAddress _subnet_mask;
private:
  md5_byte_t _me_md5_digest[16];

//Debug Info
  int push_backs;
  int msg_forward;

//Table
  Vector<DHTentry> dht_list;

//local operation
  int dht_operation(Packet *packet);

  int dht_read(DHTentry *_dht_entry);
  int dht_write(DHTentry *_dht_entry);
  int dht_overwrite(DHTentry *_dht_entry);
  int dht_remove(DHTentry *_dht_entry);
  int dht_lock(DHTentry *_dht_entry, uint8_t *hwa);
  int dht_unlock(DHTentry *_dht_entry, uint8_t *hwa);

//distribution
  Vector<ForwardInfo> forward_list;

  uint8_t dht_packet_id; 

  EtherAddress *find_corresponding_node(DHTentry *_dht_entry);
  int setup_fingertable();
  unsigned int get_succ(unsigned int id);
  bool i_am_the_real_one(DHTentry *_dht_entry);
  bool my_finger_is_better(EtherAddress *cur_src , EtherAddress *cur_dest , 
    EtherAddress *cor_node ,DHTentry *_dht_entry);



  void forward_packet_to_node(Packet *fwd_packet, EtherAddress *rcv_node);
  void send_packet_to_node(Packet *out_packet,uint8_t *rcv_hwa );
  void send_packet_to_node_direct(Packet *send_packet,uint8_t *rcv_hwa );
  void sendNodeInfoToAll(uint8_t type);
  void insert_new_info(Packet *dht_packet);

  void falcon_dht_balance(Vector<DHTnode> *new_members);

  void packet_retransmission();

  void lookup_timer_hook();

// R O U T I N G 
  Timer _lookup_timer;
    
  Vector<EtherAddress> my_neighbors;
  DHTnodeList dht_neighbors;
  DHTnodeList dht_members;  //soll irgendwann raus
  DHTnodeList finger_table;
  DHTnodeList extra_finger_table;

  uint8_t *linkprob_info;
  uint8_t linkprob_count;
  bool linkprop_table_change;

  unsigned int successor;
  unsigned int predecessor;
  unsigned int myself;


  void init_routing(EtherAddress *me);
  void nodeDetection(void);
  Packet *createInfoPacket( uint8_t type );
  void sendNodeInfo( Vector<EtherAddress> *neighbors, uint8_t type );
  void sendNodeInfo(Vector<DHTnode> *neighbors, uint8_t type );
  void sendNodeInfoTo(EtherAddress *eth_addr, uint8_t type );

  void requestForLostNodes(Vector<EtherAddress> *lost_neighbors);

  void dht_intern_packet(Packet *packet);

  uint8_t node_detection_time;

//sendbuffer with jitter
public:
  int _min_jitter,_jitter,_min_dist,_startup_time;

  long diff_in_ms(timeval t1, timeval t2);

  Timer _sendbuffer_timer;

  SendBuffer packet_queue;

  uint32_t max_fingers;
  uint32_t max_range;
  
  bool _fake_arp_for_simulator; ///< Fake arp replies for ARP DHT tests

  unsigned int get_min_jitter_in_queue();
  void queue_timer_hook();
};

CLICK_ENDDECLS
#endif
