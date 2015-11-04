/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#ifndef SEISMODETECTION_IFFT_ELEMENT_HH
#define SEISMODETECTION_IFFT_ELEMENT_HH
#include <click/element.hh>
#include "elements/brn/brnelement.hh"
#include "elements/brn/brn2.h"

#include "seismo_reporting.hh"

CLICK_DECLS

class SeismoDetectionIFFT : public BRNElement, public SeismoDetectionAlgorithm {

  public:

  SeismoDetectionIFFT();
  ~SeismoDetectionIFFT();

  const char *class_name() const  { return "SeismoDetectionIFFT"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "0/0"; } //0: local 1: remote

  void *cast(const char *);

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();

  //void push(int, Packet *p);

  String stats();

  void update(SrcInfo *si, uint32_t next_new_block);
  SeismoAlarmList *get_alarm() { return &_sal; };

  SeismoAlarmList _sal;
  SeismoAlarm *_current_alarm;
  uint16_t  _next_id;

  int fft_libfftw3(int32_t *in_data, int32_t *out_data, uint16_t n);
  int detect_alarm(int32_t *in_data, uint16_t n);

  int32_t *_ifft_re_block;
  int32_t *_ifft_im_block;

  int32_t *_ifft_re_block_cpy;
  int32_t *_ifft_im_block_cpy;

  int32_t *_ifft_re_out;

  uint32_t _ifft_block_size;
  uint32_t _ifft_max_block_size;
  uint32_t _ifft_block_index;

  uint32_t _channel;
  uint32_t _fft_threshold;
};

CLICK_ENDDECLS
#endif
