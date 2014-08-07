#ifndef BRN2DEVICEELEMENT_HH
#define BRN2DEVICEELEMENT_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif

#include "elements/brn/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * BRN2Device()
 * =s 
 * information for a single device
 * =d
 */

#define STRING_UNKNOWN  "UNKNOWN"
#define STRING_WIRED    "WIRED"
#define STRING_WIRELESS "WIRELESS"
#define STRING_VIRTUAL  "VIRTUAL"

#define DEVICETYPE_UNKNOWN  0
#define DEVICETYPE_WIRED    1
#define DEVICETYPE_WIRELESS 2
#define DEVICETYPE_VIRTUAL  3

class BRN2Device : public BRNElement {
  public:
    //
    //methods
    //
    BRN2Device();
    ~BRN2Device();

    const char *class_name() const  { return "BRN2Device"; }
    const char *processing() const  { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return false; }

    int initialize(ErrorHandler *);
    void add_handlers();

    const String& getDeviceName();
    void setDeviceName(String dev_name);

    const EtherAddress *getEtherAddress();
    void setEtherAddress(EtherAddress *ea);

    EtherAddress *getEtherAddressFix();

    const IPAddress *getIPAddress();
    void setIPAddress(IPAddress *ip);

#ifdef HAVE_IP6
    const IP6Address *getIP6Address();
    void setIP6Address(IP6Address *ip);
#endif

    uint32_t getDeviceType();
    void setDeviceType( uint32_t type);

    const String& getDeviceTypeString();
    void setDeviceTypeString(String type);

    uint8_t getDeviceNumber();
    void setDeviceNumber(uint8_t);

    bool is_service_device();
    bool is_master_device();

    bool is_routable();
    void set_routable(bool routable);

    inline bool allow_broadcast() { return _allow_broadcast; }
    inline void set_allow_broadcast(bool allow_bcast) { _allow_broadcast = allow_bcast; }

    inline uint8_t getChannel() { return _channel; }
    inline void setChannel(uint8_t c) { _channel = c; }

    int set_power_iwconfig(int power, ErrorHandler *errh);

    inline uint16_t get_power() { return _power;}

    String device_info();

  private:

    //
    //member
    //

    String device_name;

    EtherAddress device_etheraddress;
    EtherAddress device_etheraddress_fix;  //first device address. This is used to reset the mac address
                                           //to default, e.g. after mac-cloning
    IPAddress ipv4;
#ifdef HAVE_IP6
    IP6Address ipv6;
#endif

    String device_type_string;
    uint32_t device_type;
    uint8_t device_number;

    bool is_service_dev;
    bool is_master_dev;

    uint32_t getTypeIntByString(String type);
    String getTypeStringByInt(uint32_t type);

    bool _routable;

    bool _allow_broadcast;

    /**
     * WIRELESS DEVICES
     *
     */

    /** QueueCtrl */
    uint8_t _channel;

    uint8_t _no_queues; //number of queues
    uint32_t *_cwmin;   //Contention Window Minimum; Array (see: monitor)
    uint32_t *_cwmax;   //Contention Window Maximum; Array (see:monitor)
    uint32_t *_aifs;    //Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)
    uint32_t *_queue_info;

    uint16_t _power;

    /** CCA */
    int _rx_threshold;
    int _cs_threshold;
    int _cp_threshold;

  public:

    uint32_t set_backoff();
    uint32_t get_backoff();
    int set_backoff_scheme(uint32_t scheme);

    uint8_t get_no_queues() { return _no_queues; }
    uint32_t *get_cwmin() { return _cwmin; }
    uint32_t *get_cwmax() { return _cwmax; }
    uint32_t *get_aifs() { return _aifs; }

    /** TX CONTROL **/
    int abort_transmission(EtherAddress &dst);

    void set_cca(int cs_threshold, int rx_threshold, int cp_threshold);
    void get_cca();
};

CLICK_ENDDECLS
#endif
