// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * tosocket.{cc,hh} -- element write data to socket
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif //HAVE_UNISTD_H
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <fcntl.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/straccum.hh>
#include "sendannotosocket.hh"
#include "brn.h"

CLICK_DECLS

SendAnnoToSocket::SendAnnoToSocket()
  : _task(this), _verbose(false), _fd(-1), _active(-1),
    _snaplen(2048), _nodelay(1), _frame(true)
{
  _me = new EtherAddress();
}

SendAnnoToSocket::~SendAnnoToSocket()
{
  delete _me;
}

/*
void
SendAnnoToSocket::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? n : 1);
}
*/

int
SendAnnoToSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_kparse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UDP' or `UNIX')", &socktype,
		  cpIgnoreRest,
		  cpEnd) < 0)
    return -1;
  socktype = socktype.upper();


  // remove keyword arguments
  if (cp_va_parse_remove_keywords(conf, 2, this, errh,
		"VERBOSE", cpBool, "be verbose?", &_verbose,
		"SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		"NODELAY", cpUnsigned, "disable Nagle algorithm?", &_nodelay,
		"FRAME", cpBool, "frame packets?", &_frame,
                "ETHERADDRESS",  cpEtherAddress, "ether address", _me,

		cpEnd) < 0)
    return -1;

  if (!_me)
    return errh->error("EtherAddress not specified");

  if (socktype == "TCP" || socktype == "UDP") {
    _family = PF_INET;
    click_chatter(" sockettype: %s \n", socktype.c_str());
    _socktype = socktype == "TCP" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = socktype == "TCP" ? IPPROTO_TCP : IPPROTO_UDP;
    if (cp_va_kparse(conf, this, errh,
		    cpIgnore,
		    cpIPAddress, "IP address", &_ip,
		    cpUnsignedShort, "port number", &_port,
		    cpEnd) < 0)
      return -1;
  }

  else if (socktype == "UNIX") {
    _family = PF_UNIX;
    _socktype = SOCK_STREAM;
    _protocol = 0;
    if (cp_va_kparse(conf, this, errh,
		    cpIgnore,
		    cpString, "filename", &_pathname,
		    cpEnd) < 0)
      return -1;
    if (_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
      return errh->error("filename too long");
  }

  else
    return errh->error("unknown socket type `%s'", socktype.c_str());

  return 0;
}


int
SendAnnoToSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
SendAnnoToSocket::initialize(ErrorHandler *errh)
{
  // open socket, set options, bind to address
  _fd = socket(_family, _socktype, _protocol);
  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  if (_family == PF_INET) {
    sa.in.sin_family = _family;
    sa.in.sin_port = htons(_port);
    sa.in.sin_addr = _ip.in_addr();
    sa_len = sizeof(sa.in);
  }
  else {
    sa.un.sun_family = _family;
    strcpy(sa.un.sun_path, _pathname.c_str());
    sa_len = sizeof(sa.un.sun_family) + _pathname.length();
  }

#ifdef SO_SNDBUF
  // set transmit buffer size
  if (setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, &_snaplen, sizeof(_snaplen)) < 0)
    return initialize_socket_error(errh, "setsockopt");
#endif

#ifdef TCP_NODELAY
  // disable Nagle algorithm
  if (_protocol == IPPROTO_TCP && _nodelay)
    if (setsockopt(_fd, IP_PROTO_TCP, TCP_NODELAY, &_nodelay, sizeof(_nodelay)) < 0)
      return initialize_socket_error(errh, "setsockopt");
#endif

  if (_protocol == IPPROTO_TCP && !_ip) {
    // bind to port
    if (bind(_fd, (struct sockaddr *)&sa, sa_len) < 0)
      return initialize_socket_error(errh, "bind");

    // start listening
    if (_socktype == SOCK_STREAM)
      if (listen(_fd, 0) < 0)
	return initialize_socket_error(errh, "listen");

    add_select(_fd, SELECT_READ);
  }
  else {
    // connect
    if (_socktype == SOCK_STREAM)
      if (connect(_fd, (struct sockaddr *)&sa, sa_len) < 0)
	return initialize_socket_error(errh, "connect");

    _active = _fd;
  }

  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }
  return 0;
}

void
SendAnnoToSocket::cleanup(CleanupStage)
{
  if (_active >= 0 && _active != _fd) {
    close(_active);
    _active = -1;
  }
  if (_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_fd, SHUT_RDWR);
#else
    shutdown(_fd, 2);
#endif
    close(_fd);
    _fd = -1;
  }
}

void
SendAnnoToSocket::selected(int fd)
{
  int new_fd = accept(fd, (struct sockaddr *)&sa, &sa_len);

  if (new_fd < 0) {
    if (errno != EAGAIN)
      click_chatter("%s: accept: %s", declaration().c_str(), strerror(errno));
    return;
  }

  if (_verbose) {
    if (_family == PF_INET)
      click_chatter("%s: opened connection %d from %s.%d", declaration().c_str(), new_fd, IPAddress(sa.in.sin_addr).unparse().c_str(), ntohs(sa.in.sin_port));
    else
      click_chatter("%s: opened connection %d", declaration().c_str(), new_fd);
  }

  _active = new_fd;
}

void
SendAnnoToSocket::handle_ip(WritablePacket *p, String &msg)
{
  const click_ip *iph = p->ip_header();
  p->pull(sizeof(click_ip));
  
  /// @TODO iph is sometimes null, which causes a sigsegv
  if( NULL == iph )
    return;

  IPAddress src(iph->ip_src.s_addr);
  IPAddress dst(iph->ip_dst.s_addr);
  //int ip_len = ntohs(iph->ip_len);
  //int payload_len = ip_len - (iph->ip_hl << 2);



  /*
  if (_print_id)
    sa << "id " << ntohs(iph->ip_id) << ' ';
  if (_print_ttl)
    sa << "ttl " << (int)iph->ip_ttl << ' ';
  if (_print_tos)
    sa << "tos " << (int)iph->ip_tos << ' ';
  if (_print_len)
    sa << "length " << ip_len << ' ';

  */

  if (IP_FIRSTFRAG(iph)) {
    switch (iph->ip_p) {
    
     case IP_PROTO_TCP: {
       const click_tcp *tcph = p->tcp_header();
       unsigned short srcp = ntohs(tcph->th_sport);
       unsigned short dstp = ntohs(tcph->th_dport);
       //unsigned seq = ntohl(tcph->th_seq);
       //unsigned ack = ntohl(tcph->th_ack);
       //unsigned win = ntohs(tcph->th_win);
       //unsigned seqlen = payload_len - (tcph->th_off << 2);
       //int ackp = tcph->th_flags & TH_ACK;

       msg += "tcp" + src.s() + ":" + String(srcp) + ">" + dst.s() + ":" + String(dstp) + ",";
       /*
       if (tcph->th_flags & TH_SYN)
	 sa << 'S', seqlen++;
       if (tcph->th_flags & TH_FIN)
	 sa << 'F', seqlen++;
       if (tcph->th_flags & TH_RST)
	 sa << 'R';
       if (tcph->th_flags & TH_PUSH)
	 sa << 'P';
       if (!(tcph->th_flags & (TH_SYN | TH_FIN | TH_RST | TH_PUSH)))
	 sa << '.';
       */
    
       /*
       sa << ' ' << seq << ':' << (seq + seqlen)
	  << '(' << seqlen << ',' << p->length() << ',' << ip_len << ')';
       if (ackp)
	 sa << " ack " << ack;
       sa << " win " << win;
       break;
       */
     }
  
     case IP_PROTO_UDP: {
       const click_udp *udph = p->udp_header();
       unsigned short srcp = ntohs(udph->uh_sport);
       unsigned short dstp = ntohs(udph->uh_dport);
       //unsigned len = ntohs(udph->uh_ulen);
       msg += "udp" + src.s() + ":" + String(srcp) + ">" + dst.s() + ":" + String(dstp) + ",";
       break;
     }

#define swapit(x) (_swap ? ((((x) & 0xff) << 8) | ((x) >> 8)) : (x))
  
     case IP_PROTO_ICMP: {
       msg += "icmp" + src.s() + ">" + dst.s() + ",";
       //sa << src << " > " << dst << ": icmp";
       const click_icmp *icmph = p->icmp_header();
       if (icmph->icmp_type == ICMP_ECHOREPLY) {
	 //const click_icmp_sequenced *seqh = reinterpret_cast<const click_icmp_sequenced *>(icmph);
	 //sa << ": echo reply (" << swapit(seqh->icmp_identifier) << ", " << swapit(seqh->icmp_sequence) << ")";
	 //msg += "echo_reply(" + swapit(seqh->icmp_identifier) << "," << swapit(seqh->icmp_sequence) + ")";
       } else if (icmph->icmp_type == ICMP_ECHO) {
	 //const click_icmp_sequenced *seqh = reinterpret_cast<const click_icmp_sequenced *>(icmph);
	 //msg += "echo_request(" + swapit(seqh->icmp_identifier) + "," << swapit(seqh->icmp_sequence) + ")";
       } else
	 msg += "type" + (int)icmph->icmp_type;
       break;
     }
  
     default: {
       msg += src.s() + ">" + dst.s() + ":ip-proto-" + String((int)iph->ip_p);
       break;
     }
  
    }
    
  } else {			// not first fragment

    msg += src.s() + ">" + dst.s() + ":";
    switch (iph->ip_p) {
     case IP_PROTO_TCP:		msg += "tcp"; break;
     case IP_PROTO_UDP:		msg += "udp"; break;
     case IP_PROTO_ICMP:	msg += "icmp"; break;
     default:			msg += "ip-proto-" + String((int)iph->ip_p); break;
    }

  }

  // print fragment info
  //if (IP_ISFRAG(iph))
    //msg += "(frag" + ntohs(iph->ip_id) + ':' + payload_len + '@'
       //+ ((ntohs(iph->ip_off) & IP_OFFMASK) << 3)
       //+ ((iph->ip_off & htons(IP_MF)) ? "+" : "") + ')';

  // print payload
  /*
  if (_contents > 0) {
    int amt = 3*_bytes + (_bytes/4+1) + 3*(_bytes/24+1) + 1;

    char *buf = sa.reserve(amt);
    char *orig_buf = buf;

    const uint8_t *data;
    if (_payload) {
      if (IP_FIRSTFRAG(iph) && iph->ip_p == IP_PROTO_TCP)
	data = p->transport_header() + (p->tcp_header()->th_off << 2);
      else if (IP_FIRSTFRAG(iph) && iph->ip_p == IP_PROTO_UDP)
	data = p->transport_header() + sizeof(click_udp);
      else
	data = p->transport_header();
    } else
      data = p->data();

    if (buf && _contents == 1) {
      for (unsigned i = 0; i < _bytes && data < p->end_data(); i++, data++) {
	if ((i % 24) == 0) {
	  *buf++ = '\n'; *buf++ = ' '; *buf++ = ' ';
	} else if ((i % 4) == 0)
	  *buf++ = ' ';
	sprintf(buf, "%02x", *data & 0xff);
	buf += 2;
      }
    } else if (buf && _contents == 2) {
      for (unsigned i = 0; i < _bytes && data < p->end_data(); i++, data++) {
	if ((i % 48) == 0) {
	  *buf++ = '\n'; *buf++ = ' '; *buf++ = ' ';
	} else if ((i % 8) == 0)
	  *buf++ = ' ';
	if (*data < 32 || *data > 126)
	  *buf++ = '.';
	else
	  *buf++ = *data;
      }
    }

    if (orig_buf) {
      assert(buf <= orig_buf + amt);
      sa.forward(buf - orig_buf);
    }
  }
  */

  /*
#if CLICK_USERLEVEL
  if (_outfile) {
    sa << '\n';
    fwrite(sa.data(), 1, sa.length(), _outfile);
  } else
#endif
  {
    _errh->message("%s", sa.c_str());
  }

  return p;
  */
}

void
SendAnnoToSocket::handle_arp(WritablePacket *, String &)
{

}

void
SendAnnoToSocket::handle_brn_dsr(WritablePacket *p, String &msg)
{
    const click_brn_dsr *brn_dsr = (const click_brn_dsr *) p->data();
    p->pull(sizeof(click_brn_dsr));

    switch (brn_dsr->dsr_type)
    {
        case BRN_DSR_RREQ:
            msg += "RREQ,";
            break;
        case BRN_DSR_RREP:
            msg += "RREP,";
            break;
        case BRN_DSR_RERR:
            {
                EtherAddress dsr_unreachable_src(brn_dsr->body.rerr.dsr_unreachable_src.data);
                EtherAddress dsr_unreachable_dst(brn_dsr->body.rerr.dsr_unreachable_dst.data);
                EtherAddress dsr_msg_originator(brn_dsr->body.rerr.dsr_msg_originator.data);
                msg = msg + "RERR(" + "un_rea_src" + dsr_unreachable_src.s() + "un_rea_dst" + dsr_unreachable_dst.s() + "origin" + dsr_msg_originator.s() + ")";
            break;
            }
        case BRN_DSR_SRC:
            msg += "SRC";
/*
            switch (brn_dsr->body.src.dsr_payload_type)
            {
                //case BRN_DSR_PAYLOADTYPE_RAW:
                    //msg += "_with_raw";
                    //break;
                case BRN_DSR_PAYLOADTYPE_UDT:
                    msg += "_with_udt,";
                    handle_brn_dsr_udt( p, msg );
                    break;
                case BRN_DSR_PAYLOADTYPE_DHT:
                    msg += "_with_dht,";
                    break;
                default:
                    msg += "_with_raw,";
                    break;
            }
*/
            break;
        default:
            msg += String((uint16_t) brn_dsr->dsr_type);
    }

    EtherAddress dst(brn_dsr->dsr_dst.data);
    msg += "dst_" + dst.s() + ",";
    EtherAddress src(brn_dsr->dsr_src.data);
    msg += "src_" + src.s() + ",";

    msg += "sl_" + String((uint16_t) brn_dsr->dsr_segsleft) + ",";
    msg += "hops_" + String((uint16_t) brn_dsr->dsr_hop_count) + "(";

    for (int i = 0; i < (uint16_t) brn_dsr->dsr_hop_count; i++) 
    {
        EtherAddress hop(brn_dsr->addr[i].hw.data);
        msg += hop.s() + ",";
    }
    msg += "),";
}

void
SendAnnoToSocket::handle_brn_dsr_udt(WritablePacket *p, String &msg)
{
    const click_brn_dsr_udt *pHeader = 
      reinterpret_cast<const click_brn_dsr_udt*>(p->data());
    if( NULL == pHeader )
        return;

    const void* pData = 
      static_cast<const void*>(pHeader + 1);
    int nDataLen = p->length() -  UDT_HEADER_SIZE;
    p->pull(sizeof(click_brn_dsr_udt));
    int i;
    
    // Convert to host byte order
    uint32_t packed_hdr = ntohl(pHeader->packed_hdr);
    uint32_t timestamp  = ntohl(pHeader->timestamp);
    
    // Distinction control/data packet
    if( 1 == packed_hdr >> 31 ) // Control packet
    {
        PacketType type = static_cast<PacketType>((packed_hdr >> 28) & 0x00000007);
      
        msg += " seqno_" + String(packed_hdr & 0x7FFFFFFF) 
          + " ts_" + String(timestamp);
  
        int nLen = nDataLen / sizeof(int32_t);
        int32_t* pCData = NULL;
        if( NULL != pData && 0 < nLen )
            pCData = new int32_t[nLen];
        for( i = 0; i < nLen; ++i )
            pCData[i] = ntohl( static_cast<const int32_t*>(pData)[i] );
  
        switch( type )
        {
            case PT_HANDSHAKE: ///< Handshake
                msg += " handshake ver_" + String(pCData[0]) + " mss_" + 
                String(pCData[2]) + " ffs_" +  String(pCData[3]) + " isn_" 
                + String(pCData[1]);
                break;
            case PT_KEEPALIVE: ///< Keep alive
                msg += " keepalive";
                break;
            case PT_ACK: ///< Acknowledgement
                msg += " ack ackseqno_" + String(pCData[0]);
                if( 2*sizeof(int32_t) != nDataLen )
                {
                    msg += " rtt_" + String(pCData[1]) + " rttvar_" + 
                      String(pCData[2]) + " flowwindow_" + String(pCData[3]) 
                      + " bandwidth_" + String(pCData[4]);
                }
                break;
            case PT_NAK: ///< Negative Acknowledgement
                msg += " nak";
                for( i = 0; i < nLen; ++i )
                {
                    msg += "seqno_" + String(pCData[i] & 0x7FFFFFFF);
                    if( 0 != (pCData[i] & 0x80000000) )
                    {
                        msg += "~" + String(pCData[i+1]); ++i;
                    }
                }
                break;
            case PT_RESERVED: ///< Reserved for congestion control
                msg += " reserved";
                break;
            case PT_SHUTDOWN: ///< Shutdown
                msg += " shutdown";
                break;
            case PT_ACK2: ///< Acknowledgement of Acknowledgement
                msg += " ack2 ackseqno_" + String(packed_hdr & 0x0000FFFF);
                break;
            case PT_FFU: ///< For future use
                msg += " ffu";
                break;
            default: 
                msg += " type_unknown";
                break;
        }
        // ...
  
        if( NULL != pCData )        
            delete[]( pCData );
    }
    else // Data packet
    {
        msg += " seqno_" + String(packed_hdr & 0x7FFFFFFF) 
          + " ts_" + String(timestamp) + " data len_" + 
          String(p->length());
    }
}

void
SendAnnoToSocket::handle_brn(WritablePacket *p, String &msg)
{
    const click_brn *brn = (const click_brn *) p->data();
    p->pull(sizeof(click_brn));

    switch (brn->dst_port)
    {
        case BRN_PORT_LINK_PROBE:
            msg += "ETX";
            break;
        case BRN_PORT_SDP:
            msg += "SDP";
            break;
        case BRN_PORT_TFTP:
            msg += "TFTP";
            break;
        case BRN_PORT_DSR:
            msg += "DSR,";
            handle_brn_dsr(p, msg);
            break;
        default:
            msg += String((uint16_t) brn->dst_port);
    }
}

void
SendAnnoToSocket::handle_wifi(WritablePacket *p, String &msg)
{
  const struct click_wifi *w = (const struct click_wifi *) p->data();
  p->pull(sizeof(click_wifi));


  EtherAddress bssid;
  EtherAddress src;
  EtherAddress dst;

  switch (w->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    msg = msg + _me->s() + " " + src.s() + " NODS";
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    dst = EtherAddress(w->i_addr3);
    msg = msg + _me->s() + " " + src.s() + " TODS";
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(w->i_addr1);
    bssid = EtherAddress(w->i_addr2);
    src = EtherAddress(w->i_addr3);
    msg = msg + _me->s() + " " + bssid.s() + " FROMDS";
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    msg = msg + _me->s() + " " + src.s() + " DSTODS";
    break;
  default:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
  }


  switch (w->i_fc[0] & WIFI_FC0_TYPE_MASK)
  {
      case WIFI_FC0_TYPE_MGT:
        msg += "(mgt)_";
        break;
      case WIFI_FC0_TYPE_CTL:
        msg += "(ctl)_";
        break;
      case WIFI_FC0_TYPE_DATA:
      {
        msg += "(data)_";
        const struct click_llc *llc = (const struct click_llc *) p->data();
        p->pull(sizeof(click_llc));

        uint16_t ether_type;
        ether_type = llc->llc_un.type_snap.ether_type;

        switch (htons(ether_type))
        {
            case ETHERTYPE_IP:
                msg += "IP,";
                handle_ip(p, msg);
                break;
            case ETHERTYPE_ARP:
                msg += "ARP,";
                handle_arp(p, msg);
                break;
            case ETHERTYPE_BRN:
                msg += "BRN,";
                handle_brn(p, msg);
                break;
            default:
                msg += String((uint16_t) htons(ether_type)) + "?";
                break;
        }
        break;
    }
    default:
        msg += "(" + String((uint16_t) w->i_fc[0] & WIFI_FC0_TYPE_MASK) + "?)_";
  }
}

void
SendAnnoToSocket::send_packet(Packet *p)
{
  //bool _strict = false;

/*
  int w = 0;
  if (_frame) {
    p->push(sizeof(uint32_t));
    *(uint32_t *)p->data() = htonl(p->length());
  }
*/
  //Wifi

  WritablePacket *p_out = (WritablePacket *) p->clone();

  String msg;
  handle_wifi(p_out, msg);
  msg += "\n";

/*
  int packet_type = p->user_anno_c(PACKET_TYPE_KEY_USER_ANNO);
  String msg;
  switch (packet_type)
  {
    case BRN_DSR_RREQ: msg = "RREQ"; break;
    case BRN_DSR_RREP: msg = "RREP"; break;
    case BRN_DSR_RERR: msg = "RERR"; break;
    case BRN_DSR_SRC: msg = "SRC"; break;
    default: msg = "UNKNOWN"; 
             if (EtherAddress(ether->ether_dhost) == EtherAddress((const unsigned char *) "\xff\xff\xff\xff\xff\xff"))
                msg = "BROADCAST";
  }
*/
  p_out->kill();
  click_chatter("SendAnnoToSocket: sendto: %s \n", msg.c_str());

  while (msg.length() != 0)
  {
    // send n bytes
    int n = sendto(_active, msg.c_str(), msg.length(), 0, (const struct sockaddr*)&sa, sa_len);

    // if error break
    if (n < 0 && errno != EINTR)
    {
      break;
    }

    // remove sent bytes from string
    msg = msg.substring(n);
  }
  /* jkm */

  /*
  while (p->length()) {
    w = sendto(_active, p->data(), p->length(), 0, (const struct sockaddr*)&sa, sa_len);
    if (w < 0 && errno != EINTR)
      break;
    p->pull(w);
  }
  */

  /*
  if (w < 0) {
    click_chatter("SendAnnoToSocket: sendto: %s", strerror(errno));
    checked_output_push(0, p);
  }
  else
    p->kill();
  */

}

void
SendAnnoToSocket::push(int, Packet *p)
{
  send_packet(p);
  output(0).push(p);
}

/*
bool
SendAnnoToSocket::run_task()
{
  // XXX reduce tickets when idle
  Packet *p = input(0).pull();
  if (p)
    send_packet(p);
  else if (!_signal)
    return false;
  _task.fast_reschedule();
  return p != 0;
}
*/

void
SendAnnoToSocket::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SendAnnoToSocket)
