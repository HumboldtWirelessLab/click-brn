#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brnrxcorrelationstats.hh"

CLICK_DECLS

BrnRXCorrelationStats::BrnRXCorrelationStats()
  :_debug(BrnLogger::DEFAULT)
{
}

BrnRXCorrelationStats::~BrnRXCorrelationStats()
{
}

int
BrnRXCorrelationStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _note_lp = 16;

  if (cp_va_kparse(conf, this, errh,
      "RXCORRELATION", cpkP+cpkM, cpElement, &_rxcorr,
      "NOTELP", cpkP, cpInteger, &_note_lp,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BrnRXCorrelationStats::initialize(ErrorHandler *)
{
  return 0;
}

int
BrnRXCorrelationStats::calculatedPER(BrnRXCorrelation::NeighbourInfo *cand)
{
  return cand->getBackwardPER(_rxcorr->getLPID());
}

int
BrnRXCorrelationStats::calculatedPERPair(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB)
{
  uint32_t last_poss_lp = _rxcorr->getLPID();
  uint32_t min_lp;
  uint32_t recvlp;
  bool ina, inb;

  int base_a, base_b, id_size_a, id_size_b;

  min_lp = last_poss_lp - _note_lp + 1;
  id_size_a = candA->getMIDSLen();
  id_size_b = candB->getMIDSLen();

  base_a = candA->_my_ids_base;
  base_b = candB->_my_ids_base;

  while ( (candA->_my_ids[base_a] < min_lp) && (base_a != candA->_my_ids_next) ) base_a = (base_a + 1) % id_size_a;
  while ( (candB->_my_ids[base_b] < min_lp) && (base_b != candB->_my_ids_next) ) base_b = (base_b + 1) % id_size_b;

  recvlp = 0;

  for ( uint32_t i = min_lp; i <= last_poss_lp; i++ ) {
    ina = false;
    inb = false;
    BRN_DEBUG("Search for: %d",i);

    for ( uint32_t a = base_a; a != candA->_my_ids_next; a = ( a + 1 ) % id_size_a ) {
      if ( candA->_my_ids[a] == i ) {
        ina = true;
        break;
      }
      if ( candA->_my_ids[a] > i ) break;
    }

    if ( ! ina ) {
      for ( uint32_t b = base_b; b != candB->_my_ids_next; b = ( b + 1 ) % id_size_b ) {
        if ( candB->_my_ids[b] == i ) {
          inb = true;
          break;
        }
        if ( candB->_my_ids[b] > i ) break;
      }
    }

    if ( ina || inb ) recvlp++;
  }

  BRN_DEBUG("Found: %d of %d",recvlp, _note_lp);

  return ((100 * recvlp) / _note_lp ) ;
}

int
BrnRXCorrelationStats::calculatedPERIndepend(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB)
{
  int pera, perb, res;

  pera = candA->getBackwardPER(_rxcorr->getLPID());
  perb = candB->getBackwardPER(_rxcorr->getLPID());

  res = pera + (((100 - pera) * perb) / 100 );

  if ( res < 0 ) res = 0;
  if ( res > 100 ) res = 100;

  return res;
}

int
BrnRXCorrelationStats::calculatedPERDependPos(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB)
{
  int pera, perb, res;

  pera = candA->getBackwardPER(_rxcorr->getLPID());
  perb = candB->getBackwardPER(_rxcorr->getLPID());

  if ( pera > perb ) res = pera;
  else res = perb;

  return res;
}

int
BrnRXCorrelationStats::calculatedPERDependNeg(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB)
{
  int res;

  if ((res = candA->getBackwardPER(_rxcorr->getLPID())+candB->getBackwardPER(_rxcorr->getLPID())) > 100 ) return 100;

  return res;
}

String BrnRXCorrelationStats::printSinglePerInfo()
{
  StringAccum sa;
  BrnRXCorrelation::NeighbourInfo *_ni;
  int cni = _rxcorr->countNeighbours();

  for ( int i = 0; i < cni; i++ ) {
    _ni = _rxcorr->getNeighbourInfo(i);
    sa << "Node (" << _ni->_addr.unparse() << ") -> Me (" << _rxcorr->_me.unparse() << "): " << _ni->getPER() << " Last Possible: " << _ni->get_last_possible_rcv_lp();
    sa << " (" << _ni->_ids[_ni->_ids_base] << "," << _ni->_ids_next << "," << _ni->_ids_base << ")";
    sa << "  Me (" << _rxcorr->_me.unparse() << ") -> Node (" << _ni->_addr.unparse() << "): " << _ni->getBackwardPER(_rxcorr->getLPID());
    sa << " (" << _rxcorr->getLPID() << "," << _ni->_my_ids[_ni->_my_ids_base] << "," << _ni->_my_ids_next << "," << _ni->_my_ids_base << ")";
    sa << "\n";
  }

  return sa.take_string();
}

String BrnRXCorrelationStats::printPairPerInfo()
{
  StringAccum sa;
  BrnRXCorrelation::NeighbourInfo *_ni_1;
  BrnRXCorrelation::NeighbourInfo *_ni_2;
  int cni = _rxcorr->countNeighbours();

  for ( int i = 0; i < cni-1; i++ ) {
    _ni_1 = _rxcorr->getNeighbourInfo(i);

    for ( int j = i+1; j < cni; j++ ) {
      _ni_2 = _rxcorr->getNeighbourInfo(j);

      sa << "Me (" << _rxcorr->_me.unparse() << ") -> Nodes (" << _ni_1->_addr.unparse() << ", " << _ni_2->_addr.unparse() << ") : ";
      sa << "PERPair: " << calculatedPERPair(_ni_1,_ni_2) << "  PERIndepentend: " << calculatedPERIndepend(_ni_1,_ni_2);
      sa << "  PERPos: " << calculatedPERDependPos(_ni_1,_ni_2) << "  PERNeg: " << calculatedPERDependNeg(_ni_1,_ni_2) << "\n";
    }
  }

  return sa.take_string();
}

static String
read_handler_single_per(Element *e, void *)
{
  return ((BrnRXCorrelationStats*)e)->printSinglePerInfo();
}

static String
read_handler_pair_per(Element *e, void *)
{
  return ((BrnRXCorrelationStats*)e)->printPairPerInfo();
}

static int 
write_handler_debug(const String &in_s, Element *e, void *,ErrorHandler *errh)
{
  BrnRXCorrelationStats *rxcs = (BrnRXCorrelationStats*)e;
  String s = cp_uncomment(in_s);

  int debug;

  if (!cp_integer(s, &debug))
    return errh->error("Debug is Integer");

  rxcs->_debug = debug;

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
  BrnRXCorrelationStats *rxcs = (BrnRXCorrelationStats*)e;

  return String(rxcs->_debug);
}

void
BrnRXCorrelationStats::add_handlers()
{
  add_read_handler("singleper", read_handler_single_per, 0);
  add_read_handler("pairper", read_handler_pair_per, 0);

  add_read_handler("debug", read_handler_debug, 0);
  add_write_handler("debug", write_handler_debug, 0);
}

#include <click/vector.cc>
template class Vector<BrnRXCorrelation::NeighbourInfo*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnRXCorrelationStats)
