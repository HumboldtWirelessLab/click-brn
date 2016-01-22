#ifndef CLICK_GUICONNECTOR_HH
#define CLICK_GUICONNECTOR_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/timer.hh>
#include <sys/un.h>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

#define GUI_CONNECTOR_MAX_BUFFER_SIZE 2048

class GuiConnector : public BRNElement { public:

  GuiConnector();
  ~GuiConnector();

  const char *class_name() const	{ return "GuiConnector"; }
  const char *port_count() const	{ return "0/0"; }

  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  void add_handlers();

  void run_timer(Timer* );

  void communicationLoop();
  bool handleOperation(String op);
  void finish();
  Timer _timer;

private:
  int _fd;      // socket descriptor
  int _active;  // connection descriptor

  // local address to bind()
  union { struct sockaddr_in in; struct sockaddr_un un; } _local;
  socklen_t _local_len;

  // remote address to connect() to or sendto() (for
  // non-connection-mode sockets)
  union { struct sockaddr_in in; struct sockaddr_un un; } _remote;
  socklen_t _remote_len;

  IPAddress _local_ip;    // for AF_INET, address to bind()
  uint16_t _local_port;   // for AF_INET, port to bind()
  IPAddress _remote_ip;   // for AF_INET, address to connect() to or sendto()
  uint16_t _remote_port;  // for AF_INET, port to connect() to or sendto()

  BRN2NodeIdentity *_node_identity;

  int _family;      // AF_INET or AF_UNIX
  int _socktype;    // SOCK_STREAM or SOCK_DGRAM
  int _protocol;

  int _sndbuf;      // maximum socket send buffer in bytes
  int _rcvbuf;      // maximum socket receive buffer in bytes

  int initialize_socket_error(ErrorHandler *, const char *);

  char _buffer[GUI_CONNECTOR_MAX_BUFFER_SIZE];
};

CLICK_ENDDECLS
#endif
