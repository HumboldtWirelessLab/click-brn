////////////////////////////////////// c++ ///////////////////////////////
//
//  Module           : FibonacciHeap.hh
//  Description      : FibonacciHeap class definition
//  Author           : Jan Knopf
//  Email            : FibonacciHeap@email.de
//  Copyright        : University of Heidelberg
//  Created on       : Mon Sep 27 09:16:04 2004
//  Last modified by : knopf
//  Last modified on : Mon Oct 11 11:58:24 2004
//  Update count     : 0
//
//////////////////////////////////////////////////////////////////////////
//
// Date         Name     Changes/Extensions
// ----         ----     ------------------
//
//////////////////////////////////////////////////////////////////////////


#ifndef FIBONACCIHEAP_HH
#define FIBONACCIHEAP_HH

#include <click/element.hh>

#include "fibonaccinode.hh"

CLICK_DECLS

//Default Heap size
#define DEFAULTSIZE 1000
//Default enlarge number -> FibonacciHeap::addNodesToHeap
#define ADDNUMBER 500

/**
 * This class implements a Fibinacci Heap.
 * The implementation is based on the book "Algorithms" by Cormen et al.
 */ 
class FibonacciHeap {


private:

    // -----------------------------------------------------
    // -------- V a r i a b l e s --------------------------
    // -----------------------------------------------------
    /**
     * m_numberOfNodes contains the number of nodes currently 
     * in the heap.
     */
    int m_numberOfNodes;

    /**
     * m_heapSize contains the size of the heap.
     */
    int m_heapSize;

    /**
     * m_freeNode contains the index of the first free node
     * in the free list.
     */
    int m_freeNode;

    /**
     * m_minimumNode contains the index of the node in the heap
     * with the minimum value. It is also a pointer to the root
     * list.
     */
    int m_minimumNode;

    /**
     * m_nodelist is an array of FibonacciNode's. The size of the
     * array is m_heapSize. This array contains the heap.
     *
     * It also contains the free list. You can access the free list
     * via m_freeNode. The next node in this list is saved in m_right
     * variable of the FibonacciNode
     */
    FibonacciNode* m_nodeList;

    /**
     * the s_noNode const indicates that a nodeId is a invalid one
     */
    static const int s_noNode = -1;


public:

    // -----------------------------------------------------
    // -------- M e t h o d s  ( p u b l i c ) -------------
    // -----------------------------------------------------

    /**
     * Constructor.
     * \param size size of heap, default is 1000
     */
    FibonacciHeap(int size = DEFAULTSIZE);

    /**
     * Destructor
     */ 
    ~FibonacciHeap();

    /**
     * Copy-constructor
     */
    FibonacciHeap (const FibonacciHeap&);

    /**
     * Assignment operator
     */
    FibonacciHeap& operator=(const FibonacciHeap&);

    /**
     * Unite operator
     */
    FibonacciHeap& operator+=(const FibonacciHeap&);

    /**
     * This function inserts a new node into the heap.
     * \param nodeValue the value of the node to be insert
     *
     * \return id, if successful, else s_noNode
     */
    int insertNode(int nodeValue);

    /**
     * This function deletes a node from the heap.
     * \param nodeId the nodeId of the node to be deleted
     */
    void deleteNode(int nodeId);

    /**
     * This function extracts the minimum node from the heap.
     *
     * \return id of the minimum node
     */
    int extractMinimumNode();

    /**
     * This function extracts the minimum node from the heap.
     *
     * \return value of the minimum node
     */
    int extractMinimumNodeValue();

    /**
     * This function returns the number of nodes in heap
     *
     * \return m_numberOfNodes
     */
    int getNumberOfNodes() const;

    /**
     * This function returns the size of the heap
     *
     * \return m_heapSize
     */
    int getHeapSize() const;

    /**
     * This function returns the id of the minimum node in heap
     *
     * \return m_minimumNode
     */
    int getMinimumNode() const;

    /**
     * This function returns the value of the minimum node in heap
     *
     * \return value of m_minimumNode
     */
    int getMinimumNodeValue() const;

    /**
     * This function returns the value of a node
     * \param nodeId the id of the node
     *
     * \return value of node node_id
     */
    int getNodeValue(int nodeId) const;

    /**
     * This function returns if a node is in the heap
     * \param nodeId the id of the node
     *
     * \return nodeId in heap or not
     */
    bool isNodeUsed(int nodeId) const;

    /**
     * This function changes the value of a node
     * \param nodeId the id of the node
     * \param newValue the new value of the node
     */
    void changeNodeValue(int nodeId, int newValue);

    /**
     * This function creates a printout of the Heap
     */
    void printHeap() const;


private:

    // -----------------------------------------------------
    // -------- M e t h o d s  ( p r i v a t e ) -----------
    // -----------------------------------------------------

    /**
     * This function inserts a node in a existing list (e.g. child list)
     * \param newNode the node to be insert
     * \param right the node to become the right neighbor of newNode
     */
    void insertInList(int newNode, int right);

    /**
     * This function removes a node from a a existing list 
     * (e.g. child list)
     * \param nodeId the node to be removed
     */
    void removeFromList(int nodeId);

    /**
     * This function enlarges the heap.
     * It is called by insertNode
     * \param numberOfNodesToAdd enlarge Heap by this variable,
     *                           default = ADDNUMBER 
     */
    void addNodesToHeap(unsigned int numberOfNodesToAdd = ADDNUMBER);

    /**
     * This function consolidates the root list of the heap by reducing
     * the root nodes to a minimal neccesary level. 
     */
    void consolidate();

    /**
     * This function inserts the newChild below newParent
     * \param newChild nodeId of the node to become a child
     * \param newParent nodeId of the node to become a parent
     */
    void heapLink(int newChild, int newParent);

    /**
     * This function removes a child from it's parent and 
     * inserts it in the root list. 
     * \param exChild nodeId of the ex child node
     * \param exParent nodeId of the ex parent node
     */
    void cut(int exChild, int exParent);

    /**
     * This function investigates, if exParent has lost it's second child.
     * If this is the case, it will be cut() and the parent of exParent is
     * investigated. 
     * \param exParent nodeId of a parent which just lost a child
     */
    void cascadingCut(int exParent);

    /**
     * This function decreases the value of a node
     * \param nodeId the id of the node
     * \param newValue the new value of the node
     */
    void decreaseNodeValue(int nodeId, int newValue);

    /**
     * This function increases the value of a node
     * \param nodeId the id of the node
     * \param newValue the new value of the node
     */
    void increaseNodeValue(int nodeId, int newValue);


}; // class FibonacciHeap

CLICK_ENDDECLS

#endif
