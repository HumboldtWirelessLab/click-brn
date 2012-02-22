#ifndef CLICK_SIMDEVICE_HH
#define CLICK_SIMDEVICE_HH
#include <click/element.hh>
#include <click/simclick.h>
CLICK_DECLS

/**
 * SimDevice - Base class for simulated devices
 */
class SimDevice : public Element {

  public:
    SimDevice();
    virtual ~SimDevice();

    virtual int incoming_packet(int ifid,int ptype,const unsigned char* data,int len,
                                simclick_simpacketinfo* pinfo) = 0;

    inline String ifname() const;
    inline int fd() const;

  protected:
    String _ifname;
    int _fd;

};

String
SimDevice::ifname() const
{
  return _ifname;
}

int
SimDevice::fd() const
{
  return _fd;
}

CLICK_ENDDECLS
#endif

