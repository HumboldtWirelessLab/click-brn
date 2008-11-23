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

#include <jni.h>
#include <java_array.hh>

extern JNIEnv     *javaEnv;

JavaArray::JavaArray()
  : JavaBaseClass()
{
}

JavaArray::JavaArray(jarray obj)
  : JavaBaseClass(obj)
{
}

void* JavaArray::getArrayData()
{
  return NULL;
}

int JavaArray::getArrayLength()
{
  int len = (int)(javaEnv->GetArrayLength((jarray)getJavaObject()));
  handleJavaException();
  return len;
}

// --------------------------------------------------------------------------

JavaByteArray::JavaByteArray()
  : JavaArray()
{
}

JavaByteArray::JavaByteArray(jarray obj)
  : JavaArray(obj)
{
}

JavaByteArray::JavaByteArray(const char* data,int len)
  : JavaArray()
{
  jbyteArray obj = javaEnv->NewByteArray((jsize)len);
  handleJavaException();
  
  jbyte* jdata = new jbyte[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetByteArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
  //javaEnv->DeleteLocalRef(obj);
}

void* JavaByteArray::getArrayData()
{
  int    len = getArrayLength();
  jbyte* jdata = new jbyte[len];
  javaEnv->GetByteArrayRegion((jbyteArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  char* data = new char[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaBooleanArray::JavaBooleanArray()
  : JavaArray()
{
}

JavaBooleanArray::JavaBooleanArray(jarray obj)
  : JavaArray(obj)
{
}

JavaBooleanArray::JavaBooleanArray(const bool* data,int len)
  : JavaArray()
{
  jbooleanArray obj = javaEnv->NewBooleanArray((jsize)len);
  handleJavaException();
  
  jboolean* jdata = new jboolean[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetBooleanArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaBooleanArray::getArrayData()
{
  int       len = getArrayLength();
  jboolean* jdata = new jboolean[len];
  javaEnv->GetBooleanArrayRegion((jbooleanArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  bool* data = new bool[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaCharArray::JavaCharArray()
  : JavaArray()
{
}

JavaCharArray::JavaCharArray(jarray obj)
  : JavaArray(obj)
{
}

JavaCharArray::JavaCharArray(const char* data,int len)
  : JavaArray()
{
  jcharArray obj = javaEnv->NewCharArray((jsize)len);
  handleJavaException();
  
  jchar* jdata = new jchar[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetCharArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaCharArray::getArrayData()
{
  int       len = getArrayLength();
  jchar* jdata = new jchar[len];
  javaEnv->GetCharArrayRegion((jcharArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  char* data = new char[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaShortArray::JavaShortArray()
  : JavaArray()
{
}

JavaShortArray::JavaShortArray(jarray obj)
  : JavaArray(obj)
{
}

JavaShortArray::JavaShortArray(const short* data,int len)
  : JavaArray()
{
  jshortArray obj = javaEnv->NewShortArray((jsize)len);
  handleJavaException();
  
  jshort* jdata = new jshort[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetShortArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaShortArray::getArrayData()
{
  int       len = getArrayLength();
  jshort* jdata = new jshort[len];
  javaEnv->GetShortArrayRegion((jshortArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  short* data = new short[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaIntArray::JavaIntArray()
  : JavaArray()
{
}

JavaIntArray::JavaIntArray(jarray obj)
  : JavaArray(obj)
{
}

JavaIntArray::JavaIntArray(const int* data,int len)
  : JavaArray()
{
  jintArray obj = javaEnv->NewIntArray((jsize)len);
  handleJavaException();
  
  jint* jdata = new jint[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetIntArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaIntArray::getArrayData()
{
  int       len = getArrayLength();
  jint* jdata = new jint[len];
  javaEnv->GetIntArrayRegion((jintArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  int* data = new int[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaLongArray::JavaLongArray()
  : JavaArray()
{
}

JavaLongArray::JavaLongArray(jarray obj)
  : JavaArray(obj)
{
}

JavaLongArray::JavaLongArray(const long* data,int len)
  : JavaArray()
{
  jlongArray obj = javaEnv->NewLongArray((jsize)len);
  handleJavaException();
  
  jlong* jdata = new jlong[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetLongArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaLongArray::getArrayData()
{
  int       len = getArrayLength();
  jlong* jdata = new jlong[len];
  javaEnv->GetLongArrayRegion((jlongArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  long* data = new long[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaFloatArray::JavaFloatArray()
  : JavaArray()
{
}

JavaFloatArray::JavaFloatArray(jarray obj)
  : JavaArray(obj)
{
}

JavaFloatArray::JavaFloatArray(const float* data,int len)
  : JavaArray()
{
  jfloatArray obj = javaEnv->NewFloatArray((jsize)len);
  handleJavaException();
  
  jfloat* jdata = new jfloat[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetFloatArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaFloatArray::getArrayData()
{
  int       len = getArrayLength();
  jfloat* jdata = new jfloat[len];
  javaEnv->GetFloatArrayRegion((jfloatArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  float* data = new float[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}

// --------------------------------------------------------------------------

JavaDoubleArray::JavaDoubleArray()
  : JavaArray()
{
}

JavaDoubleArray::JavaDoubleArray(jarray obj)
  : JavaArray(obj)
{
}

JavaDoubleArray::JavaDoubleArray(const double* data,int len)
  : JavaArray()
{
  jdoubleArray obj = javaEnv->NewDoubleArray((jsize)len);
  handleJavaException();
  
  jdouble* jdata = new jdouble[len];
  for (int i=0;i<len;i++) {
    jdata[i] = data[i];
  }
  javaEnv->SetDoubleArrayRegion(obj, (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  delete[] jdata;
  
  setJavaObject(obj);
//  javaEnv->DeleteLocalRef(obj);
}

void* JavaDoubleArray::getArrayData()
{
  int       len = getArrayLength();
  jdouble* jdata = new jdouble[len];
  javaEnv->GetDoubleArrayRegion((jdoubleArray)getJavaObject(), (jsize)0, (jsize)len, jdata);
  handleJavaException();
  
  double* data = new double[len];
  for (int i=0;i<len;i++) {
    data[i] = jdata[i];
  }
  
  delete[] jdata;
  return data;
}


