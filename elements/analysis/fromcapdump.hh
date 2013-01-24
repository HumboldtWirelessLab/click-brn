// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMCAPDUMP_HH
#define CLICK_FROMCAPDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/ipflowid.hh>
#include <click/fromfile.hh>
CLICK_DECLS

/*
=c

FromCapDump(FILENAME [, I<KEYWORDS>])

=s traces

reads packets from a 'cap' output file

=d

Reads TCP packet descriptors from a file produced by Mark Allman's 'cap' tool
and emits the corresponding TCP packets.  Here's an example 'cap' file:

   SND 0
   INF - cap v2.13
   SYN > 1046102982.856142 0 0 40 0
   SYN < 1046102982.931832 0 0 40 0
   DAT > 1046102982.931941 1 1 1500 1460
   ACK < 1046102983.156387 1 1 40 0
   DAT > 1046102983.156531 2 2 1500 1460
   ...
   DAT > 1046102996.137294 5068 5000 1500 1460 FIN
   ACK < 1046102996.215164 5068 5000 40 0 FIN

C<SYN>, C<DAT>, and C<ACK> lines have the following fields: packet type,
direction, timestamp (in seconds past the epoch), unique packet number, packet
sequence number, packet IP length, payload length, and optional flags.

The file may be compressed with gzip(1) or bzip2(1); FromCapDump will
run zcat(1) or bzcat(1) to uncompress it.

FromCapDump reads from the file named FILENAME unless FILENAME is a
single dash `C<->', in which case it reads from the standard input. It will
not uncompress the standard input, however.

Output packets have timestamp, aggregate, paint, and packet number
annotations.  The paint annotation is 0 for data packets and 1 for
acknowledgements.  The first packet number annotation equals the unique packet
number; the second equals the packet sequence number.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, then FromCapDump will ask the router to stop when it
is done reading. Default is false.

=item ACTIVE

Boolean. If false, then FromCapDump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item ZERO

Boolean. Determines the contents of packet data not set by the dump. If true
E<lparen>the default), this data is zero. If false, it is random garbage.

=item CHECKSUM

Boolean. If true, then output packets' IP, TCP, and UDP checksums are set. If
false (the default), the checksum fields contain random garbage.

=item SAMPLE

Unsigned real number between 0 and 1. FromCapDump will output each
packet with probability SAMPLE. Default is 1. FromCapDump uses
fixed-point arithmetic, so the actual sampling probability may differ
substantially from the requested sampling probability. Use the
C<sampling_prob> handler to find out the actual probability. If MULTIPACKET is
true, then the sampling probability applies separately to the multiple packets
generated per record.

=item FLOWID

String, containing a space-separated flow ID (source address, source port,
destination address, destination port, and, optionally, protocol). Defines the
IP addresses and ports on output packets.  If no FLOWID is given, FromCapDump
uses C<1.0.0.1 1 2.0.0.2 2>.

=item AGGREGATE

Unsigned integer.  Defines the aggregate annotation for output packets.
Default is 1.

=back

Only available in user-level processes.

=n

Packets generated by FromCapDump always have IP version 4 and a correct
IP header length. The rest of the packet data is zero or garbage, unless set
by the dump. Generated packets will usually have short lengths, but the extra
header length annotations are set correctly.

FromCapDump is a notifier signal, active when the element is active and
the dump contains more packets.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns 'IP'. Useful for ToDump's USE_ENCAP_FROM option.

=h filesize read-only

Returns the length of the FromCapDump file, in bytes, or "-" if that
length cannot be determined.

=h filepos read-only

Returns FromCapDump's position in the file, in bytes.

=h stop write-only

When written, sets `active' to false and stops the driver.

=a

FromIPSummaryDump, ToIPSummaryDump

Mark Allman, "Measuring End-to-End Bulk Transfer Capacity", Proc. Internet
Measurement Workshop 2001.*/

class FromCapDump : public Element { public:

    FromCapDump() CLICK_COLD;
    ~FromCapDump() CLICK_COLD;

    const char *class_name() const	{ return "FromCapDump"; }
    const char *port_count() const	{ return PORTS_0_1; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);
    Packet *pull(int);

  private:

    enum { SAMPLING_SHIFT = 28 };
    enum { NO_SEQNO = 0xFFFFFFFFU };

    FromFile _ff;

    uint32_t _sampling_prob;
    IPFlowID _flowid;
    bool _flowid_is_rcv;
    uint32_t _aggregate;

    Vector<uint32_t> _p2s_map;

    bool _stop : 1;
    bool _zero : 1;
    bool _checksum : 1;
    bool _binary : 1;
    bool _active;

    Task _task;
    ActiveNotifier _notifier;

    uint32_t packno2seqno(uint32_t packno);
    uint32_t packno2seqno(uint32_t packno, int len);

    Packet *read_packet(ErrorHandler *);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
