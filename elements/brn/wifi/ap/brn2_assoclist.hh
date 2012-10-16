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

#ifndef BRN2ASSOCLISTELEMENT_HH
#define BRN2ASSOCLISTELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/deque.hh>
#include <elements/brn/routing/linkstat/brn2_brnlinktable.hh>

CLICK_DECLS

#define MAX_SEQ_NO 255
#define SEQ_NO_INF 255

/*
 * =c
 * BRN2AssocList()
 * =s a list of ethernet addresses
 * stores the ethernet address of associated nodes (clients, brn nodes) ...
 * =d
 */
class BRN2AssocList : public Element
{
//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------
public:
  typedef Deque<Packet*>                                           PacketList;
  typedef HashMap<EtherAddress,EtherAddress>                              CnMap;

  typedef enum tag_client_state {
    NON_EXIST,    ///< Dummy for non existant (initial state)
    SEEN_OTHER,   ///< Known and not associated with brn 
    SEEN_BRN,     ///< Known and associated with another BRN mesh node
    ASSOCIATED,   ///< Locally associated
    DISASSOCIATED,///< Locally disassociated
    ROAMED        ///< Previously associated, but roamed
  } client_state;

  enum {
    UPDATE_CHANCES = 6 ///< Number of updates allowed until buffered data gets discarded
  };

  /* State transitions:
   * 
   * NON-EXISTANT -> SEEN_OTHER   (received data with foreign BSSID)
   * (ALL)        -> ASSOCIATED   (succ. association response sent)
   * SEEN_OTHER   -> SEEN_OTHER   (received data with foreign BSSID)
   * SEEN_OTHER   -> SEEN_BRN     (iapp hello received)
   * SEEN_BRN     -> SEEN_BRN     (received data with same foreign BSSID)
   * ASSOCIATED   -> ASSOCIATED   (received data from assoc'd sta)
   * ASSOCIATED   -> ROAMED       (recv iapp notify or 'learned' it)
   * (ALL)        -> NON-EXISTANT (entry timed out)
   *
   * all others are invalid transitions
   */
  class ClientInfo { 
    friend class BRN2AssocList;
  protected:
    client_state   _state; ///< State of the client station 

    // valid in all states
    EtherAddress   _eth; ///< Address of the client
    EtherAddress   _ap; ///< Address of access point the sta is assoc'd.
    BRN2Device     *_dev; ///< Device the client is connected to

    // valid in state ASSOCIATED
    PacketList     _failed_q; ///< FIFO-q for undeliverable packets (after retry)

    // valid in state ASSOCIATED and ROAMED
    CnMap          _cn_map;  ///< Map of address to which a route update was sent
    uint8_t        _seq_no;  ///< Sequence number for roaming
    EtherAddress   _old_ap;   ///< Address of the previous ap, if known.


    String         _ssid; ///< station's associated SSID

    // internals
  protected:
    uint32_t       _age;
    timeval        _last_updated;
    uint32_t       _clear_counter;

  public:
    ClientInfo() : _state(SEEN_OTHER), _age(0), _clear_counter(0) { 
    }

    ClientInfo(client_state state, EtherAddress eth,  EtherAddress ap, 
      BRN2Device *dev, const String& ssid) :
        _state(state), _eth(eth), _ap(ap), _dev(dev), _ssid(ssid), _age(0) {
      _last_updated = Timestamp::now().timeval();
    }

    inline client_state get_state() const {
      return (_state); 
    }

    inline EtherAddress get_eth() const { 
      return (_eth); 
    }

    inline EtherAddress get_ap() const { 
      return (_ap); 
    }

    inline EtherAddress get_old_ap() const {
      return (_old_ap);
    }

    inline BRN2Device *get_dev() const {
      return (_dev); 
    }

    inline String get_dev_name() const {
      return (_dev->getDeviceName());
    }

    inline String get_ssid() const {
      return (_ssid); 
    }

    inline uint32_t get_age() {
      return (_age);
    }

    inline const timeval& get_last_updated() {
      return (_last_updated);
    }

    inline bool contains_cn(const EtherAddress& cn) {
      assert (ASSOCIATED == _state || ROAMED == _state);
      return (NULL != _cn_map.findp(cn));
    }

    inline uint8_t get_seq_no() {
      assert (ASSOCIATED == _state || ROAMED == _state);
      return (_seq_no);
    }

    inline PacketList& get_failed_q() {
      return (_failed_q);
    }

    uint32_t age() {
      struct timeval now;
      now = Timestamp::now().timeval();
      return _age + (now.tv_sec - _last_updated.tv_sec);
    }

    inline void add_cn(const EtherAddress& cn) {
      assert (ASSOCIATED == _state || ROAMED == _state);
      _cn_map.insert(cn,cn);
    }

  protected:
    inline void set_state(client_state state) {
      if (state != _state) {
        _cn_map.clear();
        _old_ap = EtherAddress();
        clear_failed_q();
      }
      _state = state; 
    }

    inline void set_dev_name(BRN2Device *d) { 
      _dev = d; 
    }

    inline void set_ssid(const String& s) { 
      _ssid = s;
    }

    inline void set_ap(const EtherAddress& ap) {
      _ap     = ap;
    }

    inline void set_old_ap(const EtherAddress& ap_old) {
      _old_ap = ap_old;
    }

    inline void set_seq_no(uint8_t seq_no) {
      assert (ASSOCIATED == _state || ROAMED == _state);
      _seq_no = seq_no;
    }

    PacketList release_failed_q() {
      assert (ASSOCIATED == _state || ROAMED == _state); 
      PacketList ret = _failed_q;
      _failed_q.clear();
      _clear_counter = 0;
      return (ret);
    }

    void clear_failed_q() {
      _clear_counter = 0;
      while (0 < _failed_q.size()) {
        _failed_q.front()->kill();
        _failed_q.pop_front();
      }
    }

    void update(uint32_t age) {
      _age = age;
      _last_updated = Timestamp::now().timeval();
      if (_clear_counter++ >= UPDATE_CHANCES) {
        _failed_q.clear();
         _clear_counter = 0;
      }
    }

  };

  typedef HashMap<EtherAddress, ClientInfo> ClientMap;
  typedef ClientMap::iterator               iterator;

//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
  BRN2AssocList();
  virtual ~BRN2AssocList();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const	{ return "BRN2AssocList"; }
  const char *port_count() const	{ return "0/0"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------
public:

  client_state get_state(
    const EtherAddress&   v) const;

  String get_device_name(
    const EtherAddress&   v) const;

  String get_ssid(
    const EtherAddress&   v) const;

  bool is_associated(
    const EtherAddress&   v) const;

  bool is_roaming(
    const EtherAddress&   v) const;

  EtherAddress get_ap(
    const EtherAddress&   v) const;

  iterator begin() { return _client_list->begin(); }

  ClientInfo *get_entry(
    const EtherAddress& v);

  String print_content() const;

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:

  bool insert(
    EtherAddress    eth, 
    BRN2Device      *dev,
    String          ssid,
    EtherAddress    ap_old = EtherAddress(),
    uint8_t         seq_no = SEQ_NO_INF);

  bool remove(
    EtherAddress    eth);

  void roamed( 
    EtherAddress    client,
    EtherAddress    ap_new,
    EtherAddress    ap_old,
    uint8_t         seq_nr );

  void disassociated(
    EtherAddress    client);

  void set_state(
    const EtherAddress&   v,
    client_state          new_state );

  void set_ap(
    const EtherAddress& sta,
    const EtherAddress& ap);

  /**
   * Update the association record
   *
   * @return true if all went well, false in the case of errors.
   */
  bool update(EtherAddress sta);

  void buffer_packet(
    EtherAddress    ether_dst,
    Packet*         p);

  PacketList get_buffered_packets(
    EtherAddress    ether_dst);

  /**
   * Reports the sighting of a foreign station
   *
   * @param ether_sta @a [in] Address of the station.
   * @param ether_bssid @a [in] Id of the basic service set
   * @param dev_name @a [in] The device on which the client was seen.
   * @return true if the client ap association is already known, false otherwise.
   */
  bool client_seen(
    EtherAddress    ether_sta, 
    EtherAddress    ether_bssid,
    BRN2Device      *dev);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int             _debug;
  int             _client_qsize_max;

  ClientMap*      _client_list;

  Brn2LinkTable*   _link_table;
};

CLICK_ENDDECLS
#endif
