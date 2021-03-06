/**
 * This macro takes two histogram files and compares them.
 * It produces a "compared.root" file with the corresponding
 * histograms superimposed (2nd in red).
 * It also gives an output with the result of the difference between
 * the corresponding histograms. If everything is zero, the two files
 * have the same histograms.
 */

// Needed to use gROOT in a compiled macro
#include "TROOT.h"

#include <string.h>
#include "TChain.h"
#include "TFile.h"
#include "TH1.h"
#include "TTree.h"
#include "TKey.h"
#include "Riostream.h"
#include "THStack.h"
#include "TCanvas.h"

TList *FileList;
TFile *Target;
TFile *DiffTarget;
unsigned int countDifferences = 0;
unsigned int totalComparedHistograms = 0;

void CompareRootfile( TDirectory *target, TDirectory *diffTarget, TList *sourcelist );

void CompareHistograms() {
  // in an interactive ROOT session, edit the file names
  // Target and FileList, then
  // root > .L hadd.C
  // root > hadd()

  gROOT->SetBatch(true);

  Target = TFile::Open( "compared.root", "RECREATE" );
  DiffTarget = TFile::Open( "differencies.root", "RECREATE" );

  FileList = new TList();

  // ************************************************************
  // List of Files
  FileList->Add( TFile::Open("Default/4.17_RunMinBias2011A+RunMinBias2011A+RECOD+HARVESTD+SKIMD/DQM_V0001_R000165121__Global__CMSSW_X_Y_Z__RECO.root") );    // 1
  // FileList->Add( TFile::Open("Default/AnotherTest/4.17_RunMinBias2011A+RunMinBias2011A+RECOD+HARVESTD+SKIMD/DQM_V0001_R000165121__Global__CMSSW_X_Y_Z__RECO.root") );    // 1
  FileList->Add( TFile::Open("RunInfoRemoval/4.17_RunMinBias2011A+RunMinBias2011A+RECOD+HARVESTD+SKIMD/DQM_V0001_R000165121__Global__CMSSW_X_Y_Z__RECO.root") );    // 2
  // FileList->Add( TFile::Open("SiStripDetCablingChange/4.17_RunMinBias2011A+RunMinBias2011A+RECOD+HARVESTD+SKIMD/DQM_V0001_R000165121__Global__CMSSW_X_Y_Z__RECO.root") );    // 2
  // FileList->Add( TFile::Open("CrossCheck/4.17_RunMinBias2011A+RunMinBias2011A+RECOD+HARVESTD+SKIMD/DQM_V0001_R000165121__Global__CMSSW_X_Y_Z__RECO.root") );    // 2

  CompareRootfile( Target, DiffTarget, FileList );

  cout << endl << "Number of histograms compared: " << totalComparedHistograms << endl;
  cout << endl << "Number of different histograms: " << countDifferences << endl;
}

void CompareRootfile( TDirectory *target, TDirectory *diffTarget, TList *sourcelist ) {

  // Create a THStack to draw the histograms superimposed
  THStack * comp = 0;

  //  cout << "Target path: " << target->GetPath() << endl;
  TString path( (char*)strstr( target->GetPath(), ":" ) );
  path.Remove( 0, 2 );

  TFile *first_source = (TFile*)sourcelist->First();

  first_source->cd( path );
  TDirectory *current_sourcedir = gDirectory;
  //gain time, do not add the objects in the list in memory
  Bool_t status = TH1::AddDirectoryStatus();
  TH1::AddDirectory(kFALSE);

  bool wasDifferent = false;

  // loop over all keys in this directory
  TIter nextkey( current_sourcedir->GetListOfKeys() );
  TKey *key, *oldkey=0;
  while ( (key = (TKey*)nextkey())) {

    wasDifferent = false;
    //keep only the highest cycle number for each key
    if (oldkey && !strcmp(oldkey->GetName(),key->GetName())) continue;

    // read object from first source file
    first_source->cd( path );
    TObject *obj = key->ReadObj();

    if ( obj->IsA()->InheritsFrom( "TH1" ) ) {
      // descendant of TH1 -> merge it

      // cout << "Comparing histogram " << obj->GetName() << endl;
      TH1 *h1 = (TH1*)obj;

      comp = new THStack(TString(h1->GetName()) + "Stack", TString(h1->GetTitle()) + " stack");

      // loop over all source files and add the content of the
      // correspondant histogram to the one pointed to by "h1"
      TFile *nextsource = (TFile*)sourcelist->After( first_source );
      comp->Add(h1);

      bool found = false;
      while ( nextsource ) {
        // make sure we are at the correct directory level by cd'ing to path
        nextsource->cd( path );
        TKey *key2 = (TKey*)gDirectory->GetListOfKeys()->FindObject(h1->GetName());
        if (key2) {
          found = true;
          TH1 *h2 = (TH1*)key2->ReadObj();
          h2->SetLineColor(kRed);
          comp->Add(h2);

          if ( h1->GetNbinsX() != h2->GetNbinsX() ) {
            cout << "ERROR: histograms " << h1->GetName() << " have different bins on the x axis" << endl;
	    nextsource = (TFile*)sourcelist->After( nextsource );
	    continue;
            exit(1);
          }
          // 0 is the underflow and nBins+1 is the overflow.
          long double diff = 0.;
          for( int iBin = 1; iBin <= h1->GetNbinsX(); ++iBin ) {

            // Using long double for precision. This could still fail for approximations
            long double binH1 = h1->GetBinContent(iBin);
            long double binH2 = h2->GetBinContent(iBin);

            // Sum only if the bin contents are different (to avoid approximation errors)
            //if( binH1 != binH2 ) {
	    diff += fabs(binH1 - binH2);
	    // cout << "binH1 = " << binH1 << ", binH2 = " << binH2 << endl;
            //}

          }
          // cout << h1->GetName() << " difference = " << diff << endl;
	  ++totalComparedHistograms;
          if( diff != 0 ) {
	    cout << h1->GetName() << " difference = " << diff << endl;
	    ++countDifferences;
	    wasDifferent = true;
	  }
        }
        nextsource = (TFile*)sourcelist->After( nextsource );
      }
      if (!found) cout << "ERROR: no matching histogram found in the second file" << endl;
    }
    else if ( obj->IsA()->InheritsFrom( "TDirectory" ) ) {
      // it's a subdirectory

      TString tempName(obj->GetName());
      if( tempName.Contains("DQM") || tempName.Contains("Run") || tempName.Contains("SiStrip") || tempName.Contains("MechanicalView")
	  || tempName.Contains("TIB") || tempName.Contains("TOB") || tempName.Contains("TID") || tempName.Contains("TEC") || tempName.Contains("side") || tempName.Contains("wheel")
	  || tempName.Contains("Bad") || tempName.Contains("layer") || tempName.Contains("Track") || tempName.Contains("Beam") || tempName.Contains("General") || tempName.Contains("Hit") ) {

	// cout << "Found subdirectory " << obj->GetName() << endl;

	// create a new subdir of same name and title in the target file
	target->cd();
	TDirectory *newdir = target->mkdir( obj->GetName(), obj->GetTitle() );

	// newdir is now the starting point of another round of merging
	// newdir still knows its depth within the target file via
	// GetPath(), so we can still figure out where we are in the recursion
	CompareRootfile( newdir, diffTarget, sourcelist );
      }
    }
//     else {
//       // object is of no type that we know or can handle
//       cout << "Unknown object type, name: "
//            << obj->GetName() << " title: " << obj->GetTitle() << endl;
//     }

    // now write the compared histograms (which are "in" obj) to the target file
    // note that this will just store obj in the current directory level,
    // which is not persistent until the complete directory itself is stored
    // by "target->Write()" below
    if ( obj ) {
      target->cd();

      if( obj->IsA()->InheritsFrom( "TH1" ) ) {
	if( !obj->IsA()->InheritsFrom( "TH2" ) && !obj->IsA()->InheritsFrom( "THStack" ) ) {
	  // Write the superimposed histograms to file
	  //obj->Write( key->GetName() );
	  TCanvas canvas( TString(obj->GetName())+"Canvas", TString(obj->GetName())+" canvas", 1000, 800 );
	  comp->Draw("nostack");
	  canvas.Write();
	  comp->Write();
	  if( wasDifferent ) {
	    diffTarget->cd();
	    canvas.Write();
	    comp->Write();
	  }
	}
      }
    }

  } // while ( ( TKey *key = (TKey*)nextkey() ) )

  // save modifications to target file
  target->SaveSelf(kTRUE);
  diffTarget->SaveSelf(kTRUE);
  TH1::AddDirectory(status);
}
