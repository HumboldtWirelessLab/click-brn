#ifndef BRN2DEVICEELEMENT_HH
#define BRN2DEVICEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

CLICK_DECLS

/*
 * =c
 * BRN2Device()
 * =s 
 * information for a single device
 * =d
 */

#define WIRED "WIRED"
#define WIRELESS "WIRELESS"
#define VIRTUAL "VIRTUAL"

#define DEVICETYPE_WIRED    0
#define DEVICETYPE_WIRELESS 1
#define DEVICETYPE_VIRTUAL  2

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
    EtherAddress *getEtherAddress();
    const String& getDeviceType();
    void setDeviceNumber(uint8_t);
    uint8_t getDeviceNumber();

    //
    //member
    //
    int _debug;


  private:
    //
    //member
    //

    String device_name;
    EtherAddress *device_etheraddress;
    String device_type;
    uint8_t device_number;

};

CLICK_ENDDECLS
#endif
