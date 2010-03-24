#ifndef BRN2DEVICEELEMENT_HH
#define BRN2DEVICEELEMENT_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif

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

class BRN2Device : public Element {
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

    const IPAddress *getIPAddress();
    void setIPAddress(IPAddress *ip);

#ifdef HAVE_IP6
    const IP6Address *getIP6Address();
    void setIP6Address(IP6Address *ip);
#endif

    const uint32_t getDeviceType();
    void setDeviceType( uint32_t type);

    const String& getDeviceTypeString();
    void setDeviceTypeString(String type);

    const uint8_t getDeviceNumber();
    void setDeviceNumber(uint8_t);

    bool is_service_device();
    bool is_master_device();

    bool is_routable();
    void set_routable(bool routable);

    //
    //member
    //
    int _debug;


  private:

    //
    //member
    //

    String device_name;

    EtherAddress device_etheraddress;

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

};

CLICK_ENDDECLS
#endif
