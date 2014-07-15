#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <elements/brn/standard/brnaddressinfo.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "linux_rt_ctrl.hh"

CLICK_DECLS

LinuxRTCtrl::LinuxRTCtrl():
  rt_sockfd(-1),
  null_ip(),
  uid(1)
{
  BRNElement::init();
}

LinuxRTCtrl::~LinuxRTCtrl()
{
  click_chatter("Close");
  close(rt_sockfd);
}

int
LinuxRTCtrl::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
LinuxRTCtrl::initialize(ErrorHandler *)
{
  rt_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if ( rt_sockfd == -1) {
    BRN_WARN("socket is -1\n");
  }

  uid = getuid();

  if (!uid) {
    BRN_WARN("Click is not running with root privilege. You will not be able to change routes!");
  }

  return 0;
}


String
LinuxRTCtrl::linux_rt_info()
{
  StringAccum sa;
  sa << "<linuxroutingtable node=\"" << BRN_NODE_NAME << "\" >\n";

  /*
  for ( uint32_t i = 0; i < _no_queues; i++ )
    sa << "\t<entry dest=\"" << i << "\" mask=\"" << (uint32_t)_cwmin[i] << "\" gw=\"" << (uint32_t)_cwmax[i] << "\" interface=\"" << (uint32_t)_aifs[i] << "\" prio=\"" << (uint32_t)_cwmin[i] << "\" />\n";
*/
  sa << "</linuxroutingtable>\n";

  return sa.take_string();
}

int
LinuxRTCtrl::set_sockaddr(struct sockaddr_in *sockaddr, IPAddress &ip)
{
  sockaddr->sin_family = AF_INET;
  if (ip) {
    //BRN_DEBUG("set: %s",ip.unparse().c_str());
    sockaddr->sin_addr.s_addr = inet_addr(ip.unparse().c_str());//(in_addr_t)ip.data();
  } else {
    //BRN_DEBUG("set 0: %s",ip.unparse().c_str());
    sockaddr->sin_addr.s_addr = 0;
  }
  sockaddr->sin_port = 0;

  return 0;
}

int
LinuxRTCtrl::set_rt_entry(struct rtentry *rm, IPAddress &ip, IPAddress &mask, IPAddress &gw)
{
  memset(rm, 0, sizeof(struct rtentry));

  set_sockaddr((struct sockaddr_in*)&rm->rt_dst, ip);
  set_sockaddr((struct sockaddr_in*)&rm->rt_genmask, mask);
  set_sockaddr((struct sockaddr_in*)&rm->rt_gateway, gw);

  rm->rt_flags = RTF_UP | RTF_GATEWAY;
  return 0;
}


int
LinuxRTCtrl::set_rt_entry_metric(struct rtentry *rm, int metric)
{
  rm->rt_metric = metric;
  return 0;
}

int
LinuxRTCtrl::add_rt_entry(struct rtentry *rm)
{
  int err;

  if ( uid != 0 ) return 0;

  if ((err = ioctl(rt_sockfd, SIOCADDRT, rm)) < 0) {
    BRN_ERROR("SIOCADDRT failed , ret->%d %d\n",err, errno);
    switch (errno) {
      case EEXIST:
        BRN_ERROR("EXIST");
        break;
      case ENETUNREACH:
        BRN_ERROR("ENETUNREACH");
        break;
      case EPERM:
        BRN_ERROR("EPERM");
        break;
      default:
        BRN_ERROR("UNKNOWN");
    }
    return -1;
  }

  return 0;
}

int
LinuxRTCtrl::del_rt_entry(struct rtentry *rm)
{
  int err;

  if ( uid != 0 ) return 0;

  if ((err = ioctl(rt_sockfd, SIOCDELRT, rm)) < 0) {
    BRN_ERROR("SIOCDELRT failed , ret->%d\n",err);
        switch (errno) {
      case EEXIST:
        BRN_ERROR("EXIST");
        break;
      case ENETUNREACH:
        BRN_ERROR("ENETUNREACH");
        break;
      case EPERM:
        BRN_ERROR("EPERM");
        break;
      default:
        BRN_ERROR("UNKNOWN");
    }
    return -1;
  }

  return 0;
}

int
LinuxRTCtrl::update_rt_entry(struct rtentry *rm)
{
  int err;

  if ( uid != 0 ) return 0;

  if ((err = ioctl(rt_sockfd, SIOCADDRT, rm)) < 0) {
    BRN_ERROR("SIOCADDRT failed , ret->%d %d\n",err, errno);
    switch (errno) {
      case EEXIST:
        BRN_ERROR("EXIST");
        break;
      case ENETUNREACH:
        BRN_ERROR("ENETUNREACH");
        break;
      case EPERM:
        BRN_ERROR("EPERM");
        break;
      default:
        BRN_ERROR("UNKNOWN");
    }
    return -1;
  }

  return 0;
}

int
LinuxRTCtrl::add_route(IPAddress &ip, IPAddress &mask, IPAddress &gw)
{
  struct rtentry rm;

  BRN_DEBUG("ADD: host/net: %s netmask: %s gw: %s",ip.unparse().c_str(), mask.unparse().c_str(), gw.unparse().c_str());

  set_rt_entry(&rm, ip, mask, gw);
  return add_rt_entry(&rm);
}

int
LinuxRTCtrl::del_route(IPAddress &ip, IPAddress &mask, IPAddress &gw)
{
  struct rtentry rm;

  BRN_DEBUG("DEL: host/net: %s netmask: %s gw: %s",ip.unparse().c_str(), mask.unparse().c_str(), gw.unparse().c_str());

  set_rt_entry(&rm, ip, mask, gw);
  return del_rt_entry(&rm);
}

int
LinuxRTCtrl::add_default_gateway(IPAddress &default_gw)
{
  return add_route(null_ip, null_ip, default_gw);
}

int
LinuxRTCtrl::del_default_gateway(IPAddress &default_gw)
{
  return del_route(null_ip, null_ip, default_gw);
}

int
LinuxRTCtrl::linux_rt_update(const String &s)
{
  Vector<String> args;

  cp_spacevec(s, args);

  if ( args.size() < 4 ) {
    BRN_WARN("RT update: use (del|add) host netmask gw");
    return 0;
  }

  IPAddress net_address;
  IPAddress subnet_mask;
  IPAddress gw_address;

  cp_ip_address(args[1],&net_address);
  cp_ip_address(args[2],&subnet_mask);
  cp_ip_address(args[3],&gw_address);

  if ( args[0] == "add" ) {
    add_route(net_address, subnet_mask, gw_address);
  } else if ( args[0] == "del" ) {
    del_route(net_address, subnet_mask, gw_address);
  }

  return 0;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_rt_info(Element *e, void *)
{
  LinuxRTCtrl *rt = (LinuxRTCtrl *)e;
  return rt->linux_rt_info();
}

static int
write_rt_table(const String &in_s, Element *e, void *, ErrorHandler */*errh*/)
{
  LinuxRTCtrl *rt = (LinuxRTCtrl *)e;
  rt->linux_rt_update(in_s);
  return 0;
}

void
LinuxRTCtrl::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("rt", read_rt_info, 0);

  add_write_handler("rt", write_rt_table, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LinuxRTCtrl)
