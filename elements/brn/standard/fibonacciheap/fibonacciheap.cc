////////////////////////////////////// c++ ///////////////////////////////
//
//  Module           : FibonacciHeap.cpp
//  Description      : Declaration of functions of class FibonacciHeap
//  Author           : Jan Knopf
//  Email            : FibonacciHeap@email.de
//  Copyright        : University of Heidelberg
//  Created on       : Mon Sep 27 09:16:04 2004
//  Last modified by : knopf
//  Last modified on : Wed Jan 07 11:50:53 2009
//  Update count     : 1
//
//  Source           : 		
//
//////////////////////////////////////////////////////////////////////////
//
// Date         Name     Changes/Extensions
// ----         ----     ------------------
// Jan 07, 2009 knopf    bugs reported by Matthias Walter
//                       changed endl into std::endl
//                       changed bad_alloc into std::bad_alloc
//                       fix increasedNodeValue() - could produce invalid 
//                         heap if node was minimum node.
//
//////////////////////////////////////////////////////////////////////////

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "fibonacciheap.hh"

CLICK_DECLS

//
// C o n s t r u c t o r
//
FibonacciHeap::FibonacciHeap(int size){
  int i;
  
  try {
    m_nodeList = new FibonacciNode[size];
  }
  catch (std::bad_alloc) {
//    std::cerr << "  FibonacciHeap::FibonacciHeap ERROR " << std::endl;
//    std::cerr << "       NOT enough memory " << std::endl;
    throw;
  }
  m_numberOfNodes = 0;
  m_heapSize = size;
  m_freeNode = 0;
  m_minimumNode = s_noNode;
  
  // initialise free list
  for(i = 0; i < (size-1); i++) {
    m_nodeList[i].setRight(i+1);
  }
}

//
// D e s t r u c t o r
//
FibonacciHeap::~FibonacciHeap(){
  delete[] m_nodeList;
}

//
// C o p y - c o n s t r u c t o r
//
FibonacciHeap::FibonacciHeap (const FibonacciHeap& FH){
  m_numberOfNodes = FH.m_numberOfNodes;
  m_heapSize = FH.m_heapSize;
  m_freeNode = FH.m_freeNode;
  m_minimumNode = FH.m_minimumNode;
  
  try {
    m_nodeList        = new FibonacciNode[m_heapSize];
  }
  catch (std::bad_alloc) {
//    std::cerr << "  FibonacciHeap::FibonacciHeap(const FibonacciHeap& FH)";
//    std::cerr << "ERROR " << std::endl;
//    std::cerr << "       NOT enough memory " << std::endl;
    throw;
  }
  
  for (int i = 0; i < m_heapSize; i++) {
    m_nodeList[i] = FH.m_nodeList[i];
  }
}

//
// A s s i g n m e n t   o p e r a t o r
//
FibonacciHeap& FibonacciHeap::operator=(const FibonacciHeap& FH){

  // self reference ?
  if (this != &FH) {
    // current heap bigger than assigned heap? 
    if ( m_heapSize > FH.m_heapSize ){
	    // copy assigned heap
	    for (int i = 0; i < FH.m_heapSize; i++) {
        m_nodeList[i] = FH.m_nodeList[i];
	    }
	    // initialise unused nodes
	    for (int i = FH.m_heapSize; i < m_heapSize; i++) {
        m_nodeList[i].setNoParent();
        m_nodeList[i].setNoChild();
        m_nodeList[i].setNoLeft();
        m_nodeList[i].setRight(i+1);
        m_nodeList[i].setDegree(0);
        m_nodeList[i].setMark(false);
        m_nodeList[i].setFree(true);
	    }
	    // update free list
	    if ( FH.m_freeNode == s_noNode) {
        m_nodeList[m_heapSize-1].setNoRight();
	    }
	    else {
        m_nodeList[m_heapSize-1].setRight(FH.m_freeNode);
	    }
	    m_freeNode = FH.m_heapSize;
    }
    else {
	    // create new heap
	    if ( m_heapSize != FH.m_heapSize ) {
        m_heapSize = FH.m_heapSize;
        
        delete[] m_nodeList;
        try {
          m_nodeList = new FibonacciNode[m_heapSize];
        }
        catch (std::bad_alloc) {
//          std::cerr << "  FibonacciHeap::operator=";
//          std::cerr << "ERROR " << std::endl;
//          std::cerr << "       NOT enough memory " << std::endl;
          throw;
        }
	    }
	    for (int i = 0; i < FH.m_heapSize; i++) {
        m_nodeList[i] = FH.m_nodeList[i];
	    }
      
	    m_freeNode = FH.m_freeNode;
    }
    
    m_minimumNode = FH.m_minimumNode;
    m_numberOfNodes = FH.m_numberOfNodes;
  }
  
  return *this;
}

//
// U n i t e   o p e r a t o r
//
FibonacciHeap& FibonacciHeap::operator+=(const FibonacciHeap& FH){
  
  FibonacciNode node;
  Vector<FibonacciNode> tmp; // help array to save old heap

  // self reference ?
  if ( this != &FH ) {
    // save old heap
    for (int i=0; i<m_heapSize; i++) {
	    tmp.push_back(m_nodeList[i]);
	}
    
    // generate new heap
    int newHeapSize; // new size of heap
    newHeapSize = m_heapSize + FH.m_heapSize;
    delete[] m_nodeList;
    try {
	    m_nodeList = new FibonacciNode[newHeapSize];
    }
    catch (std::bad_alloc) {
//	    std::cerr << "  FibonacciHeap::operator+=";
//	    std::cerr << "ERROR " << std::endl;
//	    std::cerr << "       NOT enough memory " << std::endl;
	    throw;
    }
    
    // copy old heap to new one
    for (int i=0; i<m_heapSize; i++) {
      m_nodeList[i] = tmp[i];
    }
    // add the second heap
    for (int i=m_heapSize; i<newHeapSize; i++) {
	    node = FH.m_nodeList[i-m_heapSize];
	    if ( !node.noParent() ) {
        m_nodeList[i].setParent(node.getParent() + m_heapSize);
	    }
	    else {
        m_nodeList[i].setNoParent();
	    }
	    if ( !node.noChild() ) {
        m_nodeList[i].setChild(node.getChild() + m_heapSize);
	    }
	    else {
        m_nodeList[i].setNoChild();
	    }
	    if ( !node.noLeft() ) {
        m_nodeList[i].setLeft(node.getLeft() + m_heapSize);
	    }
	    else {
        m_nodeList[i].setNoLeft();
	    }
	    if ( !node.noRight() ) {
        m_nodeList[i].setRight(node.getRight() + m_heapSize);
	    }
	    else {
        m_nodeList[i].setNoRight();
	    }
	    m_nodeList[i].setValue( node.getValue() );
	    m_nodeList[i].setMark( node.getMark() );
	    m_nodeList[i].setFree( node.getFree() );
    }
    
    // merge root lists
    int left;        // left of minimum node in old heap
    int leftFH;      // left of minimum node in added heap
    left = m_nodeList[m_minimumNode].getLeft();
    leftFH = FH.m_nodeList[FH.m_minimumNode].getLeft();
    leftFH += m_heapSize;
    m_nodeList[left].setRight(FH.m_minimumNode + m_heapSize);
    m_nodeList[FH.m_minimumNode + m_heapSize].setLeft(left);
    m_nodeList[m_minimumNode].setLeft(leftFH);
    m_nodeList[leftFH].setRight(m_minimumNode);
    
    // set new minimum node
    if ( FH.m_nodeList[FH.m_minimumNode].getValue() < 
         m_nodeList[m_minimumNode].getValue() ) {
	    m_minimumNode = FH.m_minimumNode + m_heapSize;
    }
    
    // update free list
    if ( FH.m_freeNode != s_noNode ) {
	    if ( m_freeNode == s_noNode ) {
        m_freeNode = FH.m_freeNode + m_heapSize;
	    }
	    else {
        int currentId = m_freeNode; // current processed Id
        while ( m_nodeList[currentId].getRight() != s_noNode ) {
          currentId = m_nodeList[currentId].getRight();
        }
        m_nodeList[currentId].setRight(FH.m_freeNode 
                                       + m_heapSize);
	    }
    }
    
  }
  
  return *this;
}

//
// i n s e r t N o d e
//
int FibonacciHeap::insertNode(int nodeValue){
  int nodeId;       // node ID of new node

  // no more free nodes? -> Create new ones
  if ( m_freeNode == s_noNode ) {
//    std::cerr << " FibonacciHeap::insertNode " << std::endl;
//    std::cerr << "   WARNING: Heap is full, adding more nodes " << std::endl;
    addNodesToHeap();
  }
  
  // insert new node
  nodeId = m_freeNode;    
  if ( m_nodeList[nodeId].noRight() ) {
    m_freeNode = s_noNode;
  }
  else {
    m_freeNode = m_nodeList[nodeId].getRight();
  }
  
  // initialise new node
  m_nodeList[nodeId].setDegree(0);
  m_nodeList[nodeId].setNoParent();
  m_nodeList[nodeId].setNoChild();
  m_nodeList[nodeId].setFree(false);
  m_nodeList[nodeId].setValue(nodeValue);
  m_nodeList[nodeId].setMark(false);
  
  // new minimum node?
  if (m_minimumNode == s_noNode) {
    m_nodeList[nodeId].setLeft(nodeId);
    m_nodeList[nodeId].setRight(nodeId);
    m_minimumNode = nodeId;
  }
  else {
    insertInList(nodeId, m_minimumNode);
    int minNodeValue = m_nodeList[m_minimumNode].getValue(); // minimum node value
    if ( nodeValue < minNodeValue ) {
	    m_minimumNode = nodeId;
    }
  }
  m_numberOfNodes++;
  return nodeId;
}

//
// d e l e t e N o d e
//
void FibonacciHeap::deleteNode(int nodeId)
{
  // node ID valid?
  if ( (nodeId >= 0) && (nodeId < m_heapSize) ) {
    if ( ! m_nodeList[nodeId].getFree() ) {
	    // set nodeID value to minimum and extract
	    if (nodeId != m_minimumNode) {
        int minNodeValue = m_nodeList[m_minimumNode].getValue();// minimum node value
        decreaseNodeValue(nodeId, minNodeValue);
	    }
	    m_minimumNode = nodeId;
	    extractMinimumNode();
    }
    else {
	    // NODE allready deleted
//	    std::cerr << " FibonacciHeap::deleteNode   WARNING " << std::endl;
//	    std::cerr << "      Node " << nodeId 
//                << " is allready deleted. " << std::endl;
    }
  }
  else {
    // OUT OF RANGE
//    std::cerr << " FibonacciHeap::deleteNode   WARNING " << std::endl;
//    std::cerr << "      Node ID " << nodeId 
//              << " is not a valid ID. " << std::endl;
  }
}

//
// e x t r a c t M i n i m u m N o d e 
//
int FibonacciHeap::extractMinimumNode(){
  int minimumNodeId = m_minimumNode; // old minimum node
  int child;  // child of minimum node
  int child_left; // left of child 
  int right;  // right of minimum node
  int left;   // left of minimum node

  // HEAP EMPTY?    
  if (m_numberOfNodes == 0) {
    return(s_noNode);
  }
    
  right = m_nodeList[minimumNodeId].getRight();
  left = m_nodeList[minimumNodeId].getLeft();
    
  // minimum node only node in root list?
  if ( minimumNodeId != right ) {
    // minimum node children?
    if ( ! m_nodeList[minimumNodeId].noChild() ) {
	    // add child list to the root list
	    child = m_nodeList[minimumNodeId].getChild();
	    child_left = m_nodeList[child].getLeft();
	    
	    m_nodeList[left].setRight(child);
	    m_nodeList[child].setLeft(left);
	    m_nodeList[right].setLeft(child_left);
	    m_nodeList[child_left].setRight(right);
	    
	    do {
        m_nodeList[child].setNoParent();
        child = m_nodeList[child].getRight();
	    } while ( child != right );
    }
    // only root list exists
    else {
	    removeFromList(minimumNodeId);
    }
	
    m_minimumNode = right;
	
    try {
	    consolidate();
    }
    catch (std::bad_alloc) {
//	    std::cerr << " FibonacciHeap::extractMinimumNode   WARNING " << std::endl;
//	    std::cerr << "     Consolidat failed " << std::endl;
    }
  }
  // no root list -> make child list the new root list
  else {
    if ( ! m_nodeList[minimumNodeId].noChild() ) {
	    child = m_nodeList[minimumNodeId].getChild();
	    child_left = m_nodeList[child].getLeft();
	    m_minimumNode = child;
	    child = child_left;
	    do {
        child = m_nodeList[child].getRight();
        m_nodeList[child].setNoParent();
	    } while (child != child_left);
	    
	    try {
        consolidate();
	    }
	    catch (std::bad_alloc) {
//        std::cerr << " FibonacciHeap::extractMinimumNode   WARNING " << std::endl;
//        std::cerr << "     Consolidat failed " << std::endl;
	    }
    }
  }
    
  // delete node and add to free list
  m_nodeList[minimumNodeId].setNoParent();
  m_nodeList[minimumNodeId].setNoChild();
  m_nodeList[minimumNodeId].setRight(m_freeNode);
  m_nodeList[minimumNodeId].setNoLeft();
  m_nodeList[minimumNodeId].setDegree(0);
  m_nodeList[minimumNodeId].setMark(false);
  m_nodeList[minimumNodeId].setFree(true);

  m_freeNode = minimumNodeId;
  m_numberOfNodes--;
    
  return minimumNodeId;
}

//
// e x t r a c t M i n i m u m N o d e V a l u e
//
int FibonacciHeap::extractMinimumNodeValue(){
  int minNodeValue;
  
  // HEAP EMPTY
  if ( m_minimumNode == s_noNode ) {
    return(s_noNode);
  }
  else {
    minNodeValue = m_nodeList[m_minimumNode].getValue();
    extractMinimumNode();
  }

  return minNodeValue;
}

//
// g e t N u m b e r O f N o d e s
//
int FibonacciHeap::getNumberOfNodes() const { 
  return m_numberOfNodes;
}

//
// g e t H e a p S i z e
//
int FibonacciHeap::getHeapSize() const {
  return m_heapSize; 
}

//
// g e t M i n i m u m N o d e 
//
int FibonacciHeap::getMinimumNode() const {
  return m_minimumNode;
}

//
// g e t M i n i m u m N o d e V a l u e
//
int FibonacciHeap::getMinimumNodeValue() const {
  int minimumNodeValue;

  minimumNodeValue = m_nodeList[m_minimumNode].getValue();

  return minimumNodeValue;
}

//
// g e t N o d e V a l u e
//
int FibonacciHeap::getNodeValue(int nodeId) const {
  if ( ( nodeId < 0 ) || ( nodeId >= m_heapSize ) ) {
    return s_noNode; // OUT OF RANGE
  }
  else {
    return ( m_nodeList[nodeId].getValue() );
  }
}

//
// i s N o d e U s e d
//
bool FibonacciHeap::isNodeUsed(int nodeId) const {
  if ((nodeId < 0) || (nodeId >= m_heapSize)) {
    return(false); // OUT OF RANGE
  }
    
  return ( !m_nodeList[nodeId].getFree() );
}

//
// c h a n g e N o d e V a l u e
//
void FibonacciHeap::changeNodeValue(int nodeId, int newValue){
  if ( (nodeId >= 0) && (nodeId < m_heapSize) ) {
    if (! m_nodeList[nodeId].getFree() ) {
	    if (newValue < m_nodeList[nodeId].getValue() ) { 
        decreaseNodeValue(nodeId, newValue);
	    }
	    else {
        increaseNodeValue(nodeId, newValue);
	    }
    }
  }
}

//
// p r i n t H e a p
//
void FibonacciHeap::printHeap() const {
//  int i;

  /*std::cout << std::endl << std::endl ;
  std::cout << "           Number of Nodes : " << m_numberOfNodes << std::endl;
  std::cout << "           Heapsize        : " << m_heapSize << std::endl;
  std::cout << "           Free Node       : " << m_freeNode << std::endl;
  std::cout << "           Minimum Node    : " << m_minimumNode << std::endl;
  std::cout << std::endl;
  std::cout << "            P  C  L  R  D  V  M  F" << std::endl;
  for (i=0; i<m_heapSize; i++){
    std::cout << "           " << m_nodeList[i].getParent() << "  ";
    std::cout << m_nodeList[i].getChild() << "  ";
    std::cout << m_nodeList[i].getLeft() << "  ";
    std::cout << m_nodeList[i].getRight() << "  ";
    std::cout << m_nodeList[i].getDegree() << "  ";
    std::cout << m_nodeList[i].getValue() << "  ";
    std::cout << m_nodeList[i].getMark() << "  ";
    std::cout << m_nodeList[i].getFree() << std::endl;
  }
  std::cout << std::endl << std::endl ;*/
}
    

//
// i n s e r t I n L i s t
//
void FibonacciHeap::insertInList(int newNode, int right){
  int left = m_nodeList[right].getLeft();

  m_nodeList[newNode].setRight(right);
  m_nodeList[newNode].setLeft(left);

  m_nodeList[right].setLeft(newNode);
  m_nodeList[left].setRight(newNode);
}

//
// r e m o v e F r o m L i s t
//
void FibonacciHeap::removeFromList(int nodeId){
  int left = m_nodeList[nodeId].getLeft();
  int right = m_nodeList[nodeId].getRight();

  m_nodeList[nodeId].setRight(nodeId);
  m_nodeList[nodeId].setLeft(nodeId);

  if (right != nodeId) {
    m_nodeList[left].setRight(right);
    m_nodeList[right].setLeft(left);
  }
}

//
// a d d N o d e s T o H e a p
//
void FibonacciHeap::addNodesToHeap(unsigned int numberOfNodesToAdd)
{
  Vector<FibonacciNode> tmp;
  int i;
  int newSize;

  // save old heap
  for (i=0; i<m_heapSize; i++) {
    tmp.push_back(m_nodeList[i]);
  }

  delete[] m_nodeList;

  // create new heap
  newSize = m_heapSize + numberOfNodesToAdd;
  try {
    m_nodeList = new FibonacciNode[newSize];
  }
  catch (std::bad_alloc) {
/*    std::cerr << "  FibonacciHeap::addNodesToHeap ERROR " << std::endl;
    std::cerr << "       NOT enough memory " << std::endl;
  */  throw;
  }

  // copy old heap
  for (i=0; i<m_heapSize; i++) {
    m_nodeList[i] = tmp[i];
  }

  // update free list
  for (i=m_heapSize; i<newSize-1; i++) {
    m_nodeList[i].setRight(i+1);
  }

  m_nodeList[newSize-1].setNoRight();

  m_freeNode = m_heapSize;
  m_heapSize = newSize;
}

//
// c o n s o l i d a t e
//
void FibonacciHeap::consolidate()
{
  int DnH = 1;  // Degree of heap
  int* A; // array with DnH elements
  int i = 1;
  int x;  // cuurent node id
  int y;  // compared node id
  int stop_node;  // last node in root list
  int no_of_rootnodes = 1;
  int* rootnodes; // save rootnodes
  int currentNodeValue;
  int minimumNodeValue;

  // calculate degree of heap
  do {
    i *= 2;
    DnH++;
  } while( i < m_numberOfNodes );
    
  // initialise A array
  try {
    A = new int[DnH];
  }
  catch (std::bad_alloc) {
/*    std::cerr << "  FibonacciHeap::consolidate()";
    std::cerr << "ERROR " << std::endl;
    std::cerr << "       NOT enough memory for array A" 
              << std::endl;
  */  throw;
  }
  for(i = 0; i < DnH; i++) A[i] = s_noNode;
    
  // calculate number of root nodes
  x = m_minimumNode;
  stop_node = m_nodeList[x].getLeft();
  while ( x != stop_node ) {
    no_of_rootnodes++;
    x = m_nodeList[x].getRight();
  }
    
  // save root nodes
  try {
    rootnodes = new int[no_of_rootnodes];
  }
  catch (std::bad_alloc) {
/*    std::cerr << "  FibonacciHeap::consolidate()";
    std::cerr << "ERROR " << std::endl;
    std::cerr << "       NOT enough memory for array rootnodes" 
              << std::endl;
  */  throw;
  }
  x = m_minimumNode;
  for (i = 0; i < no_of_rootnodes; i++) {
    rootnodes[i] = x;
    x = m_nodeList[x].getRight();
  }    

  // for each root node
  for (i = 0; i < no_of_rootnodes; i++) {
    x = rootnodes[i];
    int d = m_nodeList[x].getDegree();
    // merge trees of same degree
    while ( A[d] != s_noNode) {
	    y = A[d];
	    if (m_nodeList[x].getValue() > m_nodeList[y].getValue() ) {
        int tmp = y;
        y = x;
        x = tmp;
	    }
	    heapLink(y,x);
	    A[d] = s_noNode;
	    d++;
    }
    A[d] = x;
  }
    
  // calculate new minimum node and generate new root list
  m_minimumNode = s_noNode;
  for(i = 0; i < DnH; i++) {
    x = A[i];
    if ( x != s_noNode) {
	    if ( m_minimumNode == s_noNode ) {
        m_minimumNode = x;
        m_nodeList[m_minimumNode].setRight(m_minimumNode);
        m_nodeList[m_minimumNode].setLeft(m_minimumNode);
	    }
	    else {
        insertInList(x,m_minimumNode);
        currentNodeValue = m_nodeList[x].getValue();
        minimumNodeValue = m_nodeList[m_minimumNode].getValue(); 
        if ( currentNodeValue < minimumNodeValue ) {
          m_minimumNode = x;
        }
	    }
    }
  }
  
  delete[] rootnodes;
  delete[] A;
}

//
// h e a p L i n k
//
void FibonacciHeap::heapLink(int newChild, int newParent){
  int d = m_nodeList[newParent].getDegree();
  int child = m_nodeList[newParent].getChild();

  removeFromList(newChild);

  m_nodeList[newParent].setDegree(d+1);

  if ( m_nodeList[newParent].noChild() ) {
    m_nodeList[newParent].setChild(newChild);
  }
  else {
    insertInList(newChild,child);
  }

  m_nodeList[newChild].setParent(newParent);
  m_nodeList[newChild].setMark(false);
}

//
// c u t
//
void FibonacciHeap::cut(int exChild, int exParent){
  int d = m_nodeList[exParent].getDegree();
  int right = m_nodeList[exChild].getRight();
  int child = m_nodeList[exParent].getChild();
    
  if ( child == exChild ) { 
    if ( right == exChild ) m_nodeList[exParent].setNoChild();
    else {
	    m_nodeList[exParent].setChild(right);
	    removeFromList(exChild);
    }
  }
  else { 
    removeFromList(exChild);
  }

  m_nodeList[exParent].setDegree(d-1);

  insertInList(exChild,m_minimumNode);
    
  m_nodeList[exChild].setNoParent();
  m_nodeList[exChild].setMark(false);
}

//
// c a s c a d i n g C u t
//
void FibonacciHeap::cascadingCut(int exParent){
  int currentId = exParent;
  int parentOfCurrentId;
  bool cont = false;
    
  do {
    if ( ! m_nodeList[currentId].noParent() ) {
	    if ( m_nodeList[currentId].getMark() ) {
        parentOfCurrentId = m_nodeList[currentId].getParent();
        cut(currentId,parentOfCurrentId);
        cont = true;
        currentId = parentOfCurrentId;;
	    }
	    else {
        m_nodeList[currentId].setMark(true);
        cont = false;
	    }
    }
    else {
	    cont = false;
    }
	
  } while (cont);
}

//
// d e c r e a s e N o d e V a l u e 
//
void FibonacciHeap::decreaseNodeValue(int nodeId, int nodeValue){
  m_nodeList[nodeId].setValue(nodeValue);

  if ( ! m_nodeList[nodeId].noParent() ) {
    int parent = m_nodeList[nodeId].getParent();
    if ( nodeValue < m_nodeList[parent].getValue() ){
	    cut(nodeId,parent);
	    cascadingCut(parent);
    }
  }

  if ( nodeValue < m_nodeList[m_minimumNode].getValue() ) {
    m_minimumNode = nodeId;
  }
}

//
// i n c r e a s e N o d e V a l u e
//
void FibonacciHeap::increaseNodeValue(int nodeId, int nodeValue)
{
  // increase value and add child list to root list 
  if ( nodeValue != m_nodeList[nodeId].getValue() ) {
    int leftOfMinimumNode = m_nodeList[m_minimumNode].getLeft();
    int stopNode = m_minimumNode;
    int currentNode = m_minimumNode;
    int minimumNodeValue = m_nodeList[m_minimumNode].getValue();

    m_nodeList[nodeId].setValue(nodeValue);
	
    if (! m_nodeList[nodeId].noChild() ) {
	    int child = m_nodeList[nodeId].getChild();
	    int leftOfChild = m_nodeList[child].getLeft();
	    
	    m_nodeList[leftOfMinimumNode].setRight(child);
	    m_nodeList[child].setLeft(leftOfMinimumNode);
	    m_nodeList[m_minimumNode].setLeft(leftOfChild);
	    m_nodeList[leftOfChild].setRight(m_minimumNode);
	    
	    do {
        m_nodeList[child].setNoParent();
        child = m_nodeList[child].getRight();
	    } while ( child != m_minimumNode );

	    m_nodeList[nodeId].setNoChild();
	    m_nodeList[nodeId].setDegree(0);
	    m_nodeList[nodeId].setMark(false);
    }

    if (nodeId == m_minimumNode) {
	    minimumNodeValue = nodeValue; // Patch 1
	    do {
        currentNode = m_nodeList[currentNode].getRight();
        int value = m_nodeList[currentNode].getValue();
        if ( value < minimumNodeValue ) {
          m_minimumNode = currentNode;
          minimumNodeValue = value;
        }
	    } while ( currentNode != stopNode );
    }
  }
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FibonacciHeap)
