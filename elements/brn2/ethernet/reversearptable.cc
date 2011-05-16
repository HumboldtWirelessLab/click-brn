#include <click/config.h>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>

#include "reversearptable.hh"

CLICK_DECLS

ReverseARPTable::ReverseARPTable()
    : _entry_capacity(0), _packet_capacity(2048), _expire_timer(this)
{
    _entry_count = _packet_count = _drops = 0;
}

ReverseARPTable::~ReverseARPTable()
{
}

int
ReverseARPTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Timestamp timeout(300);
    if (cp_va_kparse(conf, this, errh,
		     "CAPACITY", 0, cpUnsigned, &_packet_capacity,
		     "ENTRY_CAPACITY", 0, cpUnsigned, &_entry_capacity,
		     "TIMEOUT", 0, cpTimestamp, &timeout,
		     cpEnd) < 0)
	return -1;
    set_timeout(timeout);
    if (_timeout_j) {
	_expire_timer.initialize(this);
	_expire_timer.schedule_after_sec(_timeout_j / CLICK_HZ);
    }
    return 0;
}

void
ReverseARPTable::cleanup(CleanupStage)
{
    clear();
}

void
ReverseARPTable::clear()
{
    // Walk the arp cache table and free any stored packets and arp entries.
    for (Table::iterator it = _table.begin(); it; ) {
	ReverseARPEntry *ae = _table.erase(it);
	while (Packet *p = ae->_head) {
	    ae->_head = p->next();
	    p->kill();
	    ++_drops;
	}
	_alloc.deallocate(ae);
    }
    _entry_count = _packet_count = 0;
    _age.__clear();
}

void
ReverseARPTable::take_state(Element *e, ErrorHandler *errh)
{
    ReverseARPTable *arpt = (ReverseARPTable *)e->cast("ReverseARPTable");
    if (!arpt)
	return;
    if (_table.size() > 0) {
	errh->error("late take_state");
	return;
    }

    _table.swap(arpt->_table);
    _age.swap(arpt->_age);
    _entry_count = arpt->_entry_count;
    _packet_count = arpt->_packet_count;
    _drops = arpt->_drops;
    _alloc.swap(arpt->_alloc);

    arpt->_entry_count = 0;
    arpt->_packet_count = 0;
}

void
ReverseARPTable::slim(click_jiffies_t now)
{
    ReverseARPEntry *ae;

    // Delete old entries.
    while ((ae = _age.front())
	   && (ae->expired(now, _timeout_j)
	       || (_entry_capacity && _entry_count > _entry_capacity))) {
	_table.erase(ae->_eth);
	_age.pop_front();

	while (Packet *p = ae->_head) {
	    ae->_head = p->next();
	    p->kill();
	    --_packet_count;
	    ++_drops;
	}

	_alloc.deallocate(ae);
	--_entry_count;
    }

    // Mark entries for polling, and delete packets to make space.
    while (_packet_capacity && _packet_count > _packet_capacity) {
	while (ae->_head && _packet_count > _packet_capacity) {
	    Packet *p = ae->_head;
	    if (!(ae->_head = p->next()))
		ae->_tail = 0;
	    p->kill();
	    --_packet_count;
	    ++_drops;
	}
	ae = ae->_age_link.next();
    }
}

void
ReverseARPTable::run_timer(Timer *timer)
{
    // Expire any old entries, and make sure there's room for at least one
    // packet.
    _lock.acquire_write();
    slim(click_jiffies());
    _lock.release_write();
    if (_timeout_j)
	timer->schedule_after_sec(_timeout_j / CLICK_HZ + 1);
}

ReverseARPTable::ReverseARPEntry *
ReverseARPTable::ensure(EtherAddress eth, click_jiffies_t now)
{
    _lock.acquire_write();
    Table::iterator it = _table.find(eth);
    if (!it) {
	void *x = _alloc.allocate();
	if (!x) {
	    _lock.release_write();
	    return 0;
	}

	++_entry_count;
	if (_entry_capacity && _entry_count > _entry_capacity)
	    slim(now);

	ReverseARPEntry *ae = new(x) ReverseARPEntry(eth);
	ae->_live_at_j = now;
	ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;
	_table.set(it, ae);

	_age.push_back(ae);
    }
    return it.get();
}

int
ReverseARPTable::insert(EtherAddress eth, const IPAddress &ip, Packet **head)
{
    click_jiffies_t now = click_jiffies();
    ReverseARPEntry *ae = ensure(eth, now);
    if (!ae)
	return -ENOMEM;

    ae->_ip = ip;
    ae->_known = true; //!ip.is_broadcast();

    ae->_live_at_j = now;
    ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;

    if (ae->_age_link.next()) {
	_age.erase(ae);
	_age.push_back(ae);
    }

    if (head) {
	*head = ae->_head;
	ae->_head = ae->_tail = 0;
	for (Packet *p = *head; p; p = p->next())
	    --_packet_count;
    }

    _table.balance();
    _lock.release_write();
    return 0;
}

int
ReverseARPTable::append_query(EtherAddress eth, Packet *p)
{
    click_jiffies_t now = click_jiffies();
    ReverseARPEntry *ae = ensure(eth, now);
    if (!ae)
	return -ENOMEM;

    if (ae->known(now, _timeout_j)) {
	_lock.release_write();
	return -EAGAIN;
    }

    // Since we're still trying to send to this address, keep the entry just
    // this side of expiring.  This fixes a bug reported 5 Nov 2009 by Seiichi
    // Tetsukawa, and verified via testie, where the slim() below could delete
    // the "ae" ReverseARPEntry when "ae" was the oldest entry in the system.
    if (_timeout_j) {
	click_jiffies_t live_at_j_min = now - _timeout_j;
	if (click_jiffies_less(ae->_live_at_j, live_at_j_min)) {
	    ae->_live_at_j = live_at_j_min;
	    // Now move "ae" to the right position in the list by walking
	    // forward over other elements (potentially expensive?).
	    ReverseARPEntry *ae_next = ae->_age_link.next(), *next = ae_next;
	    while (next && click_jiffies_less(next->_live_at_j, ae->_live_at_j))
		next = next->_age_link.next();
	    if (ae_next != next) {
		_age.erase(ae);
		_age.insert(next /* might be null */, ae);
	    }
	}
    }

    ++_packet_count;
    if (_packet_capacity && _packet_count > _packet_capacity)
	slim(now);

    if (ae->_tail)
	ae->_tail->set_next(p);
    else
	ae->_head = p;
    ae->_tail = p;
    p->set_next(0);

    int r;
    if (!click_jiffies_less(now, ae->_polled_at_j + CLICK_HZ / 10)) {
	ae->_polled_at_j = now;
	r = 1;
    } else
	r = 0;

    _table.balance();
    _lock.release_write();
    return r;
}

EtherAddress
ReverseARPTable::reverse_lookup(const IPAddress &ip)
{
    _lock.acquire_read();

    EtherAddress eth;
    for (Table::iterator it = _table.begin(); it; ++it)
	if (it->_ip == ip) {
	    eth = it->_eth;
	    break;
	}

    _lock.release_read();
    return eth;
}

String
ReverseARPTable::read_handler(Element *e, void *user_data)
{
    ReverseARPTable *arpt = (ReverseARPTable *) e;
    StringAccum sa;
    click_jiffies_t now = click_jiffies();
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_table:
      sa << "<reversearptable count=\"" << arpt->length() << "\" >";
      for (ReverseARPEntry *ae = arpt->_age.front(); ae; ae = ae->_age_link.next()) {
        int ok = ae->known(now, arpt->_timeout_j);
        sa << "\t<entry mac=\"" << ae->_eth << "\" ip=\"" << ae->_ip << "\" ok=\"" << ok;
        sa << "\" age=\"" << Timestamp::make_jiffies(now - ae->_live_at_j) << "\" />";
      }
      sa << "</reversearptable>";
      break;
    }
    return sa.take_string();
}

int
ReverseARPTable::write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    ReverseARPTable *arpt = (ReverseARPTable *) e;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
      case h_insert: {
	  EtherAddress eth;
	  IPAddress ip;
	  if (cp_va_space_kparse(str, arpt, errh,
				 "ETH", cpkP+cpkM, cpEtherAddress, &eth,
				 "IP", cpkP+cpkM, cpIPAddress, &ip,
				 cpEnd) < 0)
	      return -1;
	  arpt->insert(eth, ip);
	  return 0;
      }
      case h_delete: {
	  EtherAddress eth;
	  if (cp_va_space_kparse(str, arpt, errh,
				 "ETH", cpkP+cpkM, cpEtherAddress, &eth,
				 cpEnd) < 0)
	      return -1;
	  arpt->insert(eth, IPAddress::make_broadcast()); // XXX?
	  return 0;
      }
      case h_clear:
	arpt->clear();
	return 0;
      default:
	return -1;
    }
}

void
ReverseARPTable::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("count", Handler::OP_READ, &_entry_count);
    add_data_handlers("length", Handler::OP_READ, &_packet_count);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ReverseARPTable)
ELEMENT_MT_SAFE(ReverseARPTable)
