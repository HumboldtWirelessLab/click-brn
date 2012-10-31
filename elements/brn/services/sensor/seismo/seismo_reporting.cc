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
 * seismo_reporting.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"

CLICK_DECLS

SeismoReporting::SeismoReporting():
  _reporting_timer(this),
  _next_block_id(0),
  _interval(SEISMO_REPORT_DEFAULT_INTERVAL)
{
  BRNElement::init();
}

SeismoReporting::~SeismoReporting()
{
}

int
SeismoReporting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SEISMO", cpkP+cpkM, cpElement, &_seismo,
      "ALGORITHMS", cpkP+cpkM, cpString, &_algo_string,
      "INTERVAL", cpkP, cpInteger, &_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
SeismoReporting::initialize(ErrorHandler *errh)
{
  Vector<String> algo_vec;
  cp_spacevec(_algo_string, algo_vec);

  for (int i = 0; i < algo_vec.size(); i++) {
    Element *new_element = cp_element(algo_vec[i] , this, errh, NULL);
    if ( new_element != NULL ) {
      //click_chatter("El-Name: %s", new_element->class_name());
      SeismoDetectionAlgorithm *sda =
        (SeismoDetectionAlgorithm *)new_element->cast("SeismoDetectionAlgorithm");
      if ( sda != NULL ) {
        _sdal.push_back(sda);
      }
    }
  }

  _reporting_timer.initialize(this);
  _reporting_timer.schedule_after_msec(_interval);

  return 0;
}

void
SeismoReporting::run_timer(Timer *)
{
  BRN_DEBUG("Run timer");

  _reporting_timer.schedule_after_msec(_interval);

  seismo_evaluation();

}

void
SeismoReporting::seismo_evaluation()
{
  SrcInfo *local_si = NULL;
  SeismoInfoBlock *sib = NULL;

  if ( (local_si = _seismo->_local_info) != NULL ) {
    BRN_DEBUG("local info");
    if ( local_si->get_block(_next_block_id) != NULL ) {
       BRN_DEBUG("got block");
      if ( local_si->get_block(_next_block_id)->is_complete() ) {
        BRN_DEBUG("is comp");

        for ( int a = 0; a < _sdal.size(); a++ ) {
          BRN_DEBUG("Update Algo");
          _sdal[a]->update(local_si, _next_block_id);
        }

        if ( (sib = local_si->get_last_block()) != NULL ) {
          if ( sib->is_complete() ) _next_block_id = sib->_block_index+1;
          else _next_block_id = sib->_block_index;
          BRN_DEBUG("Next block ID: %d", _next_block_id);
        } else {
          BRN_DEBUG("no last block");
          _next_block_id = 0;
        }
      }
    }
  }
}

/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

String
SeismoReporting::print_alarm()
{
  StringAccum sa;

  sa << "<seismo_alarm size=\"" << _sal.size() << "\" >\n";

  for ( int32_t i = 0; i < _sal.size(); i++ ) {
    sa << "\t<alarm start=\"" << _sal[i]->_start.unparse() << "\" end=\"" << _sal[i]->_end.unparse() << "\" />\n";
  }

  sa << "</seismo_alarm>\n";

  return sa.take_string();
}

/*static String
read_stats_handler(Element *e, void *thunk)
{
  SeismoReporting *sr = (SeismoReporting*)e;
  return sr->print_stats();
}
*/
static String
read_alarm_handler(Element *e, void */*thunk*/)
{
  SeismoReporting *sr = (SeismoReporting*)e;
  return sr->print_alarm();
}

void
SeismoReporting::add_handlers()
{
  BRNElement::add_handlers();

  //add_read_handler("stats", read_stats_handler);
  add_read_handler("alarm", read_alarm_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoReporting)
