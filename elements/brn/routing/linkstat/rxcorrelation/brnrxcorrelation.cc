#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brnrxcorrelation.hh"

CLICK_DECLS

BrnRXCorrelation::BrnRXCorrelation()
  :_debug(BrnLogger::DEFAULT),
   _linkprobe_id(0)
{
}

BrnRXCorrelation::~BrnRXCorrelation()
{
}

int
BrnRXCorrelation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
 //     "DEVICE", cpkP+cpkM, cpEtherAddress, &_nodedevice,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, &_me,
      "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
tx_handler(void *element, const EtherAddress */*ea*/, char *buffer, int size)
{
  BrnRXCorrelation *lph = (BrnRXCorrelation*)element;
  return lph->lpSendHandler((unsigned char*)buffer, size);
}

static int
rx_handler(void *element, EtherAddress */*ea*/, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  BrnRXCorrelation *lph = (BrnRXCorrelation*)element;
  return lph->lpReceiveHandler((unsigned char*)buffer, size);
}

int
BrnRXCorrelation::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_RXC,&tx_handler, &rx_handler);

  return 0;
}

BrnRXCorrelation::NeighbourInfo *
BrnRXCorrelation::getNeighbourInfo(EtherAddress *ea) {
  int i;

  for ( i = 0; i < _cand.size(); i++ )
    if ( _cand[i]->_addr == *ea )
      return _cand[i];

  return NULL;
}

int
BrnRXCorrelation::add_recv_linkprobe(EtherAddress *_addr, unsigned int lp_id)
{
  NeighbourInfo *ni;

  ni = getNeighbourInfo(_addr);

  if ( ni == NULL )                                     // node not in list, so
  {
    ni = new NeighbourInfo(*_addr, lp_id);              // create new one
    _cand.push_back(ni);                                // add to list
  } else
    ni->add_recv_linkprobe(lp_id);                      // add linkprobe_id

  return 0;
}

int
BrnRXCorrelation::lpReceiveHandler(uint8_t *ptr, int payload_size)
{
  uint16_t lpid;
  EtherAddress neighbour_addr;
  EtherAddress source;

  NeighbourInfo *ni;
  struct rxc_neighbour_info* rxni;
  struct rxc_header* rxch = (struct rxc_header*)ptr;
  source = EtherAddress(rxch->src);
  lpid = ntohs(rxch->lp_id);

  add_recv_linkprobe(&source, lpid);
  ni = getNeighbourInfo(&source);

  BRN_DEBUG("I (%s) got linkprobe %d from %s",_me.unparse().c_str(), lpid, source.unparse().c_str());

  int index = sizeof(struct rxc_header);

  while ( index < payload_size ) {
    rxni = (struct rxc_neighbour_info*)&(ptr[index]);

    neighbour_addr = EtherAddress(rxni->mac);

    BRN_DEBUG("LINKPROBEID: ME: %s SRC: %s Neighbour: %s", _me.unparse().c_str(),
                                                               source.unparse().c_str(),
                                                               neighbour_addr.unparse().c_str());

    if ( neighbour_addr == _me ) {
      ni->add_mention_linkprobe(ntohs(rxni->first_id));

      uint16_t fid = ntohs(rxni->following_ids);
      BRN_DEBUG("ID: %d Follow: %d",ntohs(rxni->first_id),fid);

      for( int i = 0; i < MAXIDS; i++ ) {
        if ( (fid & ( 1 << i )) != 0 )
          ni->add_mention_linkprobe(ntohs(rxni->first_id) + i + 1);
      }
    }

    index += sizeof(rxc_neighbour_info);
  }

  return 0;
}

int
BrnRXCorrelation::lpSendHandler(uint8_t *ptr, int payload_size)
{
  uint16_t size;
  uint16_t n_index;
  uint16_t id_bitmap;
  uint16_t id_index;
  uint16_t id_last;
  uint16_t id_first;
  uint16_t index_first;
  struct rxc_neighbour_info* ni;

  //increase id for next message
  _linkprobe_id++;

  /* add own info ( actual id, mac address) to the header of the massage */
  struct rxc_header* rxch = (struct rxc_header*)ptr;
  memcpy(rxch->src, _me.data(), 6);
  rxch->lp_id = htons(_linkprobe_id);

  /* set pointer after the header and start to add information for each known neighbour*/
  size = sizeof(struct rxc_header);

  n_index = 0;

  while ( (n_index < _cand.size()) && ((size + sizeof(rxc_neighbour_info)) < (uint16_t)payload_size ) ) {
    ni = (struct rxc_neighbour_info*)&(ptr[size]);                                                       //pointer to next neighbourinfo
    size += sizeof(struct rxc_neighbour_info);                                                            //step pointer forward

    memcpy(ni->mac, _cand[n_index]->_addr.data(), 6);                                                     //copy address

    id_last = _cand[n_index]->get_last_recv_linkprobe();                                    //get last receive ID
    id_first = id_last - (sizeof(ni->following_ids) * 8);                                   //we want to push the last X id, so what the first id we want ?
    index_first = _cand[n_index]->get_index_of(id_first);                                   //index of the id

    ni->first_id = htons(_cand[n_index]->_ids[index_first]);                                //set first known ID

    id_bitmap = 0;                                                                          //prepare bitmap
    id_index = (index_first + 1) % MAXIDS;                                                  //set to next known ID
    while ( id_index != _cand[n_index]->_ids_next ) {
      BRN_DEBUG("Add %d for %s",_cand[n_index]->_ids[id_index], _cand[n_index]->_addr.unparse().c_str());

      //* substract 1 since otherwise the first positon is not used, since this is stores in first_id
      //TODO: add 65536 if we have overrun in id ( 65535 -> 0 )
      id_bitmap = id_bitmap | ( 1 << (((_cand[n_index]->_ids[id_index] - _cand[n_index]->_ids[index_first]/*+ 65536*/ ) % MAXIDS ) - 1));

      /* next id_index */
      id_index = (id_index + 1) % MAXIDS;
    }

    ni->following_ids = htons(id_bitmap);
    n_index++;
  }

  return(size);
}

String
BrnRXCorrelation::printNeighbourInfo()
{
  StringAccum sa;
  NeighbourInfo *_ni;

  sa << "ME (" << _linkprobe_id << "): " << _me.unparse() << "\n";
  for (int i = 0; i < _cand.size(); i++ )
  {
    _ni = _cand[i];
    sa << _ni->_addr.unparse() << " ID: ";

    int j = _ni->_ids_base;

    while ( j != _ni->_ids_next ) {
      sa << _ni->_ids[j] << " ";
      j = ( j + 1) % MAXIDS;
    }
    sa << "(" << _ni->_ids[j] << ")";
    sa << " MID: ";

    j = _ni->_my_ids_base;

    while ( j != _ni->_my_ids_next ) {
      sa << _ni->_my_ids[j] << " ";
      j = ( j + 1) % MAXMENTIONIDS;
    }
    sa << "(" << _ni->_my_ids[j] << ")";

    sa << " Interval: " << _ni->_lp_interval_in_ms;
    sa << "\n";
  }

  return sa.take_string();
}

static String
read_handler_info(Element *e, void *)
{
  return ((BrnRXCorrelation*)e)->printNeighbourInfo();
}

static int 
write_handler_debug(const String &in_s, Element *e, void *,ErrorHandler *errh)
{
  BrnRXCorrelation *brnrxc = (BrnRXCorrelation*)e;
  String s = cp_uncomment(in_s);

  int debug;

  if (!cp_integer(s, &debug))
    return errh->error("Debug is Integer");

  brnrxc->_debug = debug;

  return 0;
}

/**
 * Read the debugvalue
 * @param e
 * @param
 * @return
 */
static String
read_handler_debug(Element *e, void *)
{
  BrnRXCorrelation *brn2debug = (BrnRXCorrelation*)e;

  return String(brn2debug->_debug);
}

void
BrnRXCorrelation::add_handlers()
{
  add_read_handler("info", read_handler_info, 0);

  add_read_handler("debug", read_handler_debug, 0);
  add_write_handler("debug", write_handler_debug, 0);
}

#include <click/vector.cc>
template class Vector<BrnRXCorrelation::NeighbourInfo*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnRXCorrelation)
