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

#include "TYYYEventSource.h"

#include "TClass.h"

#include "TGo4Log.h"
#include "TGo4EventErrorException.h"
#include "TGo4EventEndException.h"
#include "TGo4UserSourceParameter.h"
#include "TYYYRawEvent.h"

#include <sys/socket.h>
#include <netinet/ip.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <iomanip>

TYYYEventSource::TYYYEventSource(const char *name,
                                 const char *args,
                                 Int_t port) :
   TGo4EventSource(name),
   fbIsOpen(kFALSE),
   fxArgs(args),
   fiPort(port)
{
   Open();
}

TYYYEventSource::TYYYEventSource(TGo4UserSourceParameter* par) :
   TGo4EventSource(" "),
   fbIsOpen(kFALSE),
   fxArgs(),
   fiPort(0)
{
   if(par) {
      SetName(par->GetName());
      SetPort(par->GetPort());
      SetArgs(par->GetExpression());
      Open();
   } else {
      TGo4Log::Error("TYYYEventSource constructor with zero parameter!");
   }
}

TYYYEventSource::TYYYEventSource() :
   TGo4EventSource("default YYY source"),
   fbIsOpen(kFALSE),
   fxArgs(),
   fiPort(0)
{
}

TYYYEventSource::~TYYYEventSource()
{
   Close();
}

Bool_t TYYYEventSource::CheckEventClass(TClass *cl)
{
   return cl->InheritsFrom(TYYYRawEvent::Class());
}

void TYYYEventSource::check_error(int ret_value, const char* message) {
	if (ret_value == -1) {
		SetCreateStatus(1);
		SetErrMess(Form("%s - %s (%d)", message, strerror(errno), errno));
		throw TGo4EventErrorException(this);
	} else {
		std::cout << "[Info] " << message << "\n";
	}
}

void show(const char* ptr, size_t count, std::ostream& out) {
	std::ios_base::fmtflags old = out.setf(std::ios_base::hex, std::ios_base::basefield);
	out.fill('0');
	for (size_t i = 0; i < count-1; i ++)
		std::cout << std::setw(2) << static_cast<unsigned>(*ptr++) << " ";
	out << std::setw(2) << static_cast<unsigned>(*ptr) << std::endl;
	out.setf(old);
}

void fillData(char* buf, int nBytes, TYYYRawEvent* evnt) {
	char* pc;
	int nSegs;
	
	if (buf[0] == 'A' && buf[1] == 'D' && buf[2] == 'C' && buf[3] == '4') {

		pc = &buf[4]; // skip the head signature "ADC4"
		int type = int(*pc++);
		int card = int(*pc++);
		int channel = int(*pc++);
		int flags = int(*pc++);
		int* pint = (int*)pc;
		nSegs = *pint++;
		pc = (char*)pint;
		pc += 4; // extra fields
	} else {
		pc = &buf[0];
		nSegs = nBytes / (4 * sizeof(short)); // without head
	}

	short* pData = (short*)pc;
	int countData = 0;
	evnt->ReAllocate(nSegs * 4 + 1); // note how many int should be allocated!
	evnt->fdData[countData++] = nSegs * 4;
	for (int i = 0; i < nSegs; i ++)
		for (int j = 0; j < 4; j ++)
			evnt->fdData[countData++] = (int)*pData++;
}

Bool_t TYYYEventSource::BuildEvent(TGo4EventElement *dest)
{
	static int countBuildEvent = 0;
	std::cout << "countBuildEvent: " << countBuildEvent++ << "\n";

	TYYYRawEvent* evnt = (TYYYRawEvent*) dest;
	if (evnt == 0)
		return kFALSE;

	if (!fbIsConnected) {
		std::cout << "attempting to connect...\n";
		connect_fd = accept(listen_fd, (sockaddr*)NULL, NULL);
		check_error(connect_fd, "socket accept");
		fbIsConnected = true;
	}

	const int maxBytesBuf = 16 + (2 << 15); // sizeof(packet_adc_header_t) + max number of bytes of data
	char buf[maxBytesBuf];

	// receive data transferred via network and store in "buf"
	Int_t status = 1;
	memset(buf, 0, sizeof(buf));
	int nBytesRecv = recv(connect_fd, buf, maxBytesBuf, 0);

	if (nBytesRecv > 0) { // if there comes actual data
		std::cout << "[Info] pid " << getpid() << " - recv data [" 
			<< count_batch << "] from client (" << nBytesRecv << " bytes)\n\n";
		status = 0;
		// fill TYYYRawEvent "evnt" in Go4 with the received raw data "buf"
		fillData(buf, nBytesRecv, evnt);
		// respond to the sender
		if (send(connect_fd, "[deliver success]\n", 20, 0) == -1)
			perror("send error");
		count_batch ++;
	} else if (nBytesRecv == 0) { // Otherwise
		std::cout << "[Info] Client closed current connect_fd\n";
		close(connect_fd);
		fbIsConnected = false;
		std::cout << "[Info] Connection socket disconnected, now waiting for new request...\n\n";
		// break
		SetCreateStatus(1);
		SetErrMess(Form("Client closed current connect_fd"));
		SetEventStatus(1);
		throw TGo4EventEndException(this);
	}

	if (status != 0) {
		evnt->SetValid(kFALSE);
		SetErrMess("YYY Event Source -- ERROR!!!");
		throw TGo4EventErrorException(this);
	}

   return kTRUE;
}

// open socket and listen to the port
Int_t TYYYEventSource::Open()
{
	if (fbIsListen)
		return -1;

	TGo4Log::Info("Start listening of TYYYEventSource");

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	check_error(listen_fd, "socket create");
	
	sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	// address in network byte order, converts unsigned int from host byte order to network byte order
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(8080);
	check_error(bind(listen_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)), "socket bind");
	check_error(listen(listen_fd, 10), "socket listen");
	TGo4Log::Info("Waiting for client's request...");
	fbIsListen = kTRUE;

   return 0;
}

Int_t TYYYEventSource::Close()
{
	if (!fbIsListen)
		return -1;
	TGo4Log::Info("Stop listening of TYYYEventSource - Server shutting down");
	Int_t status = 0;
	close(listen_fd);
	fbIsListen = kFALSE;
   return status;
}
