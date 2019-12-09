#include "TSBSSimData.h"
#include <stdlib.h>

///////////////////////////////////////////////////////////////
// hitdata classes 

g4sbshitdata::g4sbshitdata(int unique_detid, unsigned int size ){
  fDetID = unique_detid%10;
  fDetType = (det_type)((unique_detid-fDetID)/10);//Should assign the right det flag to the right 
  // again: kHCal: 0, kECal: 1, kCher: 3, kScint: 3, kGEM: 4;
  fData  = new double[size];
  fSize  = size;
  fFillbits = 0;
  
  if( size > sizeof( long long int )*8 ){
    fprintf(stderr, "%s %s line %d:  Error:  Event size too long for bit pattern storage (requested %d, have %ld)\n",
	    __FILE__, __PRETTY_FUNCTION__, __LINE__, size, 
	    sizeof(long long int)*8);
    exit(1);
  }
  
  // There is no value indexed at 0, so we'll just set it to 0 for
  // sanity's sake and not deal with crazy offsets all over
  
  fFillbits |= 1;
  fData[0] = 3.1415927;
}

void g4sbshitdata::SetData(unsigned int idx, double data ){
  if( idx >= fSize ){
    fprintf(stderr, "%s %s line %d:  Error:  index out of range (%d oor of size %d)\n",__FILE__, __PRETTY_FUNCTION__, __LINE__, idx, fSize);
    return;

  }

  fFillbits |= (1<<idx);

  fData[idx] = data;
  return;
}

double g4sbshitdata::GetData(unsigned int idx) const {
  if( idx >= fSize ){
    fprintf(stderr, "%s %s line %d:  Error:  index out of range (%d oor of size %d)\n",__FILE__, __PRETTY_FUNCTION__, __LINE__, idx, fSize);
    return 1e9;
  }

  if( !(fFillbits & (1<<idx)) ){
    fprintf(stderr, "%s %s line %d:  Error:  Accessing unset data (idx %d) val: %f\n",__FILE__, __PRETTY_FUNCTION__, __LINE__, idx, fData[idx] );
    return 1e9;
  }

  return fData[idx];
}

bool g4sbshitdata::IsFilled() const {
  if( fFillbits == ((1<<fSize) - 1) ){
	return true;
    }

    return false;
}

g4sbshitdata::~g4sbshitdata(){
    delete fData;
}

///////////////////////////////////////////////////////////////
// gendata classes

// Size is 1 bigger because we are also including the weight
// Set that default to 1
g4sbsgendata::g4sbsgendata():g4sbshitdata(-1, __GENERATED_SIZE+2){
    SetData(8,1.0);
}


//
// Output data classes
//

simdig_outdata::simdig_outdata(){
  Clear();
}

simdig_outdata::~simdig_outdata(){
  Clear();
}

void simdig_outdata::Clear(){
  fNHits = 0;
  fChannel.clear();
  fADC.clear();
  fTDC_L.clear();
  fTDC_T.clear();
}

bool simdig_outdata::CheckSize(bool ignore_adc,
			       bool ignore_tdc,
			       bool print){
  bool checkout = true;
  if(fChannel.size()!=fNHits){
    checkout = false;
    if(print)printf("fChannel.size() = %zu != %u\n", fChannel.size(), fNHits);
  }
  if(fDataWord.size()!=fNHits){
    checkout = false;
    if(print)printf("fDataWord.size() = %zu != %u\n", fDataWord.size(), fNHits);
  }
  if(!ignore_adc && fADC.size()!=fNHits){
    checkout = false;
    if(print)printf("fADC.size() = %zu != %u\n", fADC.size(), fNHits);
  }
  if(!ignore_tdc && fTDC_L.size()!=fNHits){
    checkout = false;
    if(print)printf("fADC_L.size() = %zu != %u\n", fTDC_L.size(), fNHits);
  }
  if(!ignore_tdc && fTDC_T.size()!=fNHits){
    checkout = false;
    if(print)printf("fTDC_T.size() = %zu != %u\n", fTDC_T.size(), fNHits);
  }
  
  return checkout;
}

simdigsamp_outdata::simdigsamp_outdata(){
  Clear();
}

simdigsamp_outdata::~simdigsamp_outdata(){
  Clear();
}

void simdigsamp_outdata::Clear(){
  simdig_outdata::Clear();
  fSamp.clear();
}

bool simdigsamp_outdata::CheckSize(bool ignore_adc,
				    bool ignore_tdc,
				    bool print){
  bool checkout = simdig_outdata::CheckSize(ignore_adc, ignore_tdc, print);
  if(fSamp.size()!=fNHits){
    checkout = false;
    if(print)printf("fChannel.size() = %zu != %u\n", fChannel.size(), fNHits);
  }
  
  return checkout;
}



simgemdig_outdata::simgemdig_outdata(){
  Clear();
}

simgemdig_outdata::~simgemdig_outdata(){
  Clear();
}

void simgemdig_outdata::Clear(){
  simdig_outdata::Clear();
  
  fPlane.clear();
  fModule.clear();
  fProj.clear();
  fSamp.clear();
}
