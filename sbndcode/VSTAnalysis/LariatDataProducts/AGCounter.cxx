////////////////////////////////////////////////////////////////
//
// Data Product for the Aerogel Cherenkov Counter
//
// Authors: UT Austin Karol Lang Group
//
//         *Dung Phan (brianp.dung@gmail.com)
//	    Will Flanagan (will.flanagan@utexas.edu)
//	    Brandon Soubasis (brandon.soubasis@gmail.com
//
////////////////////////////////////////////////////////////////

#include "AGCounter.h"
#include "cetlib/exception.h"

namespace ldp {
  
  //----------------------------------------------------------------------------
  AGCounter::AGCounter()
  {}
  
  //----------------------------------------------------------------------------
  AGCounter::~AGCounter()
  {}
  
  //----------------------------------------------------------------------------
  AGCounter::AGCounter(std::vector<ldp::AGCHits> const& AGCHits)
  : fAGCHits(AGCHits)
  {
    return;
  }
    
}