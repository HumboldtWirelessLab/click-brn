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

/*
 * seismo.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_LIBFFTW3
#include <fftw3.h>
#endif


#include <click/userutils.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/standard/fft/ifft.hh"
#include "seismo_reporting.hh"
#include "seismodetection_ifft.hh"

CLICK_DECLS

SeismoDetectionIFFT::SeismoDetectionIFFT():
  _current_alarm(NULL),
  _next_id(0),
  _ifft_re_block(NULL),
  _ifft_im_block(NULL),
  _ifft_re_block_cpy(NULL),
  _ifft_im_block_cpy(NULL),
  _ifft_block_size(-1),
  _ifft_block_index(0),
  _channel(0),
#ifdef HAVE_LIBFFTW3
  _fft_threshold(10000)
#else
  _fft_threshold(10000)
#endif
{
  BRNElement::init();
}

SeismoDetectionIFFT::~SeismoDetectionIFFT()
{
  delete _ifft_re_block;
  delete _ifft_im_block;

  delete _ifft_re_block_cpy;
  delete _ifft_im_block_cpy;

  delete _ifft_re_out;
}

void *
SeismoDetectionIFFT::cast(const char *n)
{
  if (strcmp(n, "SeismoDetectionIFFT") == 0)
    return (SeismoDetectionIFFT *) this;
  else if (strcmp(n, "SeismoDetectionAlgorithm") == 0)
    return (SeismoDetectionAlgorithm *) this;
  else
    return 0;
}

int
SeismoDetectionIFFT::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "BLOCK_SIZE", cpkP, cpInteger, &_ifft_block_size,
      "CHANNEL", cpkP, cpInteger, &_channel,
      "RATIOTHRESHOLD", cpkP, cpInteger, &_fft_threshold,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if ( _ifft_block_size <= 0 ) {
    errh->error("Blocksize too small");
  }

  _ifft_max_block_size = 2 * _ifft_block_size;
  //TODO: align blocksize. Valid sizes?

  return 0;
}

int
SeismoDetectionIFFT::initialize(ErrorHandler */*errh*/)
{
  //alloc blocks
  _ifft_re_block = new int32_t[_ifft_max_block_size];
  _ifft_im_block = new int32_t[_ifft_max_block_size];

  _ifft_re_block_cpy = new int32_t[_ifft_block_size];
  _ifft_im_block_cpy = new int32_t[_ifft_block_size];

  _ifft_re_out = new int32_t[_ifft_block_size];

  //clear blocks
  memset(_ifft_re_block, 0, _ifft_max_block_size*sizeof(int32_t));
  memset(_ifft_im_block, 0, _ifft_max_block_size*sizeof(int32_t));

  return 0;
}

void
SeismoDetectionIFFT::update(SrcInfo *si, uint32_t next_new_block)
{
  BRN_DEBUG("update");
  SeismoInfoBlock* sib = NULL;

 sib = si->get_block(next_new_block);

  //only handle complete block
  if ( ! sib->is_complete() ) return;

  BRN_INFO("SampleCount: %d BlockSize: %d",sib->size(),_ifft_max_block_size);

  for( uint32_t s = 0; s < sib->size(); s++ ) {

    int32_t c_mean_diff = (int32_t)((sib->_channel_values[s][_channel]) - (int32_t)(sib->_channel_mean[_channel]));
    BRN_INFO("V: %d M: %d D: %d", sib->_channel_values[s][_channel], (int32_t)sib->_channel_mean[_channel], c_mean_diff);

    //copy value of wanted channel
    _ifft_re_block[_ifft_block_index] = c_mean_diff;
    //_ifft_im_block[_ifft_block_index] = sib->_channel_values[s][_channel];
    _ifft_block_index++;

    if ( _ifft_block_index == _ifft_max_block_size ) {
      //reach end of block, so ...
      //..move block to get more space (copy a block of _ifft_block_size to the beginnig)
      memcpy(&(_ifft_re_block[0]), &(_ifft_re_block[_ifft_max_block_size-_ifft_block_size]),
               _ifft_block_size * sizeof(int32_t));

      //set index to the end of the moved block
      _ifft_block_index = _ifft_block_size;
    }
  }


  if ( (_ifft_block_index >= _ifft_block_size) && ( (_ifft_block_index-_ifft_block_size)%_ifft_block_size == 0)) { //enough data for ifft
    BRN_INFO("Enough data. Start copy....");
    memcpy(_ifft_re_block_cpy, &(_ifft_re_block[_ifft_block_index-_ifft_block_size]),
           _ifft_block_size * sizeof(int32_t));
    memcpy(_ifft_im_block_cpy, &(_ifft_im_block[_ifft_block_index-_ifft_block_size]),
           _ifft_block_size * sizeof(int32_t));

    BRN_INFO("Start FFT");
#ifdef HAVE_LIBFFTW3
    fft_libfftw3(_ifft_re_block_cpy, _ifft_re_out, _ifft_block_size);
    detect_alarm( _ifft_re_out, _ifft_block_size/2); //half due to freq/2 (see fft_libfftw3)
#else
    IFFT::ifft(_ifft_re_block_cpy, _ifft_im_block_cpy, _ifft_block_size);
    detect_alarm( _ifft_re_block_cpy, _ifft_block_size/2); //half due to freq/2 (see fft_libfftw3)
#endif

    BRN_INFO("Finished");
  }
}

int
SeismoDetectionIFFT::detect_alarm( int32_t *in_data, uint16_t n) {
  int32_t min_v = INT32_MIN;
  int32_t max_v = INT32_MAX;
  int32_t val = 0;

  for ( int32_t i = 0; i < n; i++ ) {
    val = abs((int)in_data[i]);
    if ( val < min_v ) min_v = val;
    if ( val > max_v ) max_v = val;
  }
  if ( min_v == 0 ) {
    min_v = 1;
  }

  if ( ((uint32_t)abs(max_v/min_v)) > _fft_threshold ) {
    BRN_ERROR("Detect Signal: %d (%d,%d)", (max_v/min_v), max_v, min_v);
    if ( _current_alarm == NULL ) {
      _current_alarm = new SeismoAlarm();
      _current_alarm->_id = _next_id++;
      _sal.push_back(_current_alarm);
    }
  } else {
    if ( _current_alarm != NULL ) {
      _current_alarm->end_alarm();
      _current_alarm = NULL;
    }
  }
  return 0;
}

/************************************************************************************/
/***************************** L O C A L   F F T ************************************/
/************************************************************************************/
#ifdef HAVE_LIBFFTW3
int
SeismoDetectionIFFT::fft_libfftw3(int32_t *in_data, int32_t *out_data, uint16_t n)
{
  int i;
  int nc;

  double *in;

  double *out;
  fftw_plan plan_forward;

  in = (double*)fftw_malloc(sizeof(double) * n );

  for ( i = 0; i < n; i++ ) in[i] = (double)in_data[i];

  BRN_INFO("Input Data:\n");
  for ( i = 0; i < n; i++ ) BRN_INFO( "  %4d  %12f\n", i, in[i] );
/*
  Set up an array to hold the transformed data,
  get a "plan", and execute the plan to transform the IN data to
  the OUT FFT coefficients.
*/
  nc = ( n / 2 ) + 1;

  out = (double*)fftw_malloc(sizeof(double) * n);

  plan_forward = fftw_plan_r2r_1d ( n, in, out, FFTW_R2HC, FFTW_PATIENT );

  fftw_execute ( plan_forward );

  //BRN_INFO( "Output FFT Coefficients:");
  for ( i = 0; i < nc; i++ ) {
    //BRN_INFO("FFTw3  %4d  %12f", i, out[i]);
    out_data[i] = out[i];
  }

/*
  Release the memory associated with the plans.
*/
  fftw_destroy_plan ( plan_forward );

  fftw_free ( in );
  fftw_free ( out );

  return 0;
}
#endif

/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

enum {
  H_STATS
};

String
SeismoDetectionIFFT::stats(void)
{
  Timestamp now = Timestamp::now();
  StringAccum sa;

  sa << "<seismodetection_ifft id=\"" << BRN_NODE_NAME << "\" time=\"";
  sa << now.unparse() << "\" size=\"" << _sal.size() << "\" >\n";

  for ( int32_t i = 0; i < _sal.size(); i++ ) {
    sa << "\t<alarm start=\"" << _sal[i]->_start.unparse() << "\" end=\"" << _sal[i]->_end.unparse() << "\" />\n";
  }

  sa << "</seismodetection_ifft>\n";

  return sa.take_string();
}

static String
read_handler(Element *e, void */*thunk*/)
{
  SeismoDetectionIFFT *sdc = (SeismoDetectionIFFT*)e;
  return sdc->stats();
}

void
SeismoDetectionIFFT::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_handler, H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoDetectionIFFT)
