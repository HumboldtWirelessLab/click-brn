#ifndef PACKETDEFRAGMENTATIONELEMENT_HH
#define PACKETDEFRAGMENTATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"

#include "packetfragmentation.hh"


CLICK_DECLS

#define FRAGMENTATION_EXTRA_SIZE 128

class PacketDefragmentation : public BRNElement
{
  class FragmentationInfo {
   public:
    Timestamp last_frag_time;
    WritablePacket *p;

    uint16_t packet_id;
    uint8_t no_frag;

    uint32_t recv_frag;

    uint16_t frag_size;

    FragmentationInfo(): p(NULL),packet_id(0),no_frag(0),recv_frag(0),frag_size(0) {
    }

    void insert_packet(Packet *frag) {
      struct fragmention_header *fh = (struct fragmention_header*)frag->data();
      packet_id = ntohs(fh->packet_id);
      uint8_t frag_id;

      if ( (recv_frag & (1<<fh->fragment_id)) != 0 ) {
        frag->kill();
        return;
      }

      last_frag_time = Timestamp::now();

      frag_id = fh->fragment_id;

      click_chatter("PacketID: %d FragID: %d no_Frag: %d",packet_id,frag_id,fh->no_fragments);

      if ( p == NULL ) {               // first packet at all
        no_frag = fh->no_fragments;

        click_chatter("First packet");

        frag->pull(sizeof(struct fragmention_header));

        if ( (frag_id+1) < no_frag ) { // not the last frag so calc frag_size
          click_chatter("Not the last so store stuff");

          frag_size = frag->length();
          p = frag->push((fh->fragment_id * frag_size) + FRAGMENTATION_EXTRA_SIZE);
          p->pull(FRAGMENTATION_EXTRA_SIZE);
          p = p->put((no_frag - frag_id - 1) * frag_size);

        } else {
          click_chatter("The last so store the packet");
          p = frag->uniqueify();
          frag_size = 0;
        }

      } else {                        // Not the first packet
        if ( frag_size == 0 ) {       // known packet is the last packet
          frag_size = frag->length() - sizeof(struct fragmention_header);

          WritablePacket *p_old = p;

          click_chatter("We already have the last packet. Set frag_size. %d",frag_size);

          if ( frag_id > 0 ) {
            click_chatter("Space at the beginning of the frame: %d",(frag_id * frag_size));
            p = frag->push((frag_id * frag_size) + (FRAGMENTATION_EXTRA_SIZE - sizeof(struct fragmention_header)));
            p->pull(FRAGMENTATION_EXTRA_SIZE);
          }

          click_chatter("Space at the end of the frame: %d", ((no_frag - frag_id - 2) * frag_size) + p_old->length());
          p = p->put(((no_frag - frag_id - 2) * frag_size) + p_old->length() );

          click_chatter("PacketSize is %d",p->length());
          memcpy(&(p->data()[frag_size*(no_frag-1)]), p_old->data(), p_old->length());
          p_old->kill();
        } else {                      // we already have packets
          click_chatter("We already have packet. Just add: %d %d", ((uint32_t)frag_size* (uint32_t)frag_id), (frag->length()-sizeof(struct fragmention_header)));
          memcpy(&(p->data()[((uint32_t)frag_size * (uint32_t)frag_id)]), &(frag->data()[sizeof(struct fragmention_header)]),
                                                     frag->length()-sizeof(struct fragmention_header));

          if ( (frag_id+1) == no_frag ) { //got the last one: shorten p cause by extending it first we
                                          //asume the the last frag has the same size like the other frags
            click_chatter("Fix length for last packet");
            p->take(frag_size - (frag->length()-sizeof(struct fragmention_header)));
          }
          frag->kill();
        }
      }

      recv_frag |= 1 << frag_id;
    }

    bool is_complete() {
      click_chatter("Complete ??: %d %d",no_frag,recv_frag);
      if ( no_frag == 32 ) return (~recv_frag == 0);
      return ( ((uint32_t)(1<<no_frag)-1) == recv_frag );
    }

    void setTimeNow() {
      last_frag_time = Timestamp::now();
    }

    void clear() {
      if ( p != NULL ) {
        p->kill();
        p = NULL;
      }
      packet_id = 0;
      no_frag = 0;
      recv_frag = 0;
      frag_size = 0;
    }

  };

#define FRAGMENTATION_LIST_SIZE 30

  class SrcInfo {
    EtherAddress _src;

    FragmentationInfo fi_list[FRAGMENTATION_LIST_SIZE];  //TODO: use Vector or Hashmap

   public:

    SrcInfo() {
      for ( int i = 0; i < FRAGMENTATION_LIST_SIZE; i++ )  {
        fi_list[i].p = NULL;
      }
    }

    explicit SrcInfo(EtherAddress *src) {
      for ( int i = 0; i < FRAGMENTATION_LIST_SIZE; i++ )  {
        fi_list[i].p = NULL;
      }

      _src = *src;
    }

    FragmentationInfo *getFragmentionInfo(uint16_t packet_id) {
      for ( int i = 0; i < FRAGMENTATION_LIST_SIZE; i++ )  {
        if ( (fi_list[i].p) != NULL && (fi_list[i].packet_id == packet_id) ) return &(fi_list[i]);
      }

      return NULL;
    }

    FragmentationInfo *getFreeFragmentionInfo() {
      for ( int i = 0; i < FRAGMENTATION_LIST_SIZE; i++ ) {
        if ( fi_list[i].p == NULL ) return &(fi_list[i]);
      }
      return NULL;
    }

    FragmentationInfo *getLRUFragmentionInfo(bool clear) {
      int lru_i = 0;

      if ( fi_list[0].p == NULL ) return &(fi_list[0]);

      for ( int i = 1; i < FRAGMENTATION_LIST_SIZE; i++ ) {
        if ( fi_list[i].p == NULL ) return &(fi_list[i]);
        else {
          if ( fi_list[i].last_frag_time < fi_list[lru_i].last_frag_time) {
            lru_i = i;
          }
        }
      }
      if ( clear ) fi_list[lru_i].clear();

      return &(fi_list[lru_i]);
    }

    void delFragmentionInfo(uint16_t packet_id) {
      for ( int i = 0; i < FRAGMENTATION_LIST_SIZE; i++ )  {
        if ( fi_list[i].packet_id == packet_id ) {
          fi_list[i].p = NULL;
          return;
        }
      }
    }

  };

  typedef HashMap<EtherAddress, SrcInfo> SrcInfoTable;
  typedef SrcInfoTable::const_iterator SrcInfoTableIter;

 public:
  //
  //methods
  //
  PacketDefragmentation();
  ~PacketDefragmentation();

  const char *class_name() const  { return "PacketDefragmentation"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

   SrcInfoTable _src_tab;

};

CLICK_ENDDECLS
#endif
