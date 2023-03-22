// $Id$
//-----------------------------------------------------------------------
//       The GSI Online Offline Object Oriented (Go4) Project
//         Experiment Data Processing at EE department, GSI
//-----------------------------------------------------------------------
// Copyright (C) 2000- GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
//                     Planckstr. 1, 64291 Darmstadt, Germany
// Contact:            http://go4.gsi.de
//-----------------------------------------------------------------------
// This software can be used under the license agreements as stated
// in Go4License.txt file which is part of the distribution.
//-----------------------------------------------------------------------

#include "TYYYUnpackProc.h"

#include "TClass.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TH1.h"
#include "TH2.h"
#include "TGraph.h"

#include "TGo4Log.h"
#include "TGo4WinCond.h"
#include "TGo4PolyCond.h"

#include "TYYYUnpackEvent.h"
#include "TYYYRawEvent.h"
#include "TYYYParameter.h"

//***********************************************************
TYYYUnpackProc::TYYYUnpackProc() :
   TGo4EventProcessor()
{
}

//***********************************************************
TYYYUnpackProc::TYYYUnpackProc(const char *name) :
   TGo4EventProcessor(name)
{
   TGo4Log::Info("TYYYUnpackProc: Create %s", name);

   //// init user analysis objects:
	gr1 = MakeGraph("Test/figure", "Test figure for ADC", 32);
}

//***********************************************************
TYYYUnpackProc::~TYYYUnpackProc()
{
}

//***********************************************************
Bool_t TYYYUnpackProc::CheckEventClass(TClass *cl)
{
   return cl->InheritsFrom(TYYYUnpackEvent::Class());
}

#include <fstream>

//-----------------------------------------------------------
Bool_t TYYYUnpackProc::BuildEvent(TGo4EventElement *dest)
{
   TYYYRawEvent *inp = dynamic_cast<TYYYRawEvent*> (GetInputEvent());

   TYYYUnpackEvent* poutevt = (TYYYUnpackEvent*) (dest);

   if (!inp || !poutevt) {
      TGo4Log::Error("YYYUnpackProc: events are not specified!");
      return kFALSE;
   }

/*
   // fill poutevt here:
	const int n = 100;
	for (Int_t i=0; i<n; i++) {
 		double x = i*0.1;
     	double y = 10*sin(4.*x);
		gr1->SetPoint(i, x, y);
   }
*/

   int count = 0;
   int n = inp->fdData[count++]; // the first element is the number of data
   // std::cout << "UnpackProc: n = " << inp->fdData[count++] << std::endl;
   for (int i = 0; i < n; i ++) {
      gr1->SetPoint(i, i, inp->fdData[count++]);
   }


   // test: export to file
   static int idx_file = 0;
   std::string filename = "/home/timy/Desktop/test/result_" + std::to_string(idx_file++) + ".dat";
   std::ofstream outfile(filename);

   for (int i = 0; i < n; i ++)
      outfile << inp->fdData[i+1] << std::endl;

   outfile.close();   

   return kTRUE;
}
