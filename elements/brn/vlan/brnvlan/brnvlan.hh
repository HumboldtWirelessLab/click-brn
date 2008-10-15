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

#ifndef CLICK_BRNVLAN_HH
#define CLICK_BRNVLAN_HH
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include <elements/brn/brnelement.hh>
#include <click/error.hh>
#include <click/timer.hh>

CLICK_DECLS

class Timer;

/**
 * This element stores for each SSID a VLAN ID.
 * 
 * It announces new VLANs, if a vlan is added and discharges VLANs, if they are removed.
 * Adding and removing VLANs is done through handlers. To add a VLAN you have to choose a
 * SSID under which the VLAN will be available. The VLAN ID is chosen by this element.
 * 
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * ...
 * -> Print("Ethernet frame with BRN and VLAN protocol")
 * brn_vlans :: BRNVLAN();
 * -> Print("Ethernet frame with BRN and VLAN protocol")
 * ...
 * </pre>
 * 
 * <h3>Handlers</h3>
 * 
 * - get_vlans (read)
 * - get_vlan (write)
 * - add_vlan (write)
 * - remove_vlan (write)
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.3 BRN VLAN
 * [0]output       802.3 BRN VLAN
 * </pre> 
 * @note
 * Incoming frames not matching the above specification are killed with an error message.
 * 
 * @todo
 * - Announcing and discharging VLANs (When to remove a VLAN?)
 * - Done via Broadcast
 * - Improve handlers - test if an SSID was specified
 * - support multiple BSSID to ensure that broadcasts stay within the VLAN
 * - Adjust Assocication and Authentication and ProbeResponder, even Beacons? (BRNAssocResponder, BeaconSource)
 * - Cleanup BRNAssocResponder and BeaconSource (maybe send beacons for each VLAN, but first code it properly)
 * - Click stuff - check for live reconfigure, hot swap ...
 * - Reactivate vlan.h for macros to modify 802.1Q fields
 * - checking if the fields are valid should be also done with macros to avoid code duplication
 * - remove all warnings during compiling
 * - the advertised SSID will be 'VLAN@berlinroofnet.de'
 * - maxium length of a SSID
 * - Testing (How to test a network; regression testing)
 * - Use Mathias BrnLogger / What about click's ErrorHandler
 * - Design decision: Packets for services (like DHCP, ARP) are not 802.1Q
 * encaped and thus when doing checking packets without a 802.1Q header will
 * pass
 * - Virtual station belong to VLAN ID 0 (the basis SSID)
 * - To which VLAN does a gateway belong
 * - In general find the proper places in click configuration
 * - Compare DHT-based approach with Broadcast based (When is which appropriate? Advantages vs. Disadvantages (z.b. Netzwerkpartition und Merging))
 *  
 * <hr>
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-24 00:10:18 +0200 (Tue, 24 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2996 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/brnvlan.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
 
class BRNVLAN : public BRNElement {
  
  public:

  BRNVLAN();
  ~BRNVLAN();
  
  const char *class_name() const		{ return "BRNVLAN"; }
  const char *port_count() const		{ return "1/1"; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void add_handlers();
  static String read_handler(Element *e, void *);  
  static int write_handler(const String &arg, Element *e, void *, ErrorHandler *errh);

  void push(int port, Packet *);
  
  typedef HashMap<EtherAddress, int> VLANMembers;
  typedef VLANMembers::const_iterator MembersConstIter;

  class VLAN
  {
    public:
    uint16_t _vid;
    String   _name; 
    VLANMembers _members;
    bool _protected;

    VLAN(uint16_t vid, String name, bool pro) {
      assert(name);
      
      //assert(vid <= 0xFFF && vid >= 0);
      _vid = vid;
      _name = name;
      _protected = pro;
    }
    
    VLAN() {
      VLAN(0, "RESERVED", false);
    }
    
    void add_member(EtherAddress client);
    void remove_member(EtherAddress client);
    
    VLANMembers *get_members() {
      return &_members; 
    }
    
    bool is_protected() {
      return _protected;
    }
    
  };

  private:
  
  
  typedef Vector<VLAN> VLANs;
  typedef HashMap<String, uint16_t> String2VID;
  typedef HashMap<uint16_t, VLAN> VID2VLAN;
  typedef VID2VLAN::const_iterator VLANConstIter;


  String2VID _vids; /// stores SSID to VID
  VID2VLAN _vlans;// stores VID to VLAN
  
  String _my_ssid; /// stores this nodes SSID; used for VLAN with VID 0

  /* dht stuff */  
  #define TYPE_VLAN_NAME TYPE_STRING
  #define TYPE_VLAN_ID TYPE_INTEGER
  
  /* for dht communication */
  uint8_t _unique_id; // used for each dht packet
  #define DHT_VLAN_NUM 6
  
  #define DHT_VLAN_GET_VLAN 0
  #define DHT_VLAN_GET_VID 1
  #define DHT_VLAN_ADD_VLAN 2
  #define DHT_VLAN_ADD_VID 3
  #define DHT_VLAN_REMOVE_VLAN 4
  #define DHT_VLAN_REMOVE_VID 5
  
  // timer stuff to update known vlans
  void run_timer(Timer *);
  Timer _timer;
  #define DEFAULT_INTERVAL 300
  int _interval; // interval in seconds
  
  uint16_t random_vid();
    
  public:
  
  /**
   * Is the VLAN known?
   * 
   * @param ssid of the  VLAN
   * 
   * @return true, if vlan is known, else false
   */ 
  bool is_vlan(String ssid);
  
  /**
   * Is the VLAN known?
   * 
   * @param vid of the  VLAN
   * 
   * @return true, if vlan is known, else false
   */ 
  bool is_vlan(uint16_t vid);
  
  /**
   * Get the VLAN ID for a given SSID.
   * 
   * @param ssid to get the VLAN ID for
   * 
   * @return the VLAN ID, if SSID is known, else 0xFFFF
   */ 
  uint16_t get_vlan(String ssid);
  
  /**
   * Get the VLAN for a given VID.
   * 
   * @param vid to get the VLAN
   * 
   * @return the VLAN ID, if SSID is known, else 0xFFFF
   */ 
  VLAN *get_vlan(uint16_t vid);
  
  /**
   * Get all members of a VLAN.
   * 
   * @param vid of VLAN
   * 
   * @return list of VLANMembers
   * 
   */
  const VLANMembers *get_clients_from_vid(uint16_t vid);
  
  /**
   * Add a VLAN with given VLAN id and SSID.
   * 
   * @param ssid is the SSID of the VLAN
   * @param vid is the VLAN ID of the VLAN
   * 
   * @return true, if VLAN was added, else false
   * 
   * 
   * @note
   * If this method returns true, it doesn't mean that
   * every node knows this VLAN. Packets may get lost.
   * 
   * @todo
   * - A recovery scheme for such cases is necessary or the
   *   distribution is reliable.
   * - it should not be necessary to name a vid. BRNVLAN should find it itself.
   */
  bool add_vlan(String ssid, uint16_t vid, bool protect);
  
  /**
   * Remove VLAN for SSID.
   * 
   * @param ssid is the SSID of the VLAN to remove
   * 
   * @return true, if VLAN was removed, else false
   * 
   */
  bool remove_vlan(String ssid);

  /**
   * Remove trailing bytes from ssid
   * 
   * @param ssid is the SSID
   * 
   * @return ssid without trailing bytes
   * 
   */
  String remove_end_bytes(String ssid);
  
  private:
  /**
   * 
   * Packet to look for VLAN named ssid in DHT
   * 
   * @param ssid is the SSID of the VLAN to look for
   * 
   * @return a packet for DHT
   */
  void get_vlan_from_dht(String ssid);

  /**
   * 
   * Packet to look for VLAN with vid
   * 
   * @param vid is the vid of the VLAN to look for
   * 
   * @return a packet for DHT
   */
  void get_vlan_from_dht(uint16_t vid);
  
  
  void add_vlan_to_dht(String ssid, uint16_t vid);
  void add_vid_to_dht(String ssid, uint16_t vid);
  void remove_vlan_from_dht(String ssid, uint16_t vid);
  void remove_vid_from_dht(String ssid, uint16_t vid);
  void handle_dht_response(Packet* p);
  
  uint16_t extract_vid(Packet *p, int at_number);
  String extract_vlan(Packet *p, int at_number);

};

CLICK_ENDDECLS
#endif
