#ifndef CLICK_BRNIPFLOWID_HH
#define CLICK_BRNIPFLOWID_HH
#include <click/ipaddress.hh>
#include <click/hashcode.hh>
CLICK_DECLS
class Packet;

class BRNIPFlowID { public:

  BRNIPFlowID();
  BRNIPFlowID(IPAddress, uint16_t, IPAddress, uint16_t);
  explicit BRNIPFlowID(const Packet *);	// reads ip_header and udp_header
  explicit BRNIPFlowID(const click_ip *);	// also reads adjacent TCP/UDP header

  typedef IPAddress (BRNIPFlowID::*unspecified_bool_type)() const;
  operator unspecified_bool_type() const;

  IPAddress saddr() const		{ return _saddr; }
  IPAddress daddr() const		{ return _daddr; }
  uint16_t sport() const		{ return _sport; }
  uint16_t dport() const		{ return _dport; }

  void set_saddr(IPAddress a)		{ _saddr = a; }
  void set_daddr(IPAddress a)		{ _daddr = a; }
  void set_sport(uint16_t p)		{ _sport = p; }	// network order
  void set_dport(uint16_t p)		{ _dport = p; }	// network order
  
  BRNIPFlowID rev() const;

  inline hashcode_t hashcode() const;

  String unparse() const;
  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }

 protected:
  
  // note: several functions depend on this field order!
  IPAddress _saddr;
  IPAddress _daddr;
  uint16_t _sport;			// network byte order
  uint16_t _dport;			// network byte order

};

inline
BRNIPFlowID::BRNIPFlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
BRNIPFlowID::BRNIPFlowID(IPAddress saddr, uint16_t sport,
		   IPAddress daddr, uint16_t dport)
  : _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport)
{
}

inline
BRNIPFlowID::operator unspecified_bool_type() const
{
  return _saddr || _daddr ? &BRNIPFlowID::saddr : 0;
}

inline BRNIPFlowID
BRNIPFlowID::rev() const
{
  return BRNIPFlowID(_daddr, _dport, _saddr, _sport);
}


#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))

inline hashcode_t BRNIPFlowID::hashcode() const
{ 
  // more complicated hashcode, but causes less collision
  uint16_t s = ntohs(sport());
  uint16_t d = ntohs(dport());
  hashcode_t sx = CLICK_NAME(hashcode)(saddr());
  hashcode_t dx = CLICK_NAME(hashcode)(daddr());
  return (ROT(sx, s%16)
          ^ ROT(dx, 31-d%16))
	  ^ ((d << 16) | s);
}

#undef ROT

inline bool
operator==(const BRNIPFlowID &a, const BRNIPFlowID &b)
{
  return a.dport() == b.dport() && a.sport() == b.sport()
    && a.daddr() == b.daddr() && a.saddr() == b.saddr();
}

inline bool
operator!=(const BRNIPFlowID &a, const BRNIPFlowID &b)
{
  return a.dport() != b.dport() || a.sport() != b.sport()
    || a.daddr() != b.daddr() || a.saddr() != b.saddr();
}

StringAccum &operator<<(StringAccum &, const BRNIPFlowID &);

CLICK_ENDDECLS
#endif
