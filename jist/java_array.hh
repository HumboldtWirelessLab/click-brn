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

#ifndef JAVA_ARRAY_H
#define JAVA_ARRAY_H

#include <jni.h>
#include <java_base_class.hh>

class JavaArray : public JavaBaseClass {
  public:
    JavaArray();
    JavaArray(jarray obj);
    
    void* getArrayData();
    int   getArrayLength();
};

class JavaBooleanArray : public JavaArray {
  public:
    JavaBooleanArray();
    JavaBooleanArray(jarray obj);
    JavaBooleanArray(const bool* data,int len);
    
    void* getArrayData();
};

class JavaByteArray : public JavaArray {
  public:
    JavaByteArray();
    JavaByteArray(jarray obj);
    JavaByteArray(const char* data,int len);
    
    void* getArrayData();
};

class JavaCharArray : public JavaArray {
  public:
    JavaCharArray();
    JavaCharArray(jarray obj);
    JavaCharArray(const char* data,int len);
    
    void* getArrayData();
};

class JavaShortArray : public JavaArray {
  public:
    JavaShortArray();
    JavaShortArray(jarray obj);
    JavaShortArray(const short* data,int len);
    
    void* getArrayData();
};

class JavaIntArray : public JavaArray {
  public:
    JavaIntArray();
    JavaIntArray(jarray obj);
    JavaIntArray(const int* data,int len);
    
    void* getArrayData();
};

class JavaLongArray : public JavaArray {
  public:
    JavaLongArray();
    JavaLongArray(jarray obj);
    JavaLongArray(const long* data,int len);
    
    void* getArrayData();
};

class JavaFloatArray : public JavaArray {
  public:
    JavaFloatArray();
    JavaFloatArray(jarray obj);
    JavaFloatArray(const float* data,int len);
    
    void* getArrayData();
};

class JavaDoubleArray : public JavaArray {
  public:
    JavaDoubleArray();
    JavaDoubleArray(jarray obj);
    JavaDoubleArray(const double* data,int len);
    
    void* getArrayData();
};

#endif
