#ifndef LINUXRTCTRL_HH
#define LINUXRTCTRL_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif

#include "elements/brn/brnelement.hh"

/* System stuff */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>      /* init.c, egp2.c, rt_egp.c */

#include <netdb.h>

#include <net/if.h>     /* init.c */
#include <net/route.h>
/* system stuff end  */

CLICK_DECLS

class LinuxRTCtrl : public BRNElement {
  public:
    //
    //methods
    //
    LinuxRTCtrl();
    ~LinuxRTCtrl();

    const char *class_name() const  { return "LinuxRTCtrl"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return false; }

    int initialize(ErrorHandler *);
    void add_handlers();

    String linux_rt_info();
    int linux_rt_update(const String &in_s);

  private:

    int rt_sockfd;
    //
    //member
    //

    IPAddress null_ip;

  public:
    static int set_sockaddr(struct sockaddr_in *sockaddr, IPAddress &ip);
    static int set_rt_entry(struct rtentry *rm, IPAddress &ip, IPAddress &mask, IPAddress &gw);
    int add_rt_entry(struct rtentry *rm);
    int del_rt_entry(struct rtentry *rm);
    int add_route(IPAddress &ip, IPAddress &mask, IPAddress &gw);
    int del_route(IPAddress &ip, IPAddress &mask, IPAddress &gw);
    int add_default_gateway(IPAddress &default_gw);
    int del_default_gateway(IPAddress &default_gw);
};

CLICK_ENDDECLS
#endif
