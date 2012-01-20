#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/confparse.hh>
#include <click/error.hh>

#include "elements/brn2/dht/routing/falcon/falcon_linkprobe_handler.hh"
#include "elements/brn2/dht/routing/falcon/falcon_successor_maintenance.hh"
#include "elements/brn2/dht/routing/falcon/falcon_routingtable_maintenance.hh"

#include "hawk_routingtable.hh"

CLICK_DECLS

HawkRoutingtable::HawkRoutingtable():
  _lprh(NULL),
  _succ_maint(NULL),
  _rt_maint(NULL)
{
}

HawkRoutingtable::~HawkRoutingtable()
{
}

/* called by click at configuration time */
int
HawkRoutingtable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
       "LPRH", cpkP+cpkM, cpElement, &_lprh,
       "SUCCM", cpkP+cpkM, cpElement, &_succ_maint,
       "RTM", cpkP+cpkM, cpElement, &_rt_maint,
       "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
HawkRoutingtable::initialize(ErrorHandler *)
{
  ((FalconLinkProbeHandler*)_lprh)->setHawkRoutingTable(this);
  ((FalconSuccessorMaintenance*)_succ_maint)->setHawkRoutingTable(this);
  ((FalconRoutingTableMaintenance*)_rt_maint)->setHawkRoutingTable(this);
  return 0;
}

HawkRoutingtable::RTEntry *
HawkRoutingtable::addEntry(EtherAddress *ea, uint8_t *id, int id_len, EtherAddress *next_phy)
{
  BRN_INFO("Add direct known link. Next Phy is also next Overlay Hop.");

  BRN_DEBUG("Add Entry.");
  BRN_INFO("NEW ROUTE: DST: %s NEXTPHY: %s", ea->unparse().c_str(),
                                             next_phy->unparse().c_str());
  for(int i = 0; i < _rt.size(); i++) {                         //search
    if ( memcmp( ea->data(), _rt[i]->_dst.data(), 6 ) == 0 ) {  //if found
      memcpy(_rt[i]->_dst_id, id, MAX_NODEID_LENTGH);           //update id
      _rt[i]->_dst_id_length = id_len;
      _rt[i]->_time = Timestamp::now();

      if ( ! _rt[i]->nextHopIsNeighbour() ) {
        _rt[i]->updateNextHop(next_phy);
        _rt[i]->updateNextPhyHop(next_phy);
      }

      return _rt[i];
    }
  }

  BRN_DEBUG("Entry is new: Dst: %s Next: %s", ea->unparse().c_str(),
                                              next_phy->unparse().c_str());
  HawkRoutingtable::RTEntry *n = new RTEntry(ea,id,id_len,next_phy, next_phy);
  _rt.push_back(n);

  return n;
}

HawkRoutingtable::RTEntry *
HawkRoutingtable::addEntry(EtherAddress *ea, uint8_t *id, int id_len,
                           EtherAddress *_next_phy, EtherAddress *next)
{
  EtherAddress *next_phy;

  if ( _next_phy == NULL ) {
   next_phy = getNextHop(next);
   if ( next_phy == NULL ) {
     BRN_INFO("Unable to add overlay link. Next Phy can differ from next.");
     BRN_INFO("DISCARD ROUTE: DST: %s NEXTPHY: %s NEXT: %s", ea->unparse().c_str(),
                                                    _next_phy->unparse().c_str(),
                                                    next->unparse().c_str());
     return NULL;
   }
  } else {
   next_phy = _next_phy;
  }

  /** The packet will be send to dst over next. to
    * reach next we will use next overlay");
    */
  BRN_INFO("Add overlay link. Next Phy can differ from next.");
  BRN_INFO("NEW ROUTE: DST: %s NEXTPHY: %s NEXT: %s", ea->unparse().c_str(),
                                                    next_phy->unparse().c_str(),
                                                    next->unparse().c_str());

  for(int i = 0; i < _rt.size(); i++) {                         //search
    if ( memcmp( ea->data(), _rt[i]->_dst.data(), 6 ) == 0 ) {  //if found
      memcpy(_rt[i]->_dst_id, id, MAX_NODEID_LENTGH);           //update id
      _rt[i]->_dst_id_length = id_len;
      _rt[i]->_time = Timestamp::now();

      /* update only if we don't know whether next_phy_hop knows the route
       * (incl. next hop)
       */
      if ( ! _rt[i]->nextHopIsNeighbour() ) {
        _rt[i]->updateNextHop(next);
        _rt[i]->updateNextPhyHop(next_phy);
      }

      return _rt[i];
    }
  }

  BRN_DEBUG("Entry is new: Dst: %s NextPhy: %s Next: %s", ea->unparse().c_str(),
                            next_phy->unparse().c_str(),next->unparse().c_str());
  HawkRoutingtable::RTEntry *n = new RTEntry(ea, id, id_len, next_phy, next);
  _rt.push_back(n);

  return n;
}


HawkRoutingtable::RTEntry *
HawkRoutingtable::getEntry(EtherAddress *ea)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _rt.size(); i++) {                           //search
    if ( memcmp(_rt[i]->_dst.data(), ea->data(), 6) == 0 ) {       //if found
      if ( (now - _rt[i]->_time).msecval() < HAWKMAXKEXCACHETIME ) { //check age, if not too old
        return _rt[i];                                             //give back
      } else {                                                    //too old ?
        BRN_INFO("Remove old link in Hawk RoutingTable.");
        delete _rt[i];
        _rt.erase(_rt.begin() + i);                               //delete !!
        return NULL;                                              //return NULL
      }
    }
  }

  return NULL;
}

HawkRoutingtable::RTEntry *
HawkRoutingtable::getEntry(uint8_t *id, int id_len)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _rt.size(); i++) {                           //search
    if ( memcmp(_rt[i]->_dst_id, id, id_len) == 0 ) {                 //if found
      if ( (now - _rt[i]->_time).msecval() < HAWKMAXKEXCACHETIME ) { //check age, if not too old
        return _rt[i];                                            //give back
      } else {                                                    //too old ?
        delete _rt[i];
        _rt.erase(_rt.begin() + i);                               //delete !!
        return NULL;                                              //return NULL
      }
    }
  }

  return NULL;
}

void
HawkRoutingtable::delEntry(EtherAddress *ea)
{
  RTEntry *entry;

  for(int i = 0; i < _rt.size(); i++) {
    entry = _rt[i];
    if ( memcmp(entry->_dst.data(), ea->data(), 6) == 0 ) {
      delete entry;
      _rt.erase(_rt.begin() + i);
      break;
    }
  }
}

bool
HawkRoutingtable::isNeighbour(EtherAddress *ea)
{
  if ( ea == NULL ) {
    BRN_WARN("Ask for being neighbour given NULL. I'll answer with no.");
    return false;
  }

  //Vector<EtherAddress> neighbors;
  //_lt->get_neighbors(, neighbors);

  for(int i = 0; i < _rt.size(); i++) {                         //search
    if ( memcmp( ea->data(), _rt[i]->_dst.data(), 6 ) == 0 ) {  //if found
      return (memcmp(_rt[i]->_next_hop.data(), _rt[i]->_dst.data(), 6 ) == 0 );
    }
  }
  return false;
}

EtherAddress *
HawkRoutingtable::getNextHop(EtherAddress *dst)
{
  for(int i = 0; i < _rt.size(); i++) {                         //search
    if ( memcmp( dst->data(), _rt[i]->_dst.data(), 6 ) == 0 ) {  //if found
      return &(_rt[i]->_next_hop);
    }
  }
  return NULL;
}

bool
HawkRoutingtable::hasNextPhyHop(EtherAddress *dst)
{
  RTEntry *entry = getEntry(dst);
  return ( memcmp(entry->_next_hop.data(), entry->_next_phy_hop.data(), 6 ) == 0 );
}

EtherAddress*
HawkRoutingtable::getNextPhyHop(EtherAddress *dst)
{
  RTEntry *entry = getEntry(dst);

  while ( ( entry != NULL ) && (! isNeighbour(&(entry->_next_phy_hop))) ) {
    EtherAddress *next_phy_hop = getNextHop(next_phy_hop);
    entry = getEntry(next_phy_hop);
  }

  if ( entry == NULL ) return NULL;

  return &(entry->_next_phy_hop);
}

/**************************************************************************/
/************************** H A N D L E R *********************************/
/**************************************************************************/

String
HawkRoutingtable::routingtable()
{
  StringAccum sa;

  sa << "<hawkroutingtable node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now().unparse() << "\" entries=\"" << _rt.size() << "\" >\n";

  RTEntry *entry;
  for(int i = 0; i < _rt.size(); i++) {
    entry = _rt[i];

    sa << "\t<entry node=\"" << entry->_dst.unparse() << "\" next_hop=\"" << entry->_next_phy_hop.unparse();
    sa << "\" next_overlay=\"" << entry->_next_hop.unparse() << "\" age=\"0\" />\n";
  }

  sa << "\n</hawkroutingtable>\n";

  return sa.take_string();
}

enum {H_RT};

static String
read_handler(Element *e, void * vparam)
{
  HawkRoutingtable *rq = (HawkRoutingtable *)e;

  switch ((intptr_t)vparam) {
    case H_RT: {
      return rq->routingtable();
    }
  }
  return String("n/a\n");
}

/*static int 
write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  HawkRoutingtable *rq = (HawkRoutingtable *)e;
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
    case H_RT: {
      int debug;
      if (!cp_integer(s, &debug))
        return errh->error("debug parameter must be an integer value between 0 and 4");
      rq->_debug = debug;
      break;
    }
  }
  return 0;
}
*/
void
HawkRoutingtable::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("tableinfo", read_handler, (void*) H_RT);
}

#include <click/vector.cc>
template class Vector<HawkRoutingtable::RTEntry*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(HawkRoutingtable)
