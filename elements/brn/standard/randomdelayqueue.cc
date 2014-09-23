/* OMNISCIENT
  DHT knows all othe nodes in the network. For Discovery, flooding is used.
  Everytime the routingtable of a node changed, it floods all new information.
  Node-fault detection is done by neighboring nodes
*/
#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "randomdelayqueue.hh"
#include "elements/brn/brnconf.h"
#include "elements/brn/standard/packetsendbuffer.hh"

CLICK_DECLS

RandomDelayQueue::RandomDelayQueue():
  _sendbuffer_timer(static_lookup_timer_hook,(void*)this),
  _usetsanno(false)
{
  BRNElement::init();
}

RandomDelayQueue::~RandomDelayQueue()
{
}

int
RandomDelayQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int max_delay;

  if (cp_va_kparse(conf, this, errh,
    "MINDELAY", cpkP+cpkM, cpInteger, &_min_delay, /*"minimal delay"*/
    "MAXDELAY", cpkP+cpkM, cpInteger, &max_delay, /*"maximal delay"*/
    "DIFFDELAY", cpkP+cpkM, cpInteger, &_min_diff_delay, /*"min delay between 2 Packets"*/
    "TIMESTAMPANNOS", cpkP, cpBool, &_usetsanno, /*use anna*/
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  _delay = max_delay - _min_delay;
  return 0;
}

int
RandomDelayQueue::initialize(ErrorHandler *)
{
  click_brn_srandom();

  _sendbuffer_timer.initialize(this);
  return 0;
}

void
RandomDelayQueue::static_lookup_timer_hook(Timer *, void *f)
{
  ((RandomDelayQueue*)f)->queue_timer_hook();
}

void
RandomDelayQueue::queue_timer_hook()
{
  int nextTime = packetBuffer.getTimeToNext();
  //click_chatter("Random: %d",nextTime);

  while ((nextTime == 0) || (nextTime == 1))
  {
    Packet *out_packet = packetBuffer.getNextPacket();

    if ( out_packet != NULL ) output(0).push(out_packet);
    nextTime = packetBuffer.getTimeToNext();
    //click_chatter("Random: %d",nextTime);
  }

  if (nextTime < 0) return;

  if (nextTime < _min_diff_delay) nextTime = _min_diff_delay;

  if ( _sendbuffer_timer.scheduled() )
    _sendbuffer_timer.reschedule_after_msec(nextTime);
  else
    _sendbuffer_timer.schedule_after_msec(nextTime);
}

void
RandomDelayQueue::push( int /*port*/, Packet *packet )
{
  int delay;
  Timestamp now = Timestamp::now();

  if ( _usetsanno ) {
    delay = (packet->timestamp_anno() - now).msecval();
    if ( delay <= 0 ) {
      output(0).push(packet);
      return;
    }
  } else {
    delay = (unsigned int )_min_delay + ( click_random() % _delay );
  }

  packetBuffer.addPacket_ms(packet, delay, 0);

  int nextTime = packetBuffer.getTimeToNext();
  if (nextTime < _min_diff_delay) nextTime = _min_diff_delay;

  if ( _sendbuffer_timer.scheduled() )
    _sendbuffer_timer.reschedule_after_msec(nextTime);
  else
    _sendbuffer_timer.schedule_after_msec(nextTime);
}

Packet*
RandomDelayQueue::get_packet(int index)
{
  if (index < packetBuffer.size()) return packetBuffer.get(index)->_p;
  return NULL;
}

void
RandomDelayQueue::remove_packet(int index)
{
  if (index < packetBuffer.size()) {
    packetBuffer.del(index);

    int nextTime = packetBuffer.getTimeToNext();

    if (nextTime < 0) return;
    if (nextTime == 0) queue_timer_hook();

    if (_sendbuffer_timer.scheduled())
      _sendbuffer_timer.reschedule_after_msec(nextTime);
    else
      _sendbuffer_timer.schedule_after_msec(nextTime);
  }
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
RandomDelayQueue::delay_queue_info(void)
{
  StringAccum sa;
  sa << "\n";
  return sa.take_string();
}

enum {
  H_INFO
};

static String
read_param(Element *e, void *thunk)
{
  RandomDelayQueue *rdq = (RandomDelayQueue *)e;

  switch ((uintptr_t) thunk)
  {
    case H_INFO : return ( rdq->delay_queue_info( ) );
    default: return String();
  }
}

void RandomDelayQueue::add_handlers()
{
  add_read_handler("queue_info", read_param , (void *)H_INFO);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketSendBuffer)
EXPORT_ELEMENT(RandomDelayQueue)
