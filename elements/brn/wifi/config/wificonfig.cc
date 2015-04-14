#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "wificonfig.hh"

CLICK_DECLS

WifiConfig::WifiConfig():
  device_name(NULL)
{
}

 WifiConfig::WifiConfig(String dev_name)
{
  device_name = dev_name;
}

WifiConfig::~WifiConfig()
{
}

ELEMENT_PROVIDES(WifiConfig)
ELEMENT_REQUIRES(userlevel|ns)
CLICK_ENDDECLS
