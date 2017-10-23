#include "TSBSGeant4File.h"
#include "g4sbs_types.h"
#include "fstream"

#ifndef __CINT__

// Set following variables to 1 (and recompile) t get some useful printouts
#ifndef D_FLAG
#define D_FLAG 1 //0: nothing; 1: warning; 2: debug;
#endif

TSBSGeant4File::TSBSGeant4File() : fFile(0), fSource(0) {
  fFilename[0] = '\0';
}

TSBSGeant4File::TSBSGeant4File(const char *f) : fFile(0), fSource(0) {
  //TSBSGeant4File::TSBSGeant4File(const char *f) : fFile(0), fSource(0) {
  SetFilename(f);
  fManager = TSBSDBManager::GetInstance();
  
  //Filling the table that will be used to calculate the low energy electron range in the gas. 
#if D_FLAG>1
  cout << "Initialization completed" << endl;
#endif
}

TSBSGeant4File::~TSBSGeant4File() {
  Clear();
  delete fFile;
}

void TSBSGeant4File::SetFilename( const char *f ){
  if( !f ) return;
  strcpy( fFilename, f );
}

Int_t TSBSGeant4File::Open(){
    // Return 0 on fail, 1 on success
    if( fFilename[0] == '\0' ){ return 0; }

    delete fFile;
    fFile = new TFile(fFilename);
    
    if( !fFile->IsOpen() ){ 
      fprintf(stderr, "%s: File could not be made\n",__PRETTY_FUNCTION__);
      return 0; 
    }
    
    TChain* C1 = (TChain*)fFile->Get("T");//Get the tree from the file
    
#if D_FLAG>1 
    cout << "Detector option " << fManager->Getg4sbsDetectorType() << endl;
#endif
    
    // Detector flag. See the printout content below.
    if(fManager->Getg4sbsDetectorType()<1 && fManager->Getg4sbsDetectorType()>2){
      cout << "Invalid detector option: Set correct option in db_generalinfo.dat" << endl;
      cout << "(remider: 1 - GRINCH; 2 - RICH)" << endl;
      return 0;
    }
    
    fTree = new g4sbs_tree(C1, fManager->Getg4sbsDetectorType());
    // g4sbs_tree declare all variables, branches, etc... 
    // to read, event by event, the varaibles stored in the tree. 
    // See comments in g4sbs_tree for more details...
    
    fEvNum = -1;
 
    return 1;
}

Int_t TSBSGeant4File::Close(){
    // Return 0 on fail, 1 on success
    Int_t ret = 1;
    
    if( !fFile->IsOpen() ){ return 0; }
    
    fFile->Close();
    
    delete fFile; fFile = 0;
    return ret;
}

Int_t TSBSGeant4File::ReadNextEvent(int d_flag){
  // Return 1 on success
  
  // Channel not open
  if( !fFile->IsOpen() ){ 
    fprintf(stderr, "%s %s line %d Channel not open\n",
	    __FILE__,__PRETTY_FUNCTION__,__LINE__ );
    return 0; 
  }
  
  Clear();
    
  int n_hits = 0;//total number of hits at the end of the event
  int n_gen = 0;//total number of tracks at the end of the event
  // bool newtrk, dupli;// These variables help avoid store many times the same MC track info
  bool res = false;
  
  fEvNum++;

  //cout << "Read Next Event: Evt " << fEvNum << endl;
  
  res = fTree->GetEntry(fEvNum);
  //Test that the next entry exist
  if( !res ){
    // Don't need to print this out.  Not really an error
    if(d_flag>1){
      fprintf(stderr, "%s %s line %d: Channel read return is false...  probably end of file\n",
	      __FILE__, __FUNCTION__, __LINE__ );
    } //DEBUG
    return 0;
  }
  
  double weight = fTree->ev_solang*fTree->ev_sigma; 
    
  int det_id;//2017/02/09: now corresponds to fManager->Getg4sbsDetectorType()
  
  int PMTID;
  double XPMT;
  double YPMT;
  int Npe;
  double t;
  double trms;
  int type;
  int PID_MCtrack;
  double pz_MCtrack;
  TVector3 Mom_MCtrack;
  TVector3 Vtx_MCtrack;
  TVector3 Pos_det;
  int origvolflag;
 
  double hit_data_temp[19];
  double gen_data_temp[9];
  
  // NB: See comment lines 128-129 of TSBSGeant4File.h 
  //variables for the correction of hits given by very small momenta

  switch(fManager->Getg4sbsDetectorType()){
    
  case(1)://BB GEMs
    if(d_flag>1){
      cout << "Number of Hits: " << fTree->Earm_GRINCH_hit_nhits << endl;
    } // DEBUG
    for(int i = 0; i<fTree->Earm_GRINCH_hit_nhits; i++){
      det_id = 0;
      
      type = fTree->Earm_GRINCH_hit_mTrackNo->at(i);//=1 if primary, >1 if secondary... 
      // GRINCH is supposed to ID electrons only... so a primary will always be with TrackNo = 0
      PMTID = fTree->Earm_GRINCH_hit_PMT->at(i)/5;
      XPMT = fTree->Earm_GRINCH_hit_ypmt->at(i);
      YPMT = fTree->Earm_GRINCH_hit_zpmt->at(i);// we want the PMT matrix to be sorted by increasing y_T
      Npe = fTree->Earm_GRINCH_hit_NumPhotoelectrons->at(i)*1.0e3;
      t = fTree->Earm_GRINCH_hit_Time_avg->at(i);
      trms = fTree->Earm_GRINCH_hit_Time_rms->at(i);
      //Print a warning if the hit RMS is above a few ns. This may be set in DB ?
      if(trms>2.5){
	cout << " WARNING: tRMS = " << trms 
	     << " ns > 2.5ns (~ half PMT rise time);" << endl 
	     << " digitizing such a hit may not be correct " << endl;
      }
      
      // Position of detection. In case it matters...
      Pos_det = TVector3(fTree->Earm_GRINCH_hit_xhit->at(i), // in mm
       			 fTree->Earm_GRINCH_hit_yhit->at(i), // in mm
       			 fTree->Earm_GRINCH_hit_zhit->at(i));// in mm
      
      origvolflag = fTree->Earm_GRINCH_hit_volume_flag->at(i);//=2 if produced in gas, 4 if produced in 
      
      // Stupid values for PID_MCtrack, Vtz_MCtrack and Mom_MCtrack 
      // if the MC track cannot be retrieved in the GEMs.
      PID_MCtrack = 0;
      Vtx_MCtrack = TVector3(0, // in mm
			     0, // in mm
			     1.0e9);// in mm
      Mom_MCtrack = TVector3(0, // in MeV
			     0, // in MeV
			     1.0e9);// in MeV
      
      if(origvolflag<4){// it will likely not be found if the hit is produced in the PMT glass...
	for(int j = 0; j<fTree->Earm_BBGEM_hit_nhits; j++){
	  if(fTree->Earm_BBGEM_hit_trid->at(j)==fTree->Earm_GRINCH_hit_mTrackNo->at(i)){
	    PID_MCtrack = fTree->Earm_BBGEM_hit_pid->at(j);
	    Vtx_MCtrack = TVector3(fTree->Earm_BBGEM_hit_vx->at(j)*1.0e3, // in mm
				   fTree->Earm_BBGEM_hit_vy->at(j)*1.0e3, // in mm
				   fTree->Earm_BBGEM_hit_vz->at(j)*1.0e3);// in mm
	    pz_MCtrack = sqrt( pow(fTree->Earm_BBGEM_hit_p->at(j), 2)/
			       ( pow(fTree->Earm_BBGEM_hit_txp->at(j), 2) + 
				 pow(fTree->Earm_BBGEM_hit_typ->at(j), 2) + 1.0) );
	    Mom_MCtrack = TVector3(fTree->Earm_BBGEM_hit_txp->at(j)*pz_MCtrack*1.0e3, // in MeV
				   fTree->Earm_BBGEM_hit_typ->at(j)*pz_MCtrack*1.0e3, // in MeV
				   pz_MCtrack*1.0e3);// in MeV
	    break;
	  }
	}
      }

      //Filling hit_data temporary array...
      //hit_data_temp[0] = (double)PMTrow;
      //hit_data_temp[1] = (double)PMTcol;
      hit_data_temp[0] = (double)PMTID;
      hit_data_temp[1] = XPMT;
      hit_data_temp[2] = YPMT;      
      hit_data_temp[3] = Npe;
      hit_data_temp[4] = t;
      hit_data_temp[5] = trms;
      hit_data_temp[6] = (double)type;
      hit_data_temp[10] = (double)PID_MCtrack;
      hit_data_temp[17] = (double)origvolflag;
      for(int k = 0; k<3; k++){
	hit_data_temp[k+7] = Pos_det[k];
	hit_data_temp[k+11] = Mom_MCtrack[k];
	hit_data_temp[k+14] = Vtx_MCtrack[k];
      }
      
      fg4sbsHitData.push_back(new g4sbshitdata(det_id, data_size(__CER_TAG)));

      // ... to copy it in the actual g4sbsHitData structure.
      for(int j = 0; j<18; j++){
	fg4sbsHitData[n_hits]->SetData(j, hit_data_temp[j]);
      }
      n_hits++;
      
      //Filling gen_data temporary array...
      gen_data_temp[0] = type;
      gen_data_temp[1] = PID_MCtrack;
      for(int k = 0; k<3; k++){
	gen_data_temp[k+2] = Mom_MCtrack[k];
	gen_data_temp[k+5] = Vtx_MCtrack[k];
      }
      //gen_data_temp[7] = origvolflag;
      gen_data_temp[8] = weight;
      
      /*
      //store information to rescue, if necessary, the generated info
      if(fTree->Earm_BBGEM_hit_p->at(i)>pmax && pid==fManager->GetSigPID(0)){
      	pmax = fTree->Earm_BBGEM_hit_p->at(i);
      	for(int k = 0; k<9; k++){
      	  gen_data_temp_max[k] = gen_data_temp[k];
      	}
      }
      */
      // ... to copy it in the actual g4sbsGenData structure.
      // only store signal, primary MC tracks
      if(fSource==0 && n_gen==0 && type==1){
      	fg4sbsGenData.push_back(new g4sbsgendata());
      	for(int j = 0; j<9; j++){
	  //cout << j << " " << gen_data_temp[j] << endl;
      	  fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
      	}
      	n_gen++;
      }/*
      else{// this determines if the track is new or not
	newtrk = true; 
	for(int z = n_gen-1; z>=0; z--){
	dupli = true;
	if(fg4sbsGenData[z]->GetData(1)!=gen_data_temp[1]){
	dupli=false;
	}else{
	for(int j = 5; j<8; j++){
	if(fg4sbsGenData[z]->GetData(j)!=gen_data_temp[j]){
	dupli=false;
	break;
	}
	}
	}
	if(dupli){
	newtrk = false;
	break;
	}
	}
	
	if(newtrk){
	fg4sbsGenData.push_back(new g4sbsgendata());
	for(int j = 0; j<9; j++){
	fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
	}
	n_gen++;
	}
	}
      */
      
    //   if(d_flag>1){
    // 	cout << "Hit number: " << i << " BBGEM: X_global : " 
    // 	     << fTree->Earm_BBGEM_hit_xg->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_yg->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_zg->at(i) << endl;
    // 	cout << "X_local (g4sbs): " << fTree->Earm_BBGEM_hit_tx->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_ty->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_z->at(i) << endl;
    // 	cout << "detector ID: " << det_id << ", plane: " << plane << ", sector: " << sector << endl
    // 	     << "particle ID: " << pid << ", type (1, primary, >1 secondary): " << type << endl
    // 	     << "energy deposit (eV): " << edep << endl;
    // 	cout << "Momentum (MeV): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << Mom[k] << ", ";
    // 	}
    // 	cout << " norm " << fTree->Earm_BBGEM_hit_p->at(i) << endl;
    // 	cout << "hit position at drift entrance (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_in[k] << ", ";
    // 	}
    // 	cout << " time : " << tmin << endl;
    // 	cout << "hit position at drift exit (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_out[k] << " ";
    // 	}
    // 	cout << " time : " << tmax << endl;
    // 	cout << "hit position at readout (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_RO[k] << ", ";
    // 	}
    // 	cout << endl;
    // 	cout << "Vertex position (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << Vtx[k] << ", ";
    // 	}
    // 	cout << endl;
    //   } //DEBUG          
    // }//end loop on hits
    
    // // Rescue ngen data here...
    // // ngen data is rescued if: 
    // // * the file read is signal; 
    // // * there is no gen data stored already; 
    // // * and there are at least 3 hits associated to the rescued particle
    // if(fSource==0 && n_gen==0 && n_hits>=3){
    //   int n_hits_max = 0;
    //   for(UInt_t k = 0; k<trid_hits.size(); k++){
    // 	if(gen_data_temp_max[0]==trid_hits[k])n_hits_max++;
    //   }
    //   if(n_hits_max>=3){
    // 	fg4sbsGenData.push_back(new g4sbsgendata());
    // 	for(int j = 0; j<9; j++){
    // 	  fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
    // 	}
    // 	n_gen++;
    //   }
    }
    break;
  case(2)://RICH
    if(d_flag>1){
      cout << "Number of Hits: " << fTree->Harm_RICH_hit_nhits << endl;
    } // DEBUG
    for(int i = 0; i<fTree->Harm_RICH_hit_nhits; i++){
      det_id = 0;
      
      type = fTree->Harm_RICH_hit_mTrackNo->at(i);//=1 if primary, >1 if secondary... 
      // TODO: modify the particle type according to the particle PID...
      PMTID = fTree->Harm_RICH_hit_PMT->at(i);
      XPMT = fTree->Harm_RICH_hit_ypmt->at(i);
      YPMT = -fTree->Harm_RICH_hit_xpmt->at(i);// we want the PMT matrix to be sorted by increasing y_T
      Npe = fTree->Harm_RICH_hit_NumPhotoelectrons->at(i)*1.0e3;
      t = fTree->Harm_RICH_hit_Time_avg->at(i);
      trms = fTree->Harm_RICH_hit_Time_rms->at(i);
      //Print a warning if the hit RMS is above a few ns. This may be set in DB ?
      if(trms>2.5){
	cout << " WARNING: tRMS = " << trms 
	     << " ns > 2.5ns (~ PMT rise time);" << endl 
	     << " digitizing such a hit may not be correct " << endl;
      }
      
      // Position of detection. In case it matters...
      Pos_det = TVector3(fTree->Harm_RICH_hit_xhit->at(i)*1.0e3, // in mm
       			 fTree->Harm_RICH_hit_yhit->at(i)*1.0e3, // in mm
       			 fTree->Harm_RICH_hit_zhit->at(i)*1.0e3);// in mm
      
      origvolflag = fTree->Harm_RICH_hit_volume_flag->at(i);//=2 if produced in gas, 4 if produced in 
      
      // Stupid values for PID_MCtrack, Vtz_MCtrack and Mom_MCtrack 
      // if the MC track cannot be retrieved in the GEMs.
      PID_MCtrack = 0;
      Vtx_MCtrack = TVector3(0, // in mm
			     0, // in mm
			     1.0e9);// in mm
      Mom_MCtrack = TVector3(0, // in MeV
			     0, // in MeV
			     1.0e9);// in MeV
      
      if(origvolflag<4){// it will likely not be found if the hit is produced in the PMT glass...
	for(int j = 0; j<fTree->Earm_BBGEM_hit_nhits; j++){
	  if(fTree->Harm_SBSGEM_hit_trid->at(j)==fTree->Harm_RICH_hit_mTrackNo->at(i)){
	    PID_MCtrack = fTree->Harm_SBSGEM_hit_pid->at(j);
	    Vtx_MCtrack = TVector3(fTree->Harm_SBSGEM_hit_vx->at(j)*1.0e3, // in mm
				   fTree->Harm_SBSGEM_hit_vy->at(j)*1.0e3, // in mm
				   fTree->Harm_SBSGEM_hit_vz->at(j)*1.0e3);// in mm
	    pz_MCtrack = sqrt( pow(fTree->Harm_SBSGEM_hit_p->at(j), 2)/
			       ( pow(fTree->Harm_SBSGEM_hit_txp->at(j), 2) + 
				 pow(fTree->Harm_SBSGEM_hit_typ->at(j), 2) + 1.0) );
	    Mom_MCtrack = TVector3(fTree->Harm_SBSGEM_hit_txp->at(j)*pz_MCtrack*1.0e3, // in MeV
				   fTree->Harm_SBSGEM_hit_typ->at(j)*pz_MCtrack*1.0e3, // in MeV
				   pz_MCtrack*1.0e3);// in MeV
	    break;
	  }
	}
      }

      //Filling hit_data temporary array...
      //hit_data_temp[0] = (double)PMTrow;
      //hit_data_temp[1] = (double)PMTcol;
      hit_data_temp[0] = (double)PMTID;
      hit_data_temp[1] = XPMT;
      hit_data_temp[2] = YPMT;      
      hit_data_temp[3] = Npe;
      hit_data_temp[4] = t;
      hit_data_temp[5] = trms;
      hit_data_temp[6] = (double)type;
      hit_data_temp[10] = (double)PID_MCtrack;
      hit_data_temp[17] = (double)origvolflag;
      for(int k = 0; k<3; k++){
	hit_data_temp[k+7] = Pos_det[k];
	hit_data_temp[k+11] = Mom_MCtrack[k];
	hit_data_temp[k+14] = Vtx_MCtrack[k];
      }
      
      fg4sbsHitData.push_back(new g4sbshitdata(det_id, data_size(__CER_TAG)));

      // ... to copy it in the actual g4sbsHitData structure.
      for(int j = 0; j<18; j++){
	fg4sbsHitData[n_hits]->SetData(j, hit_data_temp[j]);
      }
      n_hits++;
      
      //Filling gen_data temporary array...
      gen_data_temp[0] = type;
      gen_data_temp[1] = PID_MCtrack;
      for(int k = 0; k<3; k++){
	gen_data_temp[k+2] = Mom_MCtrack[k];
	gen_data_temp[k+5] = Vtx_MCtrack[k];
      }
      //gen_data_temp[7] = origvolflag;
      gen_data_temp[8] = weight;
      
      /*
      //store information to rescue, if necessary, the generated info
      if(fTree->Earm_BBGEM_hit_p->at(i)>pmax && pid==fManager->GetSigPID(0)){
      	pmax = fTree->Earm_BBGEM_hit_p->at(i);
      	for(int k = 0; k<9; k++){
      	  gen_data_temp_max[k] = gen_data_temp[k];
      	}
      }
      */
      // ... to copy it in the actual g4sbsGenData structure.
      // only store signal, primary MC tracks
      if(fSource==0 && n_gen==0 && type==1){
      	fg4sbsGenData.push_back(new g4sbsgendata());
      	for(int j = 0; j<9; j++){
      	  fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
      	}
      	n_gen++;
      }/*
      else{// this determines if the track is new or not
	newtrk = true; 
	for(int z = n_gen-1; z>=0; z--){
	dupli = true;
	if(fg4sbsGenData[z]->GetData(1)!=gen_data_temp[1]){
	dupli=false;
	}else{
	for(int j = 5; j<8; j++){
	if(fg4sbsGenData[z]->GetData(j)!=gen_data_temp[j]){
	dupli=false;
	break;
	}
	}
	}
	if(dupli){
	newtrk = false;
	break;
	}
	}
	
	if(newtrk){
	fg4sbsGenData.push_back(new g4sbsgendata());
	for(int j = 0; j<9; j++){
	fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
	}
	n_gen++;
	}
	}
      */
      
    //   if(d_flag>1){
    // 	cout << "Hit number: " << i << " BBGEM: X_global : " 
    // 	     << fTree->Earm_BBGEM_hit_xg->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_yg->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_zg->at(i) << endl;
    // 	cout << "X_local (g4sbs): " << fTree->Earm_BBGEM_hit_tx->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_ty->at(i) << ", " 
    // 	     << fTree->Earm_BBGEM_hit_z->at(i) << endl;
    // 	cout << "detector ID: " << det_id << ", plane: " << plane << ", sector: " << sector << endl
    // 	     << "particle ID: " << pid << ", type (1, primary, >1 secondary): " << type << endl
    // 	     << "energy deposit (eV): " << edep << endl;
    // 	cout << "Momentum (MeV): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << Mom[k] << ", ";
    // 	}
    // 	cout << " norm " << fTree->Earm_BBGEM_hit_p->at(i) << endl;
    // 	cout << "hit position at drift entrance (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_in[k] << ", ";
    // 	}
    // 	cout << " time : " << tmin << endl;
    // 	cout << "hit position at drift exit (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_out[k] << " ";
    // 	}
    // 	cout << " time : " << tmax << endl;
    // 	cout << "hit position at readout (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << X_RO[k] << ", ";
    // 	}
    // 	cout << endl;
    // 	cout << "Vertex position (mm): ";
    // 	for(int k = 0; k<3; k++){
    // 	  cout << Vtx[k] << ", ";
    // 	}
    // 	cout << endl;
    //   } //DEBUG          
    // }//end loop on hits
    
    // // Rescue ngen data here...
    // // ngen data is rescued if: 
    // // * the file read is signal; 
    // // * there is no gen data stored already; 
    // // * and there are at least 3 hits associated to the rescued particle
    // if(fSource==0 && n_gen==0 && n_hits>=3){
    //   int n_hits_max = 0;
    //   for(UInt_t k = 0; k<trid_hits.size(); k++){
    // 	if(gen_data_temp_max[0]==trid_hits[k])n_hits_max++;
    //   }
    //   if(n_hits_max>=3){
    // 	fg4sbsGenData.push_back(new g4sbsgendata());
    // 	for(int j = 0; j<9; j++){
    // 	  fg4sbsGenData[n_gen]->SetData(j, gen_data_temp[j]);
    // 	}
    // 	n_gen++;
    //   }
    }
    break;
    
  }//end switch(fManager->Getg4sbsDetectorType)
    
  return 1;
}


void TSBSGeant4File::Clear(){
  // Clear out hit and generated data

#if D_FLAG>1
  fprintf(stderr, "%s %s line %d: Deleting hits\n",
	  __FILE__, __FUNCTION__, __LINE__);
#endif //DEBUG

  unsigned int i;
  for( i = 0; i < fg4sbsHitData.size(); i++ ){
    delete fg4sbsHitData[i];
  }

  for( i = 0; i < fg4sbsGenData.size(); i++ ){
    delete fg4sbsGenData[i];
  }

  fg4sbsHitData.clear();
  fg4sbsGenData.clear();

#if D_FLAG>1
  fprintf(stderr, "%s %s line %d: Hits deleted\n",
	  __FILE__, __FUNCTION__, __LINE__);
#endif //DEBUG

  return;
}

TSBSCherData* TSBSGeant4File::GetCherData()
{
  // Return TSBSCherData object filled with GEM data of present event.
  // The returned object pointer must be deleted by the caller!

  TSBSCherData* gd = new TSBSCherData();

  GetCherData(gd);
  return gd;
}

void TSBSGeant4File::GetCherData(TSBSCherData* gd)
{
  // Pack data into TSBSCherData
   
  //    printf("NEXT EVENT ---------------------------\n");

  if( !gd ) return;
  gd->ClearEvent();
  gd->SetSource(fSource);
  gd->SetEvent(fEvNum);
  
  if (GetNData() == 0) {
    return;
  }
  gd->InitEvent(GetNData());

  g4sbshitdata *h;//, *hs;
  //bool matchedstrip;
  unsigned int i, ngdata = 0;// j,
  for(i=0; i<GetNData(); i++){
    h = GetHitData(i);

    if( h->GetData(1)>0.0 ){

      gd->SetHitDetID(ngdata,      (UInt_t)h->GetDetID() );
      gd->SetHitPMTID(ngdata,      (UInt_t)h->GetData(0) );
      gd->SetHitXPMT(ngdata,               h->GetData(1) ); 
      gd->SetHitYPMT(ngdata,               h->GetData(2) ); 
      gd->SetHitPEyield(ngdata,            h->GetData(3) ); 
      gd->SetHitTime(ngdata,               h->GetData(4) ); 
      gd->SetHitTimeRMS(ngdata,            h->GetData(5) ); 
      gd->SetParticleType(ngdata,  (UInt_t)h->GetData(6) );//  Track type (1 primary, >1 secondary) 
      gd->SetMCtrackPID(ngdata,     (Int_t)h->GetData(10));
      gd->SetOriginVolFlag(ngdata, (UInt_t)h->GetData(15));
      
      // Vector information
      TVector3 X_det(h->GetData(7), h->GetData(8), h->GetData(9));
      gd->SetPositionDet(ngdata, X_det);
      
      TVector3 p_mctrk(h->GetData(11), h->GetData(12), h->GetData(13));
      gd->SetMCtrackMomentum(ngdata, p_mctrk);

      TVector3 v_mctrk(h->GetData(14), h->GetData(15), h->GetData(16));
      gd->SetMCtrackVertex(ngdata, v_mctrk);
      
      ngdata++;
    }
  }
  gd->SetNHit(ngdata);
}


///////////////////////////////////////////////////////////////
// hitdata classes 

g4sbshitdata::g4sbshitdata(int detid, unsigned int size ){
  fDetID = detid;
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

#endif//__CINT__
