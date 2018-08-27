//*-- Author :    Ole Hansen (ole@jlab.org)    9-Dec-2011

/////////////////////////////////////////////////////////////////////
//
//   TSBSSimDecoder
//
//   Decoder for SoLID simulation data
//
//   Interprets event buffer from input as TSBSSimEvent objects
//   (containing digitized simulation data) and unpacks them into
//   crateslot arrays for low-level decoding by detectors.
//
/////////////////////////////////////////////////////////////////////

#include "TSBSSimDecoder.h"
#include "THaCrateMap.h"
#include "THaBenchmark.h"
#include "VarDef.h"
#include "TSBSDBManager.h"
#include "THaSlotData.h"

#include "TError.h"
#include "TSystem.h"
#include "TMath.h"
#include "TDatabasePDG.h"
#include "TRandom.h"
#include "THaVarList.h"

#include <SBSSimFadc250Module.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <utility>
#include <stdexcept>

using namespace std;
using namespace Podd;

//EFuchey: 2016/12/10: it is necessary to declare the TSBSDBManager as a static instance here 
// (and not make it a member) because it is used by functions whic are defined as "static inline".
static TSBSDBManager* fManager = TSBSDBManager::GetInstance();
static const Int_t kPrimaryType = 1, kPrimarySource = 0;
// Projection types must match the definitions in TreeSearch
enum EProjType { kUPlane = 0, kVPlane =1, kXPlane = 2, kYPlane = 3};

typedef vector<int>::size_type vsiz_t;

//-----------------------------------------------------------------------------
TSBSSimDecoder::TSBSSimDecoder()
{
  // Constructor
  DefineVariables();

  gSystem->Load("libEG.so");  // for TDatabasePDG
}

//-----------------------------------------------------------------------------
TSBSSimDecoder::~TSBSSimDecoder() {

  DefineVariables( THaAnalysisObject::kDelete );
  
}

//-----------------------------------------------------------------------------
Int_t TSBSSimDecoder::DefineVariables( THaAnalysisObject::EMode mode )
{
  // Define global variables for the MC quantities. Extends the base
  // class method.

  const char* const here = "TSBSSimDecoder::DefineVariables";
  
  if( mode == THaAnalysisObject::kDefine && fIsSetup )
    return THaAnalysisObject::kOK;
  
  SimDecoder::DefineVariables( mode );
  
  cout << "Read TSBSSimDecoder variables " << endl;
  
  RVarDef vars[] = {
    { 0 }
  };

  return THaAnalysisObject::
    DefineVarsFromList( vars, THaAnalysisObject::kRVarDef,
			mode, "", this, MC_PREFIX, here );
}

//-----------------------------------------------------------------------------
void TSBSSimDecoder::Clear( Option_t* opt )
{
  // Clear track and plane data

  SimDecoder::Clear(opt);   // clears fMCCherHits, fMCCherClus
  
  fPMTMap.clear(); 
}

//-----------------------------------------------------------------------------
#if ANALYZER_VERSION_CODE >= ANALYZER_VERSION(1,6,0)
int TSBSSimDecoder::LoadEvent(const UInt_t* evbuffer )
#else
int TSBSSimDecoder::LoadEvent(const Int_t* evbuffer )
#endif
{
  // Wrapper around DoLoadEvent so we can conveniently stop the benchmark
  // counter in case of errors

  int ret = DoLoadEvent( evbuffer );

  if( fDoBench ) fBench->Stop("physics_decode");

  return ret;
}

//-----------------------------------------------------------------------------
static inline
void PMTtoROC( Int_t h_chan,
	       Int_t& crate, Int_t& slot, Int_t& chan )
{
  // Convert location parameters (row, col, chan) of the given PMT
  // to hardware channel (crate,slot,chan)
  // The (crate,slot,chan) assignment must match the detmap definition in
  // the database!  See TreeSearch/dbconvert.cxx
  // In the case of GRINCH/RICH: 
  // crate = GTP; slot = VETROC; chan = PMT. (NINOs are "transparent", in a similar way to the MPDs)
  
  div_t d = div( h_chan, fManager->GetChanPerSlot() );
  slot = d.quot;
  chan = d.rem;

  d = div( slot, fManager->GetSlotPerCrate() );
  crate = d.quot;
  slot  = d.rem;
}

//-----------------------------------------------------------------------------
static inline
Int_t MakeROCKey( Int_t crate, Int_t slot, Int_t chan )
{
  return chan +
    fManager->GetChanPerSlot()*( slot + fManager->GetSlotPerCrate()*crate );
}

//-----------------------------------------------------------------------------
Int_t TSBSSimDecoder::PMTfromROC( Int_t crate, Int_t slot, Int_t chan ) const
{
  // Return index of digitized strip correspomding to hardware channel
  // (crate,slot,chan)

  if( fPMTMap.empty() )
    return -1;

  PMTMap_t::const_iterator found = fPMTMap.find( MakeROCKey(crate,slot,chan) );
  if( found == fPMTMap.end() )
    return -1;

  return found->second;
}

//-----------------------------------------------------------------------------
static inline Int_t NumberOfSetBits( UInt_t v )
{
  // Count number of bits set in 32-bit integer. From
  // http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel

  v = v - ((v >> 1) & 0x55555555);
  v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
  return (((v + (v >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

//-----------------------------------------------------------------------------
#if ANALYZER_VERSION_CODE >= ANALYZER_VERSION(1,6,0)
Int_t TSBSSimDecoder::DoLoadEvent(const UInt_t* evbuffer )
#else
Int_t TSBSSimDecoder::DoLoadEvent(const Int_t* evbuffer )
#endif
{
  // Fill crateslot structures with Monte Carlo event data in 'evbuffer'

  static const char* const here = "TSBSSimDecoder::LoadEvent";

#if ANALYZER_VERSION_CODE < ANALYZER_VERSION(1,6,0)
  Bool_t fNeedInit = fgNeedInit;
#endif
  assert( fMap || fNeedInit );

  // Local copy of evbuffer pointer, used in GetMCHitInfo
  buffer = evbuffer;

  // Cast the evbuffer pointer back to exactly the event type that is present
  // in the input file (in TSBSSimFile). The pointer-to-unsigned integer is
  // needed compatibility with the standard decoder.
  const TSBSSimEvent* simEvent = reinterpret_cast<const TSBSSimEvent*>(buffer);
  
  Int_t ret = HED_OK;
  if (first_decode || fNeedInit) {
    fMap->print();
    if( (ret = init_cmap()) != HED_OK )
      return ret;
#if ANALYZER_VERSION_CODE < ANALYZER_VERSION(1,6,0)
    if( (ret = init_slotdata(fMap)) != HED_OK)
#else
    if( (ret = init_slotdata()) != HED_OK)
#endif
      return ret;
    first_decode = false;
  }

  if( fDoBench ) fBench->Begin("clearEvent");
  Clear();
  for( int i=0; i<fNSlotClear; i++ )
    crateslot[fSlotClear[i]]->clearEvent();
  if( fDoBench ) fBench->Stop("clearEvent");

  // FIXME: needed?
  evscaler = 0;
  event_length = 0;
  
  event_type = 1;
  event_num = simEvent->fEvtID;
  recent_event = event_num;

  // Event weight
  fWeight = simEvent->fWeight;

  //
  if( fDoBench ) fBench->Begin("physics_decode");
  
  
  Bool_t newclus;
  Int_t crate, slot, chan;

  std::map<Decoder::THaSlotData*, std::vector<UInt_t> > hcalmap;
  std::cerr << "\n\n\n\n\nStart Processing event: " << event_num << std::endl;
  for(std::vector<TSBSSimEvent::DetectorData>::const_iterator it =
      simEvent->fDetectorData.begin(); it != simEvent->fDetectorData.end();
      ++it )
  {
    if((*it).fDetID == 2 && (*it).fData.size() > 0) { // HCal
      //crate = (*it).fCrate;
      //slot  = (*it).fSlot;
      //chan  = (*it).fChannel;
      int mod =  (*it).fChannel;
      if(mod < 192) {
        crate = 10;
        slot = 4+(mod/16);
      } else {
        crate = 11;
        slot = 4+(mod-192)/16;
      }
      if( slot == 11 || slot == 12) {
        slot += 2;
      }
      chan = mod%16;
      Decoder::THaSlotData *sldat = crateslot[idx(crate,slot)];
      if(sldat) { // meaning the module is available
        //if((*it).fData[0] == 0 || (*it).fData[0] == 1) { // fADC
          std::vector<UInt_t> *myev = &(hcalmap[sldat]);
          myev->push_back(chan);
          for(size_t k = 0; k < (*it).fData.size(); k++) {
            myev->push_back((*it).fData[k]);
          }
      }
      if((*it).fData[0] == 1) {
        std::cerr << "M: " << mod << ", C: " << crate << ", S: " << slot
          << ", C: " << chan << ", I: " << (*it).fData[2] << std::endl;
      }
    }
  }

  // Now that all data is prepared, loop through the hcal map and load
  // the appropriate slots
  std::cerr << "HCal hits: " << hcalmap.size() << std::endl;
  for( std::map<Decoder::THaSlotData*, std::vector<UInt_t> >::iterator it =
      hcalmap.begin(); it != hcalmap.end(); ++it) {
    it->first->GetModule()->LoadSlot(it->first,
        it->second.data(),0,it->second.size() );
        //myevbuff.resize((*it).fData.size());
        //myevbuff[0] = chan; // Only value that needs to be changed
        //for(size_t k = 1; k < myevbuff.size(); k++) {
        //  myevbuff[k] = (*it).fData[k];
        //}
        //if(myevbuff.size() > 1) { // i.e. have actual data
        //  std::cerr << "Det data, mod: " << mod << ", Crate: " << crate
        //    << ", slot=" << slot << ", chan=" << chan
        //    << ", detID: " << (*it).fDetID
        //    << ", size: " << myevbuff.size() << std::endl;
        //  sldat->GetModule()->LoadSlot(sldat,myevbuff.data(),0,myevbuff.size());
        //}
  }
  
  std::cerr << "End Processing event:   " << event_num << std::endl;
  return HED_OK;
}


