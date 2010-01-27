#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "dhtrouting_falcon.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

DHTRoutingFalcon::DHTRoutingFalcon():
  _frt(NULL)
{
}

DHTRoutingFalcon::~DHTRoutingFalcon()
{
}

void *DHTRoutingFalcon::cast(const char *name)
{
  if (strcmp(name, "DHTRoutingFalcon") == 0)
    return (DHTRoutingFalcon *) this;
  else if (strcmp(name, "DHTRouting") == 0)
         return (DHTRouting *) this;
       else
         return NULL;
}

int DHTRoutingFalcon::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      cpEnd) < 0)
    return -1;

  _me = _frt->_me;

  return 0;
}

int DHTRoutingFalcon::initialize(ErrorHandler *)
{
  return 0;
}

static bool
isBigger(DHTnode *a, DHTnode *b)         //a > b ??
{
  return (MD5::hexcompare( a->_md5_digest, b->_md5_digest ) > 0);
}

static bool
isBigger(DHTnode *a, md5_byte_t *md5d)  //a > b ??
{
  return ( MD5::hexcompare( a->_md5_digest, md5d ) > 0);
}

static bool
isSmallerEqual(DHTnode *a, DHTnode *b)         //a <= b ??
{
  return (MD5::hexcompare( b->_md5_digest, a->_md5_digest ) >= 0);
}

static bool
isSmallerEqual(DHTnode *a, md5_byte_t *md5d)  //a <= b ??
{
  return ( MD5::hexcompare( md5d, a->_md5_digest ) >= 0);
}

static bool
isSmaller(DHTnode *a, md5_byte_t *md5d)  //a < b ??
{
  return (MD5::hexcompare( md5d, a->_md5_digest ) > 0);
}

static bool
isSmaller(DHTnode *a,DHTnode *b )  //a < b ??
{
  return (MD5::hexcompare( b->_md5_digest, a->_md5_digest ) > 0);
}


DHTnode *
DHTRoutingFalcon::get_responsibly_node(md5_byte_t *key)
{
  if ( ( _frt->successor == NULL ) || ( _frt->_fingertable.size() == 0 ) ) return _frt->_me;

  if ( _frt->isInBetween(_frt->_me, _frt->predecessor, key) ) return _frt->_me;
  if ( _frt->isInBetween(_frt->successor, _frt->_me, key) ) return _frt->successor;  //TODO: this should be handle by checking the FT

  for ( int i = ( _frt->_fingertable.size() - 1 ); i >= 0; i-- ) {
    if ( ! _frt->isInBetween(_frt->_fingertable.get_dhtnode(i), _frt->_me, key) ) return _frt->_fingertable.get_dhtnode(i);
  }

  //TODO: check backlog and all nodes. Check time

  click_chatter("Upps. no good node ????");

  return _frt->_me;
}

enum {
  H_ROUTING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  DHTRoutingFalcon *dhtf = (DHTRoutingFalcon *)e;

  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO : return ( dhtf->_frt->routing_info( ) );
    default: return String();
  }
}

void DHTRoutingFalcon::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTRoutingFalcon)
