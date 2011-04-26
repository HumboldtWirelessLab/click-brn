#include <sys/socket.h>
#include <netinet/in.h>

struct socket_connection {
  int fd;
  struct  sockaddr_in local;
  struct  sockaddr_in peer;
};

struct socket_connection *open_socket_connection(char *peername, int peerport, int localport);
int send_to_peer(struct socket_connection *con, char *buffer, int size);
int recv_from_peer(struct socket_connection *con, char *buffer, int size);
int close_connection(struct socket_connection *con);
