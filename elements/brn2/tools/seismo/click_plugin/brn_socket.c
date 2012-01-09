#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include "brn_socket.h"

struct socket_connection *open_socket_connection(char *peername, int peerport, int localport) {

  struct socket_connection *con;
  struct hostent *hp;
  //struct hostent *gethostbyname();

  con = (struct socket_connection *)malloc(sizeof(struct socket_connection));

  con->local.sin_family = AF_INET;
  con->local.sin_addr.s_addr = htonl(INADDR_ANY);
  con->local.sin_port = htons(localport);
  con->fd = socket (AF_INET,SOCK_DGRAM,0);

  bind(con->fd, (struct sockaddr*) &(con->local), sizeof(con->local));

			 
  con->peer.sin_family = AF_INET;
  hp = gethostbyname(peername);
  bcopy ( hp->h_addr, &(con->peer.sin_addr.s_addr), hp->h_length);
  con->peer.sin_port = htons(peerport);

  return con;							 

}

int send_to_peer(struct socket_connection *con, char *buffer, int size) {
   sendto(con->fd, buffer, size, 0, (struct sockaddr*)&(con->peer), sizeof(con->peer));
   return 0;  //TODO: use return-value
}

int recv_from_peer(struct socket_connection *con, char *buffer, int size) {
  int rc;
  struct sockaddr_in from;
  socklen_t flen;
  flen = sizeof(struct sockaddr);

  rc = recvfrom(con->fd, buffer, size, 0,  (struct sockaddr*)&from, &flen);

  return rc;
}


int close_connection(struct socket_connection *con) {
  close(con->fd);

  return 0;
}
