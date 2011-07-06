#ifndef BRN2_NBLIST_HH
#define BRN2_NBLIST_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include <click/etheraddress.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"

CLICK_DECLS

class BRN2NBList : public BRNElement
{
  public:
    class NeighborInfo {
      public:

      //  enum { FRIEND,    //dst of a packet was me
      //         FOREIGN };  //only broadcast and foreign destination

        EtherAddress _eth;
        Vector<BRN2Device*> _devs;

        struct timeval _last_seen;

        int type;
 
        NeighborInfo()
        {
        }

        NeighborInfo(EtherAddress ea)
        {
          _eth = ea;
        }

        ~NeighborInfo()
        {
        }

    };


    BRN2NBList();
    ~BRN2NBList();

    const char *class_name() const  { return "BRN2NBList"; }
    const char *port_count() const  { return "0/0"; }
    const char *processing() const  { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return false; }
    int initialize(ErrorHandler *);
    void add_handlers();

    bool isContained(EtherAddress *v);
    NeighborInfo *getEntry(EtherAddress *v);

    int insert(EtherAddress eth, BRN2Device *dev);
    int insert(EtherAddress eth, uint8_t dev_number);

    String printNeighbors();

    const EtherAddress *getDeviceAddressForNeighbor(EtherAddress *v);

    typedef HashMap<EtherAddress, NeighborInfo> NBMap;

  private:

    //
    //member
    //

    NBMap _nb_list;
    BRN2NodeIdentity *_nodeid;

};

CLICK_ENDDECLS
#endif
