// -*- c-basic-offset: 2; related-file-name: "../../../elements/standard/addressinfo.cc" -*-
#ifndef CLICK_BRNADDRESSINFO_HH
#define CLICK_BRNADDRESSINFO_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif
CLICK_DECLS

/*
=c

BRNAddressInfo(NAME ADDRESS [ADDRESS...], ...)

=s information

specifies address information

=io

None

=d

Lets you use mnemonic names for IPv4 and IPv6 addresses, IPv4 and IPv6
address prefixes, and Ethernet addresses. Each argument has the form `NAME
ADDRESS [ADDRESS...]', which associates the given ADDRESSes with NAME. For
example, if a configuration contains this AddressInfo element,

   AddressInfo(mauer 10.0.0.1, mazu 10.0.0.10);

then other configuration strings can use C<mauer> and C<mazu> as mnemonics
for the IP addresses 10.0.0.1 and 10.0.0.10, respectively.

The mnemonic names introduced by AddressInfo elements are local with
respect to compound elements. That is, names created inside a compound
element apply only within that compound element and its subelements. For
example:

   AddressInfo(mauer 10.0.0.1);
   compound :: {
     AddressInfo(mazu 10.0.0.10);
     ... -> IPEncap(6, mauer, mazu) -> ...  // OK
   };
   ... -> IPEncap(6, mauer, mazu) -> ...    // error: `mazu' undefined

Any name can be simultaneously associated with an IP address, an IP network
address, and an Ethernet address. The kind of address that is returned is
generally determined from context. For example:

   AddressInfo(mauer 10.0.0.1/8 00-50-BA-85-84-A9);
   ... -> IPEncap(6, mauer, ...)                  // as IP address
       -> EtherEncap(0x0800, mauer, ...) -> ...   // as Ethernet address
   ... -> ARPResponder(mauer) -> ...              // as IP prefix AND Ethernet address!

An optional suffix makes the context unambiguous. C<NAME> is an ambiguous
reference to some address, but C<NAME:ip> is always an IPv4 address,
C<NAME:ipnet> is always an IPv4 network address (IPv4 address prefix),
C<NAME:ip6> is always an IPv6 address, C<NAME:ip6net> is always an IPv6
network address, and C<NAME:eth> is always an Ethernet address.

=head1 DEFAULT ADDRESSES

If you do not define an address for a given name, AddressInfo will use the
default, if any.  Defaults are as follows:

=over 2

=item *

If DEVNAME is the name of an Ethernet device, then C<DEVNAME:eth> defaults to
DEVNAME's Ethernet address.  (At userlevel, this works only on BSD and Linux.)

=item *

C<DEVNAME:ip> defaults to the first primary IPv4 address associated with the
device DEVNAME.

=back

These defaults are not available on all platforms.

=a

PortInfo */

#define ARPHRD_80211_PRISM	802       /* wifi + Prism2 header */
#define ARPHRD_80211_RADIOTAP	803       /* wifi + Radiotap header */
#define ARPHRD_80211_ATHDESC	804       /* wifi + Atheros header */
#define ARPHRD_80211_ATHDESCEXT	805       /* wifi + Atheros header + X */

class BRNAddressInfo : public Element { public:
  
  BRNAddressInfo();
  ~BRNAddressInfo();
  
  const char *class_name() const	{ return "BRNAddressInfo"; }
  
  int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
  int configure(Vector<String> &, ErrorHandler *);

  static bool query_ip(String, unsigned char *, const Element *);
  static bool query_ip_prefix(String, unsigned char *, unsigned char *, const Element *);
#ifdef HAVE_IP6
  static bool query_ip6(String, unsigned char *, const Element *);
  static bool query_ip6_prefix(String, unsigned char *, int *, const Element *);
#endif
  static bool query_ethernet(String, unsigned char *, const Element *);

};

CLICK_ENDDECLS
#endif