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

class DeviceInfo {
  String device_name;
  EtherAddress device_etheraddress;
  String device_type;

  DeviceInfo() {
  }

  ~DeviceInfo() {
  }

}

class BRN2Device : public Element {


  public:
    //
  //member
    //
    int _debug;

    //
  //methods
    //
    BRN2Device();
    ~BRN2Device();

    const char *class_name() const  { return "BRN2Device"; }
    const char *processing() const  { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return false; }

    const String& getDeviceName();
    void setDeviceName(String dev_name);

    int initialize(ErrorHandler *);
    void add_handlers();

  private:
    //
    //member
    //
    String _dev_name;
    EtherAddress *_dev_ether_address;
    String _dev_type;

};

CLICK_ENDDECLS
#endif
