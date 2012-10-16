#ifndef CLICK_REVERSEARPTABLE_HH
#define CLICK_REVERSEARPTABLE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
CLICK_DECLS


class ReverseARPTable : public Element { public:

    ReverseARPTable();
    ~ReverseARPTable();

    const char *class_name() const		{ return "ReverseARPTable"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    void take_state(Element *, ErrorHandler *);
    void add_handlers();
    void cleanup(CleanupStage);

    int lookup(EtherAddress eth, IPAddress *ip, uint32_t poll_timeout_j);
    IPAddress lookup(EtherAddress eth);
    EtherAddress reverse_lookup(const IPAddress &ip);
    int insert(EtherAddress eth, const IPAddress &ip, Packet **head = 0);
    int append_query(EtherAddress eth, Packet *p);
    void clear();

    uint32_t capacity() const {
	return _packet_capacity;
    }
    void set_capacity(uint32_t capacity) {
	_packet_capacity = capacity;
    }
    uint32_t entry_capacity() const {
	return _entry_capacity;
    }
    void set_entry_capacity(uint32_t entry_capacity) {
	_entry_capacity = entry_capacity;
    }
    Timestamp timeout() const {
	return Timestamp::make_jiffies((click_jiffies_t) _timeout_j);
    }
    void set_timeout(const Timestamp &timeout) {
	if ((uint32_t) timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	    _timeout_j = 0;
	else
	    _timeout_j = timeout.jiffies();
    }

    uint32_t drops() const {
	return _drops;
    }
    uint32_t count() const {
	return _entry_count;
    }
    uint32_t length() const {
	return _packet_count;
    }

    void run_timer(Timer *);

    enum {
	h_table, h_insert, h_delete, h_clear
    };
    static String read_handler(Element *e, void *user_data);
    static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh);

    struct ReverseARPEntry {		// This structure is now larger than I'd like
	EtherAddress _eth;		// (40B) but probably still fine.
	ReverseARPEntry *_hashnext;
	IPAddress _ip;
	bool _known;
	click_jiffies_t _live_at_j;
	click_jiffies_t _polled_at_j;
	Packet *_head;
	Packet *_tail;
	List_member<ReverseARPEntry> _age_link;
	typedef EtherAddress key_type;
	typedef EtherAddress key_const_reference;
	key_const_reference hashkey() const {
	    return _eth;
	}
	bool expired(click_jiffies_t now, uint32_t timeout_j) const {
	    return click_jiffies_less(_live_at_j + timeout_j, now)
		&& timeout_j;
	}
	bool known(click_jiffies_t now, uint32_t timeout_j) const {
	    return _known && !expired(now, timeout_j);
	}
	ReverseARPEntry(EtherAddress eth)
	    : _eth(eth), _hashnext(), _ip(IPAddress::make_broadcast()),
	      _known(false), _head(), _tail() {
	}
    };

  private:

    ReadWriteLock _lock;

    typedef HashContainer<ReverseARPEntry> Table;
    Table _table;
    typedef List<ReverseARPEntry, &ReverseARPEntry::_age_link> AgeList;
    AgeList _age;
    atomic_uint32_t _entry_count;
    atomic_uint32_t _packet_count;
    uint32_t _entry_capacity;
    uint32_t _packet_capacity;
    uint32_t _timeout_j;
    atomic_uint32_t _drops;
    SizedHashAllocator<sizeof(ReverseARPEntry)> _alloc;
    Timer _expire_timer;

    ReverseARPEntry *ensure(EtherAddress eth, click_jiffies_t now);
    void slim(click_jiffies_t now);

};

inline int
ReverseARPTable::lookup(EtherAddress eth, IPAddress *ip, uint32_t poll_timeout_j)
{
    _lock.acquire_read();
    int r = -1;
    if (Table::iterator it = _table.find(eth)) {
	click_jiffies_t now = click_jiffies();
	if (it->known(now, _timeout_j)) {
	    *ip = it->_ip;
	    if (poll_timeout_j
		&& !click_jiffies_less(now, it->_live_at_j + poll_timeout_j)
		&& !click_jiffies_less(now, it->_polled_at_j + (CLICK_HZ / 10))) {
		it->_polled_at_j = now;
		r = 1;
	    } else
		r = 0;
	}
    }
    _lock.release_read();
    return r;
}

inline IPAddress
ReverseARPTable::lookup(EtherAddress eth)
{
    IPAddress ip;
    if (lookup(eth, &ip, 0) >= 0)
	return ip;
    else
	return IPAddress::make_broadcast();
}

CLICK_ENDDECLS
#endif
