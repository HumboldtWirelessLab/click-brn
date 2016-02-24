////////////////////////////////////// c++ ///////////////////////////////
//
//  Module           : FibonacciNode.cpp
//  Description      : Declaration of functions of class FibonacciNode
//  Author           : Jan Knopf
//  Email            : FibonacciHeap@email.de
//  Copyright        : University of Heidelberg
//  Created on       : Wed Sep 22 17:31:52 2004
//  Last modified by : knopf
//  Last modified on : Fri Oct 01 10:52:14 2004
//  Update count     : 1
//
//////////////////////////////////////////////////////////////////////////
//
// Date         Name     Changes/Extensions
// ----         ----     ------------------
// 01 Oct 2004  knopf    added function getFree
//                       added function setFree
//
//////////////////////////////////////////////////////////////////////////

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "fibonaccinode.hh"

CLICK_DECLS

//
// C o n s t r u c t o r
//
FibonacciNode::FibonacciNode(): m_value(0)
{
    m_degree = 0;
    m_parent = s_noPointer;
    m_child  = s_noPointer;
    m_left   = s_noPointer;
    m_right  = s_noPointer;
    m_mark   = false;
    m_free   = true;
}

//
// D e s t r u c t o r
//
FibonacciNode::~FibonacciNode(){}

//
// s e t D e g r e e
//
void FibonacciNode::setDegree(int degree){ 
    m_degree = degree;
}

//
// s e t P a r e n t
//
void FibonacciNode::setParent(int parent){ 
    m_parent = parent;
}

//
// s e t C h i l d
// 
void FibonacciNode::setChild(int child){ 
    m_child = child;
}

//
// s e t L e f t
//
void FibonacciNode::setLeft(int left){ 
    m_left = left;
}

//
// s e t R i g h t
//
void FibonacciNode::setRight(int right){ 
    m_right = right;
}

//
// s e t V a l u e
//
void FibonacciNode::setValue(int value){ 
    m_value = value;
}

//
// s e t M a r k
//
void FibonacciNode::setMark(bool mark){ 
    m_mark = mark;
}

//
// s e t F r e e
//
void FibonacciNode::setFree(bool free){ 
    m_free = free;
}

//
// s e t N o P a r e n t
//
void FibonacciNode::setNoParent(){ 
    m_parent = s_noPointer;
}

//
// s e t N o C h i l d
// 
void FibonacciNode::setNoChild(){ 
    m_child = s_noPointer;
}

//
// s e t N o L e f t
//
void FibonacciNode::setNoLeft(){ 
    m_left = s_noPointer;
}

//
// s e t N o R i g h t
//
void FibonacciNode::setNoRight(){ 
    m_right = s_noPointer;
}

//
// g e t D e g r e e
//
int FibonacciNode::getDegree() const { 
    return m_degree;
}

//
// g e t P a r e n t
//
int FibonacciNode::getParent() const { 
    return m_parent;
}

//
// g e t C h i l d
// 
int FibonacciNode::getChild() const { 
    return m_child;
}

//
// g e t L e f t
//
int FibonacciNode::getLeft() const { 
    return m_left;
}

//
// g e t R i g h t
//
int FibonacciNode::getRight() const { 
    return m_right;
}

//
// g e t V a l u e
//
int FibonacciNode::getValue() const { 
    return m_value;
}

//
// g e t M a r k
//
bool FibonacciNode::getMark() const { 
    return m_mark;
}

//
// g e t F r e e
//
bool FibonacciNode::getFree() const { 
    return m_free;
}

//
// n o P a r e n t
//
bool FibonacciNode::noParent() const {
    return ( m_parent == s_noPointer );
}

//
// n o C h i l d
//
bool FibonacciNode::noChild() const {
    return ( m_child == s_noPointer );
}

//
// n o L e f t
// 
bool FibonacciNode::noLeft() const {
    return ( m_left == s_noPointer );
}

//
// n o R i g h t
//
bool FibonacciNode::noRight() const {
    return ( m_right == s_noPointer );
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FibonacciNode)
