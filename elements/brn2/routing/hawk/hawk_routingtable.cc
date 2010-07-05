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
HawkRoutingtable::addEntry(EtherAddress *ea, uint8_t *id, int id_len, EtherAddress *next)
{
  BRN_DEBUG("Add Entry.");
  for(int i = 0; i < _rt.size(); i++) {                         //search
    if ( memcmp( ea->data(), _rt[i]->_dst.data(), 6 ) == 0 ) {  //if found
      memcpy(_rt[i]->_dst_id, id, MAX_NODEID_LENTGH);           //update id
      _rt[i]->_dst_id_length = id_len;
      _rt[i]->_time = Timestamp::now();

      if ( (memcmp(ea->data(),next->data(), 6) == 0) &&
           (memcmp(_rt[i]->_dst.data(),_rt[i]->_next_hop.data(), 6) != 0) ) {
           BRN_DEBUG("Entry update: Dst: %s Next: %s", ea->unparse().c_str(), next->unparse().c_str());
         //_rt[i]->updateNextHop(next);
      }

      return _rt[i];
    }
  }

  BRN_DEBUG("Entry is new: Dst: %s Next: %s", ea->unparse().c_str(), next->unparse().c_str());
  HawkRoutingtable::RTEntry *n = new RTEntry(ea,id,id_len,next);
  _rt.push_back(n);

  return n;
}

HawkRoutingtable::RTEntry *
HawkRoutingtable::getEntry(EtherAddress *ea)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _rt.size(); i++) {                           //search
    if ( memcmp(_rt[i]->_dst.data(), ea->data(), 6) == 0 ) {       //if found
      if ( (now - _rt[i]->_time).msec1() < HAWKMAXKEXCACHETIME ) { //check age, if not too old
        return _rt[i];                                             //give back
      } else {                                                    //too old ?
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
      if ( (now - _rt[i]->_time).msec1() < HAWKMAXKEXCACHETIME ) { //check age, if not too old
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

/**************************************************************************/
/************************** H A N D L E R *********************************/
/**************************************************************************/

String
HawkRoutingtable::routingtable()
{
  StringAccum sa;

  sa << "RoutingTable (" << _rt.size() << "):\n";
  sa << "  Destination\t\tNext Hop\t\tAge\n";
  RTEntry *entry;

  for(int i = 0; i < _rt.size(); i++) {
    entry = _rt[i];
    sa << "  " << entry->_dst.unparse() << "\t" << entry->_next_hop.unparse() << "\tUnknown\n";
  }

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

static int 
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
