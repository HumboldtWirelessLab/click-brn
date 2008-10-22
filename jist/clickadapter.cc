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

#include <string.h>
#include <jni.h>
#include <java_array.hh>
#include <clickadapter.hh>
extern JNIEnv     *javaEnv;

enum 
{
  METHOD_sim_ifid_from_name = 0,
  METHOD_sim_ipaddr_from_name = 1,
  METHOD_sim_macaddr_from_name = 2,
  METHOD_sim_send_to_if = 3,
  METHOD_sim_schedule = 4,
  METHOD_sim_get_node_name = 5,
//  METHOD_sim_if_ready = 6, 
  METHOD_sim_trace = 7, 
  METHOD_sim_get_node_id = 8,
  METHOD_sim_get_next_pkt_id = 9, 

  _METHOD_COUNT = 10
};

namespace brn {
namespace click {

//jclass ClickAdapter::cls = NULL;
jmethodID* ClickAdapter::vtable = NULL;

void ClickAdapter::init_vtable()
{
  if (vtable)
    return;
  vtable = new jmethodID[_METHOD_COUNT];

  jclass cls = javaEnv->FindClass("click/runtime/ClickAdapter");
  handleJavaException();
//  cls = (jclass)javaEnv->NewWeakGlobalRef(cls);
//  handleJavaException();

  vtable[METHOD_sim_ifid_from_name] = javaEnv->GetMethodID(cls, "sim_ifid_from_name", "(Ljava/lang/String;)I");
  handleJavaException();
  vtable[METHOD_sim_ipaddr_from_name] = javaEnv->GetMethodID(cls, "sim_ipaddr_from_name", "(Ljava/lang/String;)Ljava/lang/String;");
  handleJavaException();
  vtable[METHOD_sim_macaddr_from_name] = javaEnv->GetMethodID(cls, "sim_macaddr_from_name", "(Ljava/lang/String;)Ljava/lang/String;");
  handleJavaException();
  vtable[METHOD_sim_send_to_if] = javaEnv->GetMethodID(cls, "sim_send_to_if", "(III[BIII)I");
  handleJavaException();
  vtable[METHOD_sim_schedule] = javaEnv->GetMethodID(cls, "sim_schedule", "(IJ)I");
  handleJavaException();
  vtable[METHOD_sim_get_node_name] = javaEnv->GetMethodID(cls, "sim_get_node_name", "()Ljava/lang/String;");
  handleJavaException();
//  vtable[METHOD_sim_if_ready] = javaEnv->GetMethodID(cls, "sim_if_ready", "(II)I");
//  handleJavaException();
  vtable[METHOD_sim_trace] = javaEnv->GetMethodID(cls, "sim_trace", "(ILjava/lang/String;)I");
  handleJavaException();
  vtable[METHOD_sim_get_node_id] = javaEnv->GetMethodID(cls, "sim_get_node_id", "(I)I");
  handleJavaException();
  vtable[METHOD_sim_get_next_pkt_id] = javaEnv->GetMethodID(cls, "sim_get_next_pkt_id", "(I)I");
  handleJavaException();
}

ClickAdapter::ClickAdapter(jobject obj)
  : JavaBaseClass(obj)
{
  if (NULL == vtable)
    init_vtable();
}

int ClickAdapter::sim_ifid_from_name(const char* arg1)
{
  jstring jarg1;
  if (arg1!=NULL) {
    jarg1 = javaEnv->NewStringUTF(arg1);
  } else {
    jarg1=NULL;
  }
  handleJavaException();
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_ifid_from_name], jarg1);
  handleJavaException();
  if (jarg1!=NULL) {
    javaEnv->DeleteLocalRef(jarg1);
  }
  int result = (int)jresult;
  return result;
}

const char* ClickAdapter::sim_ipaddr_from_name(const char* arg1)
{
  jstring jarg1;
  if (arg1!=NULL) {
    jarg1 = javaEnv->NewStringUTF(arg1);
  } else {
    jarg1=NULL;
  }
  handleJavaException();
  jobject jresult=javaEnv->CallObjectMethod(this->getJavaObject(), vtable[METHOD_sim_ipaddr_from_name], jarg1);
  handleJavaException();
  if (jarg1!=NULL) {
    javaEnv->DeleteLocalRef(jarg1);
  }
  char* result;
  if (jresult!=NULL) {
    const char*  jresult_bytes = javaEnv->GetStringUTFChars((jstring)jresult,NULL);
    handleJavaException();
    jsize        jresult_size = javaEnv->GetStringUTFLength((jstring)jresult);
    handleJavaException();
                 result = new char[jresult_size+1];
    for (int i=0;i<jresult_size;i++) {
      result[i] = jresult_bytes[i];
    }
    result[jresult_size]=0;
    javaEnv->ReleaseStringUTFChars((jstring)jresult, jresult_bytes);
    handleJavaException();
    javaEnv->DeleteLocalRef(jresult);
  } else {
    result=NULL;
  }
  return result;
}

const char* ClickAdapter::sim_macaddr_from_name(const char* arg1)
{
  jstring jarg1;
  if (arg1!=NULL) {
    jarg1 = javaEnv->NewStringUTF(arg1);
  } else {
    jarg1=NULL;
  }
  handleJavaException();
  jobject jresult=javaEnv->CallObjectMethod(this->getJavaObject(), vtable[METHOD_sim_macaddr_from_name], jarg1);
  handleJavaException();
  if (jarg1!=NULL) {
    javaEnv->DeleteLocalRef(jarg1);
  }
  char* result;
  if (jresult!=NULL) {
    const char*  jresult_bytes = javaEnv->GetStringUTFChars((jstring)jresult,NULL);
    handleJavaException();
    jsize        jresult_size = javaEnv->GetStringUTFLength((jstring)jresult);
    handleJavaException();
                 result = new char[jresult_size+1];
    for (int i=0;i<jresult_size;i++) {
      result[i] = jresult_bytes[i];
    }
    result[jresult_size]=0;
    javaEnv->ReleaseStringUTFChars((jstring)jresult, jresult_bytes);
    handleJavaException();
    javaEnv->DeleteLocalRef(jresult);
  } else {
    result=NULL;
  }
  return result;
}

int ClickAdapter::sim_send_to_if(int arg1, int arg2, int arg3, JavaByteArray* arg4, int id, int fid, int simtype)
{
  jarray jarg4;
  if (arg4!=NULL) {
    jarg4=(jarray)(arg4->getJavaObject());
  } else {
    jarg4=NULL;
  }
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_send_to_if], arg1, arg2, arg3, jarg4, id, fid, simtype);
  handleJavaException();
  int result = (int)jresult;
  return result;
}

int ClickAdapter::sim_schedule(int arg1, jlong arg2)
{
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_schedule], arg1, arg2);
  handleJavaException();
  return jresult;
}

const char* ClickAdapter::sim_get_node_name()
{
  jobject jresult=javaEnv->CallObjectMethod(this->getJavaObject(), vtable[METHOD_sim_get_node_name]);
  handleJavaException();
  char* result;
  if (jresult!=NULL) {
    const char*  jresult_bytes = javaEnv->GetStringUTFChars((jstring)jresult,NULL);
    handleJavaException();
    jsize        jresult_size = javaEnv->GetStringUTFLength((jstring)jresult);
    handleJavaException();
                 result = new char[jresult_size+1];
    for (int i=0;i<jresult_size;i++) {
      result[i] = jresult_bytes[i];
    }
    result[jresult_size]=0;
    javaEnv->ReleaseStringUTFChars((jstring)jresult, jresult_bytes);
    handleJavaException();
    javaEnv->DeleteLocalRef(jresult);
  } else {
    result=NULL;
  }
  return result;
}
/*
int ClickAdapter::sim_if_ready(int arg1, int arg2)
{
  jint jarg1 = (jint)arg1;
  jint jarg2 = (jint)arg2;
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_if_ready], jarg1, jarg2);
  handleJavaException();
  int result = (int)jresult;
  return result;
}
*/
int ClickAdapter::sim_trace(int arg1, const char* arg2)
{
  jint jarg1 = (jint)arg1;
  jstring jarg2;
  if (arg2!=NULL) {
    jarg2 = javaEnv->NewStringUTF(arg2);
  } else {
    jarg2=NULL;
  }
  handleJavaException();
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_trace], jarg1, jarg2);
  handleJavaException();
  if (jarg2!=NULL) {
    javaEnv->DeleteLocalRef(jarg2);
  }
  int result = (int)jresult;
  return result;
}

int ClickAdapter::sim_get_node_id(int arg1)
{
  jint jarg1 = (jint)arg1;
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_get_node_id], jarg1);
  handleJavaException();
  int result = (int)jresult;
  return result;
}

int ClickAdapter::sim_get_next_pkt_id(int arg1)
{
  jint jarg1 = (jint)arg1;
  jint jresult=javaEnv->CallIntMethod(this->getJavaObject(), vtable[METHOD_sim_get_next_pkt_id], jarg1);
  handleJavaException();
  int result = (int)jresult;
  return result;
}
}
}
