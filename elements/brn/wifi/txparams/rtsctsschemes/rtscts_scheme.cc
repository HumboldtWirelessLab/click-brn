#include <click/config.h>
#include "rtscts_scheme.hh"

CLICK_DECLS

RtsCtsScheme::RtsCtsScheme()
{
}

RtsCtsScheme::~RtsCtsScheme()
{
}

void *
RtsCtsScheme::cast(const char *name)
{
  if (strcmp(name, "RtsCtsScheme") == 0)
    return dynamic_cast<RtsCtsScheme *>(this);
  else if (strcmp(name, "Scheme") == 0)
    return dynamic_cast<Scheme *>(this);
  else
    return NULL;
}

void RtsCtsScheme::handle_feedback(PacketInfo */*pinfo*/)
{
}

void RtsCtsScheme::set_conf()
{
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(RtsCtsScheme)

