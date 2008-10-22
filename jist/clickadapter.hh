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

#ifndef brn_click_ClickAdapter_H
#define brn_click_ClickAdapter_H
#include <jni.h>
#include <java_base_class.hh>

namespace java {
namespace lang {
class String;
}
}

namespace brn {
namespace click {
class SimPacketInfo;
}
}
class JavaByteArray;
class JavaBooleanArray;
class JavaCharArray;
class JavaIntArray;
class JavaShortArray;
class JavaLongArray;
class JavaDoubleArray;
class JavaFloatArray;
class JavaObjectArray;

namespace brn {
namespace click {
  class ClickAdapter : public JavaBaseClass {
  public:
    ClickAdapter(jobject obj);

    int sim_ifid_from_name(const char* arg1); // public int brn.click.ClickAdapter.sim_ifid_from_name(java.lang.String)
    const char* sim_ipaddr_from_name(const char* arg1); // public java.lang.String brn.click.ClickAdapter.sim_ipaddr_from_name(java.lang.String)
    const char* sim_macaddr_from_name(const char* arg1); // public java.lang.String brn.click.ClickAdapter.sim_macaddr_from_name(java.lang.String)
    int sim_send_to_if(int arg1, int arg2, int arg3, JavaByteArray* arg4, int id, int fid, int simtype); // public int brn.click.ClickAdapter.sim_send_to_if(int,int,int,byte[],brn.click.SimPacketInfo)
    int sim_schedule(int arg1, jlong arg2); // public int brn.click.ClickAdapter.sim_schedule(int,long)
    const char* sim_get_node_name(); // public java.lang.String brn.click.ClickAdapter.sim_get_node_name()
    //int sim_if_ready(int arg1, int arg2); // public int brn.click.ClickAdapter.sim_if_ready(int,int)
    int sim_trace(int arg1, const char* arg2); // public int brn.click.ClickAdapter.sim_trace(int,java.lang.String)
    int sim_get_node_id(int arg1); // public int brn.click.ClickAdapter.sim_get_node_id(int)
    int sim_get_next_pkt_id(int arg1); // public int brn.click.ClickAdapter.sim_get_next_pkt_id(int)

  private:
    //static jclass cls;
    static jmethodID* vtable;

    void init_vtable();
};
}
}
#endif
