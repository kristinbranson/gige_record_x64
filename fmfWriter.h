#ifndef __FMFWRITER_H
#define __FMFWRITER_H

#include "windows.h"

class fmfWriter {
public:
		fmfWriter();
		~fmfWriter();


		unsigned __int64 nInput;  //Track number of frames fed into system
		unsigned __int64 nWritten; //Track number of frames written to disk
		
		bool startWrite(const char * fileName, unsigned __int32 pWidth, unsigned __int32 pHeight, FILE* out);
		bool addFrame(char * frame, unsigned long timestampHi, unsigned long timestampLo);
		unsigned __int64 stopWrite();

//private:



		unsigned int wWidth; //Image Width
		unsigned int wHeight; //Image Height
		bool writeFlag; //Status
	
		FILE * pFile; //File Target
		FILE * logFID; 
};

#endif