// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_SENDANNOTOSOCKET_HH
#define CLICK_SENDANNOTOSOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <clicknet/ether.h>
#include <sys/un.h>
CLICK_DECLS

/*
=c

SendAnnoToSocket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
SendAnnoToSocket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
SendAnnoToSocket("UNIX", FILENAME [, I<KEYWORDS>])

=s devices

sends the packet's annotation to socket (user-level)

=d

Sends packet's annotation to the specified socket. Specifying a zero IP address
reverses the normal connection sense for TCP SendAnnoToSockets, i.e. the
FromSocket will attempt to listen(2) instead of connect(2).

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum packet length. This value (minus 4 If FRAME
is true), represents the MTU of the SendAnnoToSocket if it is used as a packet
sink.

=item FRAME

Boolean. If true, frame packets in the data stream with a 4 byte
record boundary in network order that represents the length of the
packet (including the record boundary). Default is true.

=item NODELAY

Boolean. Applies to TCP sockets only. If set, disable the Nagle
algorithm. This means that segments are always sent as soon as
possible, even if there is only a small amount of data. When not set,
data is buffered until there is a sufficient amount to send out,
thereby avoiding the frequent sending of small packets, which results
in poor utilization of the network. Default is true.

=item VERBOSE

Boolean. When true, SendAnnoToSocket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=back

=e

  ... -> SendAnnoToSocket(1.2.3.4, UDP, 47)

=a FromSocket, FromDump, ToDump, FromDevice, FromDevice.u, ToDevice, ToDevice.u */

class SendAnnoToSocket : public Element { public:

  SendAnnoToSocket();
  ~SendAnnoToSocket();

  const char *class_name() const	{ return "SendAnnoToSocket"; }
  const char *port_count() const        { return "1/1"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  //void notify_noutputs(int);

  void selected(int);

  void push(int port, Packet *);
  //Packet *simple_action(Packet *p);
  //bool run_task();

protected:
  Task _task;
  void send_packet(Packet *);

 private:

  bool _verbose;
  int _fd;
  int _active;
  int _family;
  int _socktype;
  int _protocol;
  IPAddress _ip;
  int _port;
  String _pathname;
  int _snaplen;
  union { struct sockaddr_in in; struct sockaddr_un un; } sa;
  socklen_t sa_len;
  NotifierSignal _signal;
  int _nodelay;
  bool _frame;
  EtherAddress *_me;

  int initialize_socket_error(ErrorHandler *, const char *);
  void handle_wifi(WritablePacket *, String&);
  void handle_brn(WritablePacket *, String&);
  void handle_brn_dsr(WritablePacket *, String&);
  void handle_brn_dsr_udt(WritablePacket *p, String &msg);
  void handle_ip(WritablePacket *, String&);
  void handle_arp(WritablePacket *, String&);
};

CLICK_ENDDECLS
#endif
