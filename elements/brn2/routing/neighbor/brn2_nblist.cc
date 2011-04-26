#include <click/config.h>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "brn2_nblist.hh"

CLICK_DECLS

BRN2NBList::BRN2NBList()
{
  BRNElement::init();
}

BRN2NBList::~BRN2NBList()
{
}

int
BRN2NBList::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      cpEnd) < 0)
          return -1;

  if (!_nodeid || !_nodeid->cast("BRN2NodeIdentity"))
    return errh->error("BRN2NodeIdentity not specified");

  return 0;
}

int
BRN2NBList::initialize(ErrorHandler *)
{
  return 0;
}


/* checks if a node with the given ethernet address is associated with us */
bool
BRN2NBList::isContained(EtherAddress *v)
{
  return (NULL != _nb_list.findp(*v));
}

BRN2NBList::NeighborInfo *
BRN2NBList::getEntry(EtherAddress *v)
{
  return _nb_list.findp(*v);
}

const EtherAddress *
BRN2NBList::getDeviceAddressForNeighbor(EtherAddress *v) {
  BRN2Device *dev;
  NeighborInfo *nb_info = _nb_list.findp(*v);

  if ( nb_info->_devs.size() > 0 ) {
    dev = nb_info->_devs[0];
    return dev->getEtherAddress();
  }

  return NULL;
}

String
BRN2NBList::printNeighbors()
{
  BRN2Device *dev;
  StringAccum sa;
  for (NBMap::iterator i = _nb_list.begin(); i.live(); i++) {
    NeighborInfo &nb_info = i.value();
    sa << " * nb: " << nb_info._eth.unparse() << " via device: ";
    for ( int d = 0; d < nb_info._devs.size(); d++ ) {
     dev = nb_info._devs[d];
     sa << dev->getDeviceName().c_str();
     if ( (d + 1) != nb_info._devs.size() ) 
       sa << ",";
    }
    sa << " reachable\n";
  }
  return sa.take_string();
}
int
BRN2NBList::insert(EtherAddress eth, uint8_t dev_number)
{
  return insert(eth, _nodeid->getDeviceByNumber(dev_number));
}

int
BRN2NBList::insert(EtherAddress eth, BRN2Device *dev)
{
  BRN2Device *acdev;

  if (!(eth && dev)) {
//    BRN_WARN("* You idiot, you tried to insert %s, %s", eth.unparse().c_str(), dev->device_name.c_str());
    return -1;
  }

  NeighborInfo *nb_info = _nb_list.findp(eth);
  if (!nb_info) {
    _nb_list.insert(eth, NeighborInfo(eth));
//    BRN_DEBUG(" * new entry added2: %s, %s", eth.unparse().c_str(), dev_name.c_str());
    nb_info = _nb_list.findp(eth);
  }

  int d;
  for ( d = 0; d < nb_info->_devs.size(); d++ ) {
    acdev = nb_info->_devs[d];
    if ( acdev->getDeviceName() == dev->getDeviceName() ) break;
  }

  if ( d == nb_info->_devs.size() )
    nb_info->_devs.push_back(dev);

  return 0;
}

////////////////////////////////////////////////////////////////////////

static String
read_neighbor_param(Element *e, void */*thunk*/)
{
  BRN2NBList *nbl = (BRN2NBList *)e;
  return nbl->printNeighbors()+"\n";
}


void
BRN2NBList::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("neighbor", read_neighbor_param, 0);
  //add_write_handler("insert", static_insert, 0);
}

#include <click/vector.cc>
template class Vector<BRN2Device*>;
#include <click/bighashmap.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2NBList)
