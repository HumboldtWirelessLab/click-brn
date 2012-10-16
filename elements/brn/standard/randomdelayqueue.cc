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
#include "elements/brn2/brnconf.h"
#include "elements/brn2/standard/packetsendbuffer.hh"

CLICK_DECLS

RandomDelayQueue::RandomDelayQueue():
  _sendbuffer_timer(static_lookup_timer_hook,(void*)this),
  _usetsanno(false)
{
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
  click_random_srandom();

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
  struct timeval curr_time;
  curr_time = Timestamp::now().timeval();

  int nextTime = packetBuffer.getTimeToNext();

  while ( (nextTime == 1) || (nextTime == 0) )
  {
    Packet *out_packet = packetBuffer.getNextPacket();

    if ( out_packet != NULL ) output(0).push(out_packet);
    nextTime = packetBuffer.getTimeToNext();
  }

  if ( nextTime < 0 ) nextTime = 1000;

  _sendbuffer_timer.schedule_after_msec(nextTime);
}

void
RandomDelayQueue::push( int /*port*/, Packet *packet )
{
  int delay;
  Timestamp now = Timestamp::now();

  if ( _usetsanno ) {
    delay = (packet->timestamp_anno() - now).msecval();
    if ( delay < 0 ) delay = _min_diff_delay;
  } else {
    delay = (unsigned int )_min_diff_delay + ( click_random() % _delay );
  }

  packetBuffer.addPacket_ms(packet, delay, 0);
  int next_p = packetBuffer.getTimeToNext();

  _sendbuffer_timer.schedule_after_msec( next_p );
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
  if (index < packetBuffer.size()) packetBuffer.del(index);
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
