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

#ifndef BRNGATEWAY_HH
#define BRNGATEWAY_HH

#include <click/bighashmap.hh>
#include <click/router.hh>

#include "elements/brn/brnelement.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"

#include "gateway.h"

CLICK_DECLS

//GRM -> GATEWAY_REQUEST_MODE

#define GRM_READ                1
#define GRM_READ_BEFORE_UPDATE  2 /*UPDATE INCLUDES ADD*/
#define GRM_UPDATE              3 /*UPDATE INCLUDES ADD*/
#define GRM_READ_BEFORE_REMOVE  4
#define GRM_REMOVE              5

class String;
class Timer;
class EtherAddress;
class BRNSetGatewayOnFlow;


/**
 * 
 * This class describes a gateway entry in the DHT.
 * Its purpose is to encapsulate all information that is relevant to know about a gateway.
 * 
 * @todo
 * - support using other foreign access points which have internet connectivity
 * - build a backbone from gateway nodes
 * 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @version 0.5
 * @since 0.5
 *
 */

/* ===================================================================================== */

class BRNGatewayEntry {
public:
    /**
     * Default Constructor.
     * Creates a BRNGatewayEntry with IP address 0.0.0.0 and a metric of 0, which is NATed.
     * 
     */

    /*
    BRNGatewayEntry() {
        // TODO
        // unsupported by gcc3.3.3 (see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=26078)
        //BRNGatewayEntry(IPAddress(), 0, true);
    }
    */

    /**
     * Constructor to set the values of your own.
     * 
     * @param ip_addr the IP address of the gateway
     * @param metric a value indicating the gateway's connection ranging from 0 (worst) to 255 (best)
     * @param nated true, if gateway is behind a NAT, else false
     * 
     */
    //TODO see above
    //BRNGatewayEntry(IPAddress ip_addr, uint8_t metric, bool nated) {
    BRNGatewayEntry(IPAddress ip_addr = IPAddress(), uint8_t metric = 0, bool nated = true) {
        this->_metric = metric;
        this->_ip_addr = ip_addr;
        this->_nated = nated;
    }

    /**
     * Deconstructor
     * 
     */
    ~BRNGatewayEntry() { };

    /**
     * Returns the IP address.
     * 
     * @return IP address of this BRNGatewayEntry
     * 
     */
    IPAddress get_ip_address() const {
        return IPAddress(_ip_addr);
    }

    /**
     * Returns if the behind a NAT.
     * 
     * @return true, if behind a NAT, else false
     * 
     */
    bool is_behind_NAT() const {
        return _nated;
    }

    /**
     * Returns the metric of this BRNGateWayEntry.
     * The metric ranges from 0..255, where 0 indicates not a gateway and 255 best
     * connectivity.
     * 
     * @return metric of this BRNGatewayEntry
     */
    uint8_t get_metric() const {
        return _metric;
    }

    /**
     * Set metric of this BRNGateWayEntry.
     * 
     */
    void set_metric(uint8_t metric) {
       	_metric = metric;
    }

    /**
     * Return this BRNGatewayEntry's String representation.
     * The format is: '\<metric\>, \<IP address\>, \<NATed?\>'
     * 
     * @return a String representing this BRNGatewayEntry
     * 
     */
    String s() const {
        return String((uint32_t) get_metric()) + ", "
               + get_ip_address().unparse() + ", "
               + String(is_behind_NAT());
    }


//private:
    uint32_t _ip_addr; /** stores the ip address of the gateway */
    uint8_t _metric; /** stores the metric of the gateway */
    bool _nated; /** stores if the gateway is behind a NAT */
};


// TODO
/* mapping between C++ class BRNGatewayEntry and gateway_entry */
/*
struct gateway_entry
{
  uint8_t metric;
  bool _nated;
};
*/

/* ===================================================================================== */


// used to store information about gatways
typedef HashMap<EtherAddress, BRNGatewayEntry> BRNGatewayList;

/**
 * This class is a click element used to store information about gateways.
 * 
 * It acts like data stucture. Other click elements use it to get information
 * about known gateways. Its handlers 'add_gateway' and 'remove_gateway'
 * are used tell whether this node itself is a gateway.   
 * 
 * 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @version 0.5
 * @since 0.5
 * 
 * 
 * @todo
 * 
 * - I should complete wrap the list of gateways. That means I don't want to pass BRNGatewayList to 
 * 	 another element. They should call methods on me
 * - add TODOs here
 */

/* ===================================================================================== */

class BRNGateway : public BRNElement {
  public:

    class RequestInfo {
     public:
      uint32_t _id;
      uint32_t _mode;

      BRNGatewayEntry _gw_entry;

      Timestamp _request_time;

      explicit RequestInfo(uint32_t mode)
      {
        _id = 0;
        _mode = mode;
      }

      ~RequestInfo() {}

    };

    Vector<RequestInfo *> request_list;


    /**
     * Default Constructor.
     * 
     */
    BRNGateway();

    /**
     * Destructor.
     * 
     */
    ~BRNGateway();

    /**
     * Returns the class name of this click element.
     * 
     * @return 'BRNGateway'
     */
    const char *class_name() const { return "BRNGateway"; }

    /**
     * Called for configuring the BRNGateway element.
     * 
     * @param conf is the configuration string passed to BRNGateway
     * @param *errh is used to send errors
     * 
     * @return 0 in case of a successful configuration, else a value unequal 0
     */
    int configure(Vector<String> &conf, ErrorHandler *errh);

    /**
     * Returns true to enable live reconfiguration.
     * live_reconfigure() must be implemented for proper working.
     * 
     * @return true to enable live reconfiguration
     */
    bool can_live_reconfigure() const {
        return true;
    }

    /**
     * Called, if the element is reconfigured while the its router is running.
     * 
     * @param conf the reconfiguration string
     * @param *errh is used to report errors etc.
     * 
     */
    int live_reconfigure (Vector< String > & conf, ErrorHandler *errh) {
        // TODO
        // can this element even be reconfigured
        // What to do?
        return configure(conf, errh);
    }

    /**
    * Called for initialize the BRNGateway element.
    * 
    * Timer are initialized and 0 is returned.
    * 
    * @param *errh is used to send errors
    * 
    * @return 0 
    */
    int initialize(ErrorHandler *errh);

    /**
     * Find the appropriate element to hotswap.
     * 
     * Returns the element with the same name and the same class.
     * TODO What if there are multiple BRNGateways in the old config. What other problems
     * may result from really different configuration?
     * 
     * @return a BRNGateway
     * 
     */
    Element *hotswap_element() const {
        if (Router *r = router()->hotswap_router()) {

        // TODO maybe further checking needs to performed to find
        // the right BRNGateway

        if (Element *e = r->find(this->name())) {
                if (e->cast(this->name().c_str()) != NULL)
                    return e;
            }
        }
        return NULL;
    }

    /**
     * Copy the list of gateways to the new configuration.
     *
     * TODO are there other things to copy. Add some checking.
     *
     * @param *old_element is determined by hotswap_element() and != NULL
     * @param *errh used to report errors
     *
     */
    void take_state(Element *old_element, ErrorHandler *errh) {
        (void) errh;

        BRNGateway* old_gw = reinterpret_cast<BRNGateway *>( old_element);

        // TODO
        // check if configuration is properly
        // e.g. the ethernet address hasn't changed

        // take list of gateway with
        this->_known_gateways = old_gw->_known_gateways;

        // TODO
        // what about the timers
    }

	/**
     * Called to cleanup the BRNGateway, if the router is removed.
     *
     * BRNGateway tries to update the DHT, which may be too late anyway. This has to
     * be improved. TODO
     * 
     * @param cs describes, during what phase the router was, when aborted
     */
    void cleanup(CleanupStage cs);

    /**
     * Returns the number of in- an output port for a BRNGatway. 
     *
     * @see element.hh for a further description
     * @return '1/1', which says 1 input and 1 output port
     */
    const char *port_count() const {
        return "0/0";
    }

    /**
     * Returns the kind of ports of a BRNGateway.
     *
     * @see element.hh for further description
     * @return PUSH, which says the ports are push
     */
    const char *processing() const {
        return PUSH;
    }

    /**
     * Return "x/y" to indicate that no packet ever travels from an
     * input port to an output port.
     *
     * @return "x/y"
     */
    const char *flow_code () const { return "x/y"; }

    /**
     * Called when handlers are to add
     *
     *
     */
    void add_handlers();

    void run_timer(Timer *);

	/**
	 * Adds a BRNGatewayEntry for this node.
	 * 
	 * This method should be called, when this node is a gateway. The values for a BRNGatewayEntry
	 * should be set properly. This method is e.g. called by BRNICMPPingGatewayTester.
	 * An alternative way instead to calling this method, is to use the 
	 * handler 'add_gateway'.
	 * If this node is considered a gateway (metric > 0), the entry for this gateway is added, else
	 * it isn't.
	 * An old entry is <b>overwritten</b>.
	 * 
	 * 
	 * @param entry for this node
	 * 
	 * @return 0, if the entry was successfull added, else a value < 0
	 */
    int add_gateway(BRNGatewayEntry entry);
    
    /**
     * Removes the BRNGatewayEntry for this node.
     * 
     * @return 0 (because is always successful)
     * 
     */
    int remove_gateway();

    /**
     * Removes the BRNGatewayEntry for the specified ether address.
     * 
     * @return 
     * 
     */
    int remove_gateway(EtherAddress eth);


    /**
     * Returns the BRNGatewayEntry for this node.
     * 
     * If there is one the BRNGatewayEntry is returned, else NULL.
     *
     * @return BRNGatewayEntry for this node, if existent, else NULL
     */ 
    const BRNGatewayEntry* get_gateway();


    /* method to get the up to date list of known gateways */
    /* returns a pointer, which is no problem, since click is single-threaded */

    /**
     * Returns a pointer to the BRNGatewayList, which can be used to iterate
     * over the list of BRNGatewayEntries
     * 
     * @return a pointer to the BRNGatewayList
     */
    const BRNGatewayList* get_gateways();

    DHTStorage *_dht_storage;
    static void dht_callback_func(void *e, DHTOperation *op);
    void dht_request(RequestInfo *request_info, DHTOperation *op);
    void handle_dht_reply(RequestInfo *request_info, DHTOperation *op);
    BRNGateway::RequestInfo *get_request_by_dht_id(uint32_t id);
    struct brn_gateway_dht_entry *get_gwe_from_value(uint8_t *value, int valuelen, BRNGatewayEntry *gwe);
    int remove_request(RequestInfo *request_info);

private:
    friend class BRNSetGateway;
    friend class BRNSetGatewayOnFlow;
    friend class BRNGatewaySupervisor;

    EtherAddress _my_eth_addr; /// my ethernet address
    BRNGatewayList _known_gateways; /// list of available (known) gateways

    BRNSetGatewayOnFlow *_flows;


    // timers
    Timer _timer_refresh_known_gateways; /// this timer is used to refesh the list of known gateways (via sending a dht request)
    void timer_refresh_known_gateways_hook(Timer *); /// method called by the timer
    int _update_gateways_interval; /// stores the interval to refresh the list of gateways
    #define DEFAULT_UPDATE_GATEWAYS_INTERVAL 60 /// default interval in seconds 

    Timer _timer_update_dht; /// this timer is used to send update requests for this  mesh node (via sending a dht request)
    void timer_update_dht_hook(Timer *); /// method called by the timer
    int _update_dht_interval; /// interval to update this node in the DHT
    // can be high, because if another thinks this node is a gateway, but
    // it isn't, a feedback will be run
    #define DEFAULT_UPDATE_DHT_INTERVAL	60 // default interval in seconds

    /* for the handlers */
    static String read_handler(Element* , void*);
    static int write_handler(const String &, Element*, void*, ErrorHandler*);


    /* for dht communication */

    /**
     *
     *
     */
    void update_gateway_in_dht(RequestInfo *request_info, DHTOperation *req, BRNGatewayEntry gwe);

    /**
     *
     *
     */
    void remove_gateway_from_dht(RequestInfo *request_info, DHTOperation *req);
    bool pending_remove;
    /**
     * 
     * 
     */
    void get_getways_from_dht(RequestInfo *request_info, DHTOperation *req);

    // extracts the list of gateways from the dht response */
    bool update_gateways_from_dht_response(uint8_t *value, uint32_t valuelen);

};
/* ===================================================================================== */

/**
 * 
 * Used for communication with DHT
 * 
 */
/* ===================================================================================== */


//#define TYPE_STRING 5
#define TYPE_GATEWAY 7
#define DHT_KEY_GATEWAY "gateway"

// this struct is used for dht packets
// it describes an single gateway entry under the key DHT_KEY_GATEWAY with value TYPE_SUB_LIST
/*
struct dht_gateway_entry {
    // key section
    uint8_t       key_number;
    uint8_t       key_type; // TYPE_MAC
    uint8_t       key_len;
    uint8_t*       key_data;

    // value section
    uint8_t       value_number;
    uint8_t       value_type; // TYPE_GATEWAY
    uint8_t       value_len;
    uint8_t*       value_data;
};
*/

/* ===================================================================================== */

CLICK_ENDDECLS
#endif
