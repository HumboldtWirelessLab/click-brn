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

#ifndef JAVA_BASE_CLASS_H
#define JAVA_BASE_CLASS_H

#include <jni.h>

class JavaBaseClass {
  private:
    jobject obj;
    
  protected:
    inline void handleJavaException();
    
  public:
    inline JavaBaseClass();
    inline JavaBaseClass(jobject obj);
    
    inline void    clearJavaObject();
    inline void    setJavaObject(jobject obj);
    inline jobject getJavaObject();
};

extern JNIEnv  *javaEnv;
extern jobject javaException;

inline JavaBaseClass::JavaBaseClass()
{
  this->obj = NULL;
}

inline JavaBaseClass::JavaBaseClass(jobject obj)
{
  this->obj = obj;
}

inline void JavaBaseClass::handleJavaException()
{
  jthrowable exc = javaEnv->ExceptionOccurred();
  if (exc) {
    javaException = exc;
    javaEnv->ExceptionClear();
    throw exc;
  }
}

inline void JavaBaseClass::clearJavaObject()
{
  this->obj = NULL;
}

inline void JavaBaseClass::setJavaObject(jobject obj)
{
  this->obj = obj;
}

inline jobject JavaBaseClass::getJavaObject()
{
  return this->obj;
}

#endif
