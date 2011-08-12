#include <windows.h>
#include <stdio.h>
#include "fmfWriter.h"

fmfWriter::fmfWriter(){
	writeFlag = false;
}


fmfWriter::~fmfWriter(){
	stopWrite();
};

bool fmfWriter::startWrite(const char * fileName, unsigned __int32 pWidth, unsigned __int32 pHeight, FILE* out){
	
	nInput = 0;
	nWritten = 0;

	// log output
	logFID = out;

	//capture height/width
	wWidth = pWidth;
	wHeight = pHeight;

	//Open File and Write FMF Header
	pFile = fopen(fileName,"wb");
	if(pFile == NULL){
		fprintf(logFID,"Error opening file %s for writing\n",fileName);
		return false;
	}
			
	unsigned __int32 fmfVersion = 1;
	unsigned __int64 bytesPerChunk = (unsigned __int64)wHeight*(unsigned __int64)wWidth+(unsigned __int64)8;

	fwrite(&fmfVersion,4,1,pFile);		//write version number (int32)
	fwrite(&wHeight,4,1,pFile);			//write image height (int32)
	fwrite(&wWidth,4,1,pFile);			//write image width (int32)
	fwrite(&bytesPerChunk,8,1,pFile);	//write frame size + timestamp (double)
	fwrite(&nWritten,8,1,pFile);		//write number of frames (will need to be updated at end) (double)
	fprintf(logFID,"FMF Header Written\n");
	
	writeFlag = true;

	return true;

};
unsigned __int64 fmfWriter::stopWrite(){
	if (writeFlag == false) return true;

	writeFlag = false;

	//Close the file
	fseek(pFile,20,0);
	fwrite(&nWritten,8,1,pFile);
	fclose(pFile);

	return nWritten;
}
bool fmfWriter::addFrame(char * frame, unsigned long timestampHi, unsigned long timestampLo){

	nWritten++;

	// write timestamp
	fwrite(&timestampHi,4,1,pFile);
	fwrite(&timestampLo,4,1,pFile);
						
	//Write entire frame at once
	fwrite(frame,1,wWidth*wHeight,pFile);

	return true;
}