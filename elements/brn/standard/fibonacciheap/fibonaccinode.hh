////////////////////////////////////// c++ ///////////////////////////////
//
//  Module           : FibonacciNode.hh
//  Description      : FibonacciNode class definition
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
// 01 Oct 2004  knopf    added member variable free
//                       added function getFree
//                       added function setFree
//
//////////////////////////////////////////////////////////////////////////


#ifndef FIBONACCINODE_HH
#define FIBONACCINODE_HH

#include <click/element.hh>

CLICK_DECLS

/**
 * This class provides the handling of nodes in Fibonacci Heaps
 * It's only propose is to be used by the FibonacciHeap class
 */
class FibonacciNode {


 private:
    
    // -----------------------------------------------------
     // -------- V a r i a b l e s --------------------------
     // -----------------------------------------------------
    /**
     * m_degree contains the number of children of the node
     */
     int m_degree;

    /**
     * m_parent contains the index of the parent node
     */
     int m_parent;

    /**
     * m_child contains the index of one child node
     */
     int m_child;

    /**
     * m_left contains the index of the left neighbor node
     */
     int m_left;

    /**
     * m_right contains the index of the right neighbor node
     */
     int m_right;

    /**
     * m_value contains the value of the node
     */
     int m_value;

    /**
     * m_mark indicates if the node has lost a child
     */
     bool m_mark;

    /**
     * m_free indicates if the node is free for use
     */
     bool m_free;

    /**
     * s_noPointer is a constant to indicate a no index
     */
     static const int s_noPointer = -1;


 public:

    // -----------------------------------------------------
    // -------- M e t h o d s  ( p u b l i c ) -------------
    // -----------------------------------------------------
    
    /**
     * Constructor.
     */ 
    FibonacciNode();
    
    /**
     * Destructor
     */ 
    ~FibonacciNode();
    
    /**
     * This function sets the m_degree variable
     * \param degree new degree value
     */
    void setDegree(int degree);
    
    /**
     * This function sets the m_parent variable
     * \param parent new parent value
     */
    void setParent(int parent);
    
    /**
     * This function sets the m_child variable
     * \param child new child value
     */
    void setChild(int child);
    
    /**
     * This function sets the m_left variable
     * \param left new left value
     */
    void setLeft(int left);
    
    /**
     * This function sets the m_right variable
     * \param right new rigth value
     */
    void setRight(int right);
    
    /**
     * This function sets the m_value variable
      * \param value new value value
      */
    void setValue(int value);
    
    /**
     * This function sets the m_mark variable
     * \param mark new mark value
     */
     void setMark(bool mark);
    
    /**
     * This function sets the m_free variable
     * \param free new free value
     */
    void setFree(bool free);
    
    /**
     * This function sets the m_parent variable to s_noPointer
     */
    void setNoParent();
    
    /**
     * This function sets the m_child variable to s_noPointer
     */
    void setNoChild();

    /**
     * This function sets the m_left variable to s_noPointer
     */
    void setNoLeft();
    
    /**
     * This function sets the m_right variable to s_noPointer
     */
    void setNoRight();

    /**
     * This function returns the m_degree variable
     * \return m_degree
     */
    int getDegree() const;
    
    /**
     * This function returns the m_parent variable
     * \return m_parent
     */
    int getParent() const;

    /**
     * This function returns the m_child variable
     * \return m_child
     */
    int getChild() const;

    /**
     * This function returns the m_left variable
     * \return m_left
     */
    int getLeft() const;

    /**
     * This function returns the m_right variable
     * \return m_right
     */
    int getRight() const;

    /**
     * This function returns the m_value variable
     * \return m_value
     */
    int getValue() const;

    /**
     * This function returns the m_mark variable
     * \return m_mark
     */
    bool getMark() const;

    /**
     * This function returns the m_free variable
     * \return m_free
     */
    bool getFree() const;

    /**
     * This function asks if m_parent is equal to s_noPointer
     * \return true, if equal, else false
     */
    bool noParent() const;

    /**
     * This function asks if m_child is equal to s_noPointer
     * \return true, if equal, else false
     */
    bool noChild() const;

    /**
     * This function asks if m_left is equal to s_noPointer
     * \return true, if equal, else false
     */
    bool noLeft() const;

    /**
     * This function asks if m_right is equal to s_noPointer
     * \return true, if equal, else false
     */
    bool noRight() const;
 

}; // class FibonacciNode

CLICK_ENDDECLS

#endif
