#ifndef __TSBSCHER_H
#define __TSBSCHER_H

#include "THaDetector.h"

class THaEvData;

// In the SBS geometry, a chamber is a box (as defined it TSBSBox class). 
// Refer to the comment in the header of TSBSBox class for more info.

// It uses many of the methods from this class (namely the transformations methods),
// which it completes by offering the options to use these methods
// with a TVector3 object as an input instead of 3 doubles.

// TSBSCher also inherits form THaDetector, which grants it all the functions from its class
// (see http://hallaweb.jlab.org/podd/doc/html_v16/ClassIndex.html for more info).

class TSBSCher : public THaDetector {
 public:
  //Constructors and destructor
  TSBSCher(const char *name, const char *desc);//It is recommended to use this constructor
  TSBSCher() : fNPMTs(0), fNPMTrows(0), fNPMTcolsMax(0) {} // for ROOT RTTI

  virtual ~TSBSCher();
  
  //Read the geometry for the TSBSBox in the data base
  Int_t ReadDatabase (const TDatime& date);
  Int_t ReadGeometry (FILE* file, const TDatime& date, Bool_t required);
  const char* GetDBFileName() const;

  Int_t Decode( const THaEvData & );
  
  // Access to the info of TSBSGEMPlane which is regarded as a subdetector of TSBSCher.
  // (see comments in the code of class TSBSGEMPlane)
  UInt_t GetNPMTs() const {return fNPMTs;};
  UInt_t GetNPMTrows() const {return fNPMTrows;};
  UInt_t GetNPMTcolsMax() const {return fNPMTcolsMax;};
  
  void Print ();
  
 private:
  int    fNPMTs;         // number of PMTs
  int    fNPMTrows;      // number of PMT rows
  int    fNPMTcolsMax;   // max number of PMT columns 
  double fPMTmatrixHext; // horizontal extension, in m, of the PMT matrix (from lower PMT center to higher PMT center)
  double fPMTmatrixVext; // vertical extension, in m, of the PMT matrix (from left PMT center to right PMT center)
  double fInterPMTDist;  // center to center PMT distance, in m
  double fX_TCPMT;       // X position of the top close PMT center in the PMT matrix (transport coord)
  double fY_TCPMT;       // Y position of the top close PMT center in the PMT matrix (transport coord)

  ClassDef(TSBSCher,0)

};

#endif//__TSBSCHER_H