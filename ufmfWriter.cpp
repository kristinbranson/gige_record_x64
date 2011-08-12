#include <windows.h>
#include <stdio.h>
#include "ufmfWriter.h"
//#include "mwadaptorimaq.h"


// ************************* BackgroundModel **************************

void BackgroundModel::init(){

	// parameters

	// defaults
	minNFramesReset = 200;

	// hard-coded
	BGBinSize = 1;
	BGNBins = (int)ceil(256.0 / (float)BGBinSize);
	BGHalfBin = ((float)BGBinSize - 1.0) / 2.0;

	// initialize counts to 0
	nFramesAdded = 0;
	BGZ = 0;

	// buffers haven't been allocated yet
	nPixels = 0;
	BGCounts = NULL;
	BGCenter = NULL;
	
}

BackgroundModel::BackgroundModel(){
	init();
}

BackgroundModel::BackgroundModel(unsigned __int32 nPixels, int minNFramesReset){

	int i;

	init();

	this->nPixels = nPixels;
	this->minNFramesReset = minNFramesReset;

	// allocate
	BGCounts = new unsigned __int8*[nPixels];
	for(i = 0; i < nPixels; i++) {
		BGCounts[i] = new unsigned __int8[BGNBins];
		memset(BGCounts[i],0,BGNBins*sizeof(unsigned __int8));
	}
	BGCenter = new float[nPixels];
	memset(BGCenter,0,nPixels*sizeof(float));
}

BackgroundModel::~BackgroundModel(){
	
	if(BGCenter != NULL){
		delete [] BGCenter; BGCenter = NULL;
	}
	if(BGCounts != NULL){
		for(int i = 0; i < nPixels; i++){
			if(BGCounts[i] != NULL){
				delete [] BGCounts[i]; BGCounts[i] = NULL;
			}
		}
		delete [] BGCounts; BGCounts = NULL;
	}
	nPixels = 0;
	BGZ = 0;
	nFramesAdded = 0;

}

bool BackgroundModel::addFrame(unsigned char * im, double timestamp){

	for(int i = 0; i < nPixels; i++){
		if(im[i] < 0 || im[i] >= BGNBins){
			// im out of bounds
			return false;
		}
		else{
			BGCounts[i][im[i]/BGBinSize]++;
		}
	}
	BGZ++;
	nFramesAdded++;
	return true;
}

bool BackgroundModel::updateModel(){

	int i, j;
	unsigned __int32 countscurr;

	// compute the median
	unsigned __int8 off = (unsigned __int8)(BGZ/2);
	for(i = 0; i < nPixels; i++){
		for(j = 0, countscurr = 0; j < BGNBins, countscurr <= off; j++){
			 countscurr+=(unsigned __int32)BGCounts[i][j];
		}
		BGCenter[i] = (float)((j-1)*BGBinSize) + BGHalfBin;
	}

	// lower the weight of the old counts
	//if(BGZ > MaxBGZ){
	if(nFramesAdded >= minNFramesReset){
		//float w = MaxBGZ / BGZ;
		for(i = 0; i < nPixels; i++){
			memset(BGCounts[i],0,BGNBins);
			//for(j = 0; j < BGNBins; j++){
				//BGCounts[i][j] = (unsigned __int8)(w*BGCounts[i][j]);
				//BGCounts[i][j] = BGCounts[i][j] >> 1; // divide by 2
			//}
		}
		//BGZ = MaxBGZ;
		BGZ = 0;
	}
}

// ******************************** CompressedFrame **************************************

void CompressedFrame::init(){
	wWidth = 0;
	wHeight = 0;
	nPixels = 0;
	isFore = NULL;
	writeRowBuffer = NULL;
	writeColBuffer = NULL;
	writeWidthBuffer = NULL;
	writeHeightBuffer = NULL;
	writeDataBuffer = NULL;
	nWrites = NULL;
	timestamp = -1;
	ncc = 0;
	frameNumber = 0;

	boxLength = 30; // length of foreground boxes to store
	boxArea = ((int)boxLength) * ((int)boxLength); // boxLength^2
	maxFracFgCompress = .25; // maximum fraction of pixels that can be foreground in order for us to compress
	maxNFgCompress = 0; // nPixels == 0 currently

}

CompressedFrame::CompressedFrame(){
	init();
}

CompressedFrame::CompressedFrame(unsigned short wWidth, unsigned short wHeight, unsigned __int32 boxLength, double maxFracFgCompress){

	init();

	// frame size
	this->wWidth = wWidth;
	this->wHeight = wHeight;
	this->nPixels = ((int)wWidth) * ((int)wHeight);

	// compression parameters
	this->boxLength = boxLength;
	boxArea = ((int)boxLength) * ((int)boxLength); // boxLength^2
	this->maxFracFgCompress = maxFracFgCompress;
	maxNFgCompress = (int)((double)nPixels * maxFracFgCompress);

	// initialize backsub buffers
    isFore = new bool[nPixels]; // whether each pixel is foreground or not
	memset(isFore,0,nPixels*sizeof(bool));

	writeRowBuffer = new unsigned __int16[nPixels]; // ymins
	memset(writeRowBuffer,0,nPixels*sizeof(unsigned __int16));
	writeColBuffer = new unsigned __int16[nPixels]; // xmins
	memset(writeColBuffer,0,nPixels*sizeof(unsigned __int16));
	writeWidthBuffer = new unsigned __int16[nPixels]; // widths
	memset(writeWidthBuffer,0,nPixels*sizeof(unsigned __int16));
	writeHeightBuffer = new unsigned __int16[nPixels]; // heights
	memset(writeHeightBuffer,0,nPixels*sizeof(unsigned __int16));
	writeDataBuffer = new unsigned __int8[nPixels]; // image data
	memset(writeDataBuffer,0,nPixels*sizeof(unsigned __int8));
	nWrites = new unsigned __int16[nPixels]; // number of times we've written each pixel
	memset(nWrites,0,nPixels*sizeof(unsigned __int16));

}

CompressedFrame::~CompressedFrame(){

	if(isFore != NULL){
		delete[] isFore; isFore = NULL;
	}
	if(writeRowBuffer != NULL){
		delete[] writeRowBuffer; writeRowBuffer = NULL;
	}
	if(writeColBuffer != NULL){
		delete[] writeColBuffer; writeColBuffer = NULL;
	}
	if(writeWidthBuffer != NULL){
		delete[] writeWidthBuffer; writeWidthBuffer = NULL;
	}
	if(writeHeightBuffer != NULL){
		delete[] writeHeightBuffer; writeHeightBuffer = NULL;
	}
	if(writeDataBuffer != NULL){
		delete[] writeDataBuffer; writeDataBuffer = NULL;
	}
	if(nWrites != NULL){
		delete [] nWrites; nWrites = NULL;
	}
	nPixels = 0;
	ncc = 0;
	timestamp = -1;
	frameNumber = 0;

}

bool CompressedFrame::setData(unsigned __int8 * im, double timestamp, unsigned __int64 frameNumber, 
	unsigned __int8 * BGLowerBound, unsigned __int8 * BGUpperBound){

	// grab foreground boxes
	unsigned __int16 r, c, r1, c1;
	int i;
	int j;
	int i1;
	int numFore = 0;
	int numPxWritten = -1;

	_int64 filePosStart;
	_int64 filePosEnd;
	_int64 frameSizeBytes;
	bool isCompressed;

	this->timestamp = timestamp;
	this->frameNumber = frameNumber;

	// background subtraction
	for(i = 0; i < nPixels; i++){
		isFore[i] = (im[i] < BGLowerBound[i]) || (im[i] > BGUpperBound[i]);
		if(isFore[i]) {
			numFore++;
		}
	}

	if(numFore > maxNFgCompress){
		// don't compress if too many foreground pixels
		writeRowBuffer[0] = 0;
		writeColBuffer[0] = 0;
		writeWidthBuffer[0] = wWidth;
		writeHeightBuffer[0] = wHeight;
		for(j = 0; j < nPixels; j++){
			writeDataBuffer[j] = im[j];
		}

		// each pixel is written -- for statistics
		for(j = 0; j < nPixels; j++){
			nWrites[j] = 1;
		}
		numPxWritten = nPixels;
		ncc = 1;
		isCompressed = false;
	}
	else{

		//for(i = 0; i < nPixels; i++){
		//	debugWasFore[i] = isFore[i];
		//}

		bool doStopEarly = 0;
		for(i1 = 0; i1 < nPixels; i1++) nWrites[i1] = 0;

		i = 0; j = 0; ncc = 0;
		for(r = 0; r < wHeight; r++){
			for(c = 0; c < wWidth; c++, i++){

				// start a new box if this pixel is foreground
				if(!isFore[i]) continue;

				// store everything in box with corner at (r,c)
				writeRowBuffer[ncc] = r;
				writeColBuffer[ncc] = c;
				writeWidthBuffer[ncc] = min((unsigned short)boxLength,wWidth-c);
				writeHeightBuffer[ncc] = min((unsigned short)boxLength,wHeight-r);

				// loop through pixels to store
				for(r1 = r; r1 < r + writeHeightBuffer[ncc]; r1++){

					// check if we've already written something in this column
					doStopEarly = 0;
					for(c1 = c, i1 = r1*wWidth+c; c1 < c + writeWidthBuffer[ncc]; c1++, i1++){
						if(nWrites[i1] > 0){
							doStopEarly = 1;
							break;
						}
					}

					if(doStopEarly){
						if(r1 == r){
							// if this is the first row, then shorten the width and write as usual
							writeWidthBuffer[ncc] = c1 - c;
						}
						else{
							// otherwise, shorten the height, and don't write any of this row
							writeHeightBuffer[ncc] = r1 - r;
							break;
						}
					}

					for(c1 = c, i1 = r1*wWidth+c; c1 < c + writeWidthBuffer[ncc]; c1++, i1++){
						nWrites[i1]++;
						writeDataBuffer[j] = im[i1];
						isFore[i1] = 0;
						j++;
					}
				}

				ncc++;
			}
		}
		numPxWritten = j;
		isCompressed = true;
		//int nForeMissed = 0;
		//for(i1 = 0; i1 < nPixels; i1++){
		//	if(debugWasFore[i1] && nWrites[i1] == 0) nForeMissed++;
		//}
		//if(logger && nForeMissed > 0) logger->log(UFMF_DEBUG_3,"nForeMissed = %d\n",nForeMissed);
	}

	return true;

}

// ************************* ufmfWriter **************************

// ***** public API *****

// constructors

// common code for both the empty constructor and the parameter-filled constructor
void ufmfWriter::init(){

	// *** output ufmf state ***
	pFile = NULL;
	indexLocation = 0;
	indexPtrLocation = 0;

	// *** writing state ***
	isWriting = false;
	nGrabbed = 0;
	nWritten = 0;
	nBGKeyFramesWritten = 0;

	// *** threading/buffering state ***
	uncompressedFrames = NULL;
	uncompressedBufferTimestamps = NULL;
	compressedFrames = NULL;
	nUncompressedFramesBuffered = 0;
	nCompressedFramesBuffered = 0;

	_compressionThreads = NULL;
	_compressionThreadIDs = NULL;
	compressionThreadReadySignals = NULL;
	threadCount = 0;
	uncompressedBufferEmptySemaphores = NULL;
	uncompressedBufferFilledSemaphores = NULL;
	compressedBufferEmptySemaphores = NULL;
	compressedBufferFilledSemaphores = NULL;
	uncompressedBufferFrameNumbers = NULL;
	threadBufferIndex = NULL;

	// *** background subtraction state ***
	bg = NULL;
	minFrameBGModel1 = 0;
	BGLowerBound0 = NULL;
	BGUpperBound0 = NULL;
	BGLowerBound1 = NULL;
	BGUpperBound1 = NULL;
	lastBGUpdateTime = -1;
	lastBGKeyFrameTime = -1;

	// *** logging state ***
	stats = NULL;
	logFID = stderr;

	// *** threading parameter defaults ***
	nThreads = 4;
	nBuffers = 10;

	// *** video parameter defaults ****
	strcpy(fileName,"");
	wWidth = 0;
	wHeight = 0;
	nPixels = 0;
	// hardcode color format to grayscale
	strcpy(colorCoding,"MONO8");
	colorCodingLength = 5;

	// *** compression parameters ***

	// * background subtraction parameters *

	//hard code parameters for now
	MaxBGNFrames = 100; // approximate number of frames used in background computation
	// the last NFramesPerKeyFrame = BGKeyFramePeriod / BGUpdatePeriod should have weight
	// so we should reweight so that the total sum is MaxBGNFrames - NBGUpdatesPerKeyFrame
	BGUpdatePeriod = 1; // seconds between updates to the background model
	BGKeyFramePeriod = 100; // seconds between background keyframes
	backSubThresh = 10; // threshold for storing foreground pixels
	nFramesInit = 100; // for the first nFramesInit, we will always update the background model
	BGKeyFramePeriodInitLength = 0;
	nBGUpdatesPerKeyFrame = (int)floor(BGKeyFramePeriod / BGUpdatePeriod);
	float NBGUpdatesPerKeyFrame = (float)(BGKeyFramePeriod / BGUpdatePeriod);
	MaxBGZ = max(0.0,(float)MaxBGNFrames - NBGUpdatesPerKeyFrame);

	// * ufmf parameters *
	isFixedSize = 0; // patches are of a fixed size
	boxLength = 30; // length of foreground boxes to store
	maxFracFgCompress = .25; // maximum fraction of pixels that can be foreground in order for us to compress

   // *** statistics parameters ***

	strcpy(statFileName,"");
	printStats = true;
	statStreamPrintFreq = 1;
	statPrintFrameErrors = true;
	statPrintTimings = true;
	statComputeFrameErrorFreq = 1;

}

// empty constructor:
// initializes values to defaults
ufmfWriter::ufmfWriter(){
	init();
}

// parameters:
// [video parameters:]
// fileName: name of video to write to
// pWidth: width of frame
// pHeight: height of frame
// [acquisition parameters:]
// nBuffers: number of frames to buffer between acquiring and writing
// [compression parameters:]
// MaxBGNFrames: approximate number of frames used in background computation
// BGUpdatePeriod: seconds between updates to the background model
// BGKeyFramePeriod: seconds between background keyframes
// boxLength: length of foreground boxes to store
// backSubThresh: threshold for storing foreground pixels
// nFramesInit: for the first nFramesInit, we will always update the background model
// maxFracFgCompress: maximum fraction of pixels that can be foreground in order for us to try to compress the frame
// [compression stats parameters:]
// statFileName: name of file to write compression statistics to. If NULL, then statistics are combined into debug file
// printStats: whether to print compression statistics
// statStreamPrintFreq: number of frames between outputting per-frame compression statistics
// statPrintFrameErrors: whether to compute and print statistics of compression error. Currently, box-averaged and per-pixel errors are either both
// computed or both not computed. 
// statPrintTimings: whether to print information about the time each part of the computation takes. 
// nThreads: number of threads to allocate to compress frames simultaneously

ufmfWriter::ufmfWriter(const char * fileName, unsigned __int32 pWidth, unsigned __int32 pHeight, FILE * logFID, unsigned __int32 nBuffers,
	int MaxBGNFrames, double BGUpdatePeriod, double BGKeyFramePeriod, unsigned __int32 boxLength,
	double backSubThresh, unsigned __int32 nFramesInit, double* BGKeyFramePeriodInit, int BGKeyFramePeriodInitLength, double maxFracFgCompress, 
	const char *statFileName, bool printStats, int statStreamPrintFreq, bool statPrintFrameErrors, bool statPrintTimings, 
	int statComputeFrameErrorFreq, int nThreads){

	int i, j;

	// initialize state, set default parameters
	init();

	// ***** parameters *****

	// *** threading parameters ***
	this->nThreads = nThreads;
	if(nBuffers < nThreads)
		this->nBuffers = nThreads;
	else
		this->nBuffers = nBuffers;

	// *** video parameters ***
	strcpy(this->fileName, fileName);
	//capture height/width
	this->wWidth = pWidth;
	this->wHeight = pHeight;
	nPixels = (unsigned int)wWidth*(unsigned int)wHeight;

	// *** compression parameters ***

	// * background subtraction parameters *
	this->MaxBGNFrames = MaxBGNFrames;
	this->BGUpdatePeriod = BGUpdatePeriod;
	this->BGKeyFramePeriod = BGKeyFramePeriod;
	this->boxLength = boxLength;
	this->backSubThresh = (float)backSubThresh;
	this->nFramesInit = nFramesInit;
	for(int i = 0; i < BGKeyFramePeriodInitLength; i++){
		this->BGKeyFramePeriodInit[i] = BGKeyFramePeriodInit[i];
	}
	this->BGKeyFramePeriodInitLength = BGKeyFramePeriodInitLength;
	float NBGUpdatesPerKeyFrame = (float)(BGKeyFramePeriod / BGUpdatePeriod);
	nBGUpdatesPerKeyFrame = (int)floor(BGKeyFramePeriod / BGUpdatePeriod);
	MaxBGZ = max(0.0,(float)MaxBGNFrames - NBGUpdatesPerKeyFrame);

	// * ufmf parameters *
	this->maxFracFgCompress = maxFracFgCompress;

	// *** statistics parameters ***
	strcpy(this->statFileName,statFileName);
	this->printStats = printStats;
	this->statStreamPrintFreq = statStreamPrintFreq;
	this->statPrintFrameErrors = statPrintFrameErrors;
	this->statPrintTimings = statPrintTimings;
	this->statComputeFrameErrorFreq = statComputeFrameErrorFreq;

	// ***** allocate stuff *****

	// *** threading/buffering state ***
	uncompressedFrames = new unsigned char*[nBuffers];
	for(i = 0; i < nBuffers; i++){
		uncompressedFrames[i] = new unsigned char[nPixels];
		memset(uncompressedFrames[i],0,nPixels*sizeof(char));
	}
	uncompressedBufferTimestamps = new double[nBuffers];
	memset(uncompressedBufferTimestamps,0,nBuffers*sizeof(double));
	uncompressedBufferFrameNumbers = new unsigned __int64[nBuffers];
	memset(uncompressedBufferFrameNumbers,0,nBuffers*sizeof(unsigned __int64));
	compressedFrames = new CompressedFrame*[nBuffers];
	for(i = 0; i < nBuffers; i++){
		compressedFrames[i] = new CompressedFrame(wWidth,wHeight,boxLength,maxFracFgCompress);
	}

	// allocate compression thread stuff
	_compressionThreads = new HANDLE[nThreads];
	_compressionThreadIDs = new DWORD[nThreads];
	compressionThreadReadySignals = new HANDLE[nThreads];
	threadBufferIndex = new int[nThreads];
	memset(threadBufferIndex,0,nThreads*sizeof(int));

	// allocate semaphores
	uncompressedBufferEmptySemaphores = new HANDLE[nBuffers];
	uncompressedBufferFilledSemaphores = new HANDLE[nBuffers];
	compressedBufferEmptySemaphores = new HANDLE[nBuffers];
	compressedBufferFilledSemaphores = new HANDLE[nBuffers];

	//// *** background subtraction state ***
	bg = new BackgroundModel(nPixels,nBGUpdatesPerKeyFrame);
	BGLowerBound0 = new unsigned __int8[nPixels]; // per-pixel lower bound on background
	memset(BGLowerBound0,0,nPixels*sizeof(unsigned __int8));
	BGUpperBound0 = new unsigned __int8[nPixels]; // per-pixel upper bound on background
	memset(BGUpperBound0,0,nPixels*sizeof(unsigned __int8));
	BGLowerBound1 = new unsigned __int8[nPixels]; // per-pixel lower bound on background
	memset(BGLowerBound1,0,nPixels*sizeof(unsigned __int8));
	BGUpperBound1 = new unsigned __int8[nPixels]; // per-pixel upper bound on background
	memset(BGUpperBound1,0,nPixels*sizeof(unsigned __int8));

	// *** logging state ***
	this->logFID = logFID;
	logger = new ufmfLogger(logFID);
	if(printStats) {
		if(statFileName && strcmp(statFileName,""))
			stats = new ufmfWriterStats(statFileName, wWidth, wHeight, statStreamPrintFreq, statPrintFrameErrors, statPrintTimings, statComputeFrameErrorFreq, true);
		else
			stats = new ufmfWriterStats(logFID, wWidth, wHeight, statStreamPrintFreq, statPrintFrameErrors, statPrintTimings, statComputeFrameErrorFreq, true);
	}

 }

 // destructor
 ufmfWriter::~ufmfWriter(){

	 int i;

	 // stop writing if writing

	 // SHOULD WE LOCK FIRST?
	 if(isWriting){
		stopWrite();
		logger->log(UFMF_DEBUG_3,"stopped writing in destructor\n");
	 }

	 // deallocate stuff

	 // buffers for compressed, uncompressed frames
	 deallocateBuffers();

	 // background model
	 deallocateBGModel();

	 // threading stuff
	 deallocateThreadStuff();

	 nGrabbed = 0;
	 nWritten = 0;
	 isWriting = false;

	 if(stats){
		delete stats;
		stats = NULL;
		logger->log(UFMF_DEBUG_3,"deleted stats in destructor\n");
	}

}

bool ufmfWriter::startWrite(){

	int i;

	nGrabbed = 0;
	nWritten = 0;
	nBGKeyFramesWritten = 0;
	lastBGUpdateTime = -1;
	lastBGKeyFrameTime = -1;

	logger->log(UFMF_DEBUG_3,"starting to write\n");

	// open File
	pFile = fopen(fileName,"wb");
	if(pFile == NULL){
		logger->log(UFMF_ERROR,"Error opening file %s for writing\n",fileName);
		return false;
	}

	// write header
	if(!writeHeader()){
		logger->log(UFMF_ERROR,"Error writing header\n");
		return false;
	}

	// initialize semaphores
	lock = CreateSemaphore(NULL, 1, 1, NULL);
	if(lock == NULL){
		logger->log(UFMF_ERROR,"Error creating lock semaphore\n");
		return false;
	}

	// compression semaphores
	for(i = 0; i < nBuffers; i++){
		// initialize value to 1 to signify empty
		uncompressedBufferEmptySemaphores[i] = CreateSemaphore(NULL,1,1,NULL);
		if(uncompressedBufferEmptySemaphores[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating uncompressedBufferEmptySemaphores[%d] semaphore\n",i);
			return false;
		}

		// initialize value to 0 to signify not filled
		uncompressedBufferFilledSemaphores[i] = CreateSemaphore(NULL,0,1,NULL);
		if(uncompressedBufferFilledSemaphores[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating uncompressedBufferFilledSemaphores[%d] semaphore\n",i);
			return false;
		}

		// initialize value to 1 to signify empty
		compressedBufferEmptySemaphores[i] = CreateSemaphore(NULL,1,1,NULL);
		if(compressedBufferEmptySemaphores[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating compressedBufferEmptySemaphores[%d] semaphore\n",i);
			return false;
		}

		// initialize value to 0 to signify not filled
		compressedBufferFilledSemaphores[i] = CreateSemaphore(NULL,0,1,NULL);
		if(compressedBufferFilledSemaphores[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating compressedBufferFilledSemaphores[%d] semaphore\n",i);
			return false;
		}

		// initialize that compression threads are not ready
		compressionThreadReadySignals[i] = CreateSemaphore(NULL,0,1,NULL);
		if(compressionThreadReadySignals[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating compressionThreadReadySignals[%d] semaphore\n",i);
			return false;
		}

		// initialize not to start compression threads
		compressionThreadStartSignals[i] = CreateSemaphore(NULL,0,1,NULL);
		if(compressionThreadStartSignals[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating compressionThreadStartSignals[%d] semaphore\n",i);
			return false;
		}
	}

	// compression manager thread semaphore
	compressionThreadManagerReadySignal = CreateSemaphore(NULL,0,1,NULL);
	if(compressionThreadManagerReadySignal == NULL){
		logger->log(UFMF_ERROR,"Error creating compressionThreadManagerReadySignal semaphore\n");
		return false;
	}

	// writing thread semaphore
	writeThreadReadySignal = CreateSemaphore(NULL,0,1,NULL);
	if(writeThreadReadySignal == NULL){
		logger->log(UFMF_ERROR,"Error creating writeThreadReadySignal semaphore\n");
		return false;
	}

	// bg semaphore
	keyFrameWritten = CreateSemaphore(NULL,1,1,NULL);
	if(keyFrameWritten == NULL){
		logger->log(UFMF_ERROR,"Error creating keyFrameWritten semaphore\n");
		return false;
	}

	isWriting = true;
	// start compression thread manager
	_compressionThreadManager = CreateThread(NULL,0,compressionThreadManager,this,0,&_compressionThreadManagerID);
	if ( _compressionThreadManager == NULL ){ 
		return false; 
	}
	if(WaitForSingleObject(compressionThreadManagerReadySignal, MAXWAITTIMEMS) != WAIT_OBJECT_0) { 
		logger->log(UFMF_ERROR,"Error Starting Compression Manager Thread\n"); 
		return false; 
	}

	// start compression threads
	for(i = 0; i < nThreads; i++){
		_compressionThreads[i] = CreateThread(NULL,0,compressionThread,this,0,&_compressionThreadIDs[i]);
		if(_compressionThreads[i] == NULL){
			logger->log(UFMF_ERROR,"Error creating compression thread %d\n",i);
			return false;
		}
		if(WaitForSingleObject(compressionThreadReadySignals[i], MAXWAITTIMEMS) != WAIT_OBJECT_0) { 
			logger->log(UFMF_ERROR, "Error starting compression thread %d\n",i); 
			return false; 
		}
	}

	// start write thread
	_writeThread = CreateThread(NULL,0,writeThread,this,0,&_writeThreadID);
	if ( _writeThread == NULL ){ 
		return false; 
	}
	if(WaitForSingleObject(writeThreadReadySignal, MAXWAITTIMEMS) != WAIT_OBJECT_0) { 
		logger->log(UFMF_ERROR,"Error Starting Write Thread\n"); 
		return false; 
	}

	return true;
}

unsigned __int64 ufmfWriter::stopWrite(){

	logger->log(UFMF_DEBUG_3,"Stopping writing\n");

	// no need to lock since stopThreads is the only thing that will be writing to isWriting
	if(!isWriting){
		logger->log(UFMF_DEBUG_3,"Stop writing called while not writing, nothing to do.\n");

		return 0;
	}

	// stop all the writing and compressing threads
	stopThreads(true);

	// finish writing the movie -- write the indexes, close the file, etc.
	if(!finishWriting()){
		logger->log(UFMF_ERROR,"Error finishing writing the video file\n");
		return 0;
	}

	logger->log(UFMF_DEBUG_3,"Stopped all threads, wrote footer, closed video.\n");

	return nWritten;
}

// add a frame to the processing queue
bool ufmfWriter::addFrame(unsigned char * frame, double timestamp){

	int bufferIndex;
	unsigned __int64 frameNumber;

	// wait for any uncompressed frame to be empty
	bufferIndex = (int)WaitForMultipleObjects(nBuffers,uncompressedBufferEmptySemaphores,false,MAXWAITTIMEMS) - (int)WAIT_OBJECT_0;
	if(bufferIndex < 0 || bufferIndex >= nBuffers){
		logger->log(UFMF_ERROR,"Error waiting for a frame to be added: %x\n",bufferIndex+WAIT_OBJECT_0);
		return false;
	}

	// store that this buffer contains current frame
	nGrabbed++;
	frameNumber = nGrabbed;
	logger->log(UFMF_DEBUG_7,"Adding frame %d\n",frameNumber);

	uncompressedBufferFrameNumbers[bufferIndex] = frameNumber;
	Lock();
	nUncompressedFramesBuffered++;
	Unlock();

	// update background counts if necessary
	if(!addToBGModel(frame,timestamp,frameNumber)){
		logger->log(UFMF_ERROR,"Error adding frame to background model");
		return false;
	}

	// reset background model if necessary, signal to write key frame
	if(!updateBGModel(frame,timestamp,frameNumber)){
		logger->log(UFMF_ERROR,"Error computing new background model");
		return false;
	}

	// copy over the data
	memcpy(uncompressedFrames[bufferIndex],frame,nPixels*sizeof(unsigned char));
	uncompressedBufferTimestamps[bufferIndex] = timestamp;

	// signal that the uncompressed buffer is filled
	ReleaseSemaphore(uncompressedBufferFilledSemaphores[bufferIndex],1,NULL);

	return true;
}

// set video file name, width, height
// todo: resize buffers, background model if already allocated
void ufmfWriter::setVideoParams(char * fileName, int wWidth, int wHeight){
	strcpy(this->fileName, fileName);
	this->wWidth = wWidth;
	this->wHeight = wHeight;
	this->nPixels = wWidth * wHeight;

}

// parameters:
// [compression parameters:]
// MaxBGNFrames: approximate number of frames used in background computation
// BGUpdatePeriod: seconds between updates to the background model
// BGKeyFramePeriod: seconds between background keyframes
// boxLength: length of foreground boxes to store
// backSubThresh: threshold for storing foreground pixels
// nFramesInit: for the first nFramesInit, we will always update the background model
// maxFracFgCompress: maximum fraction of pixels that can be foreground in order for us to try to compress the frame
bool ufmfWriter::readParamsFile(char * paramsFile){

	FILE * fp = fopen(paramsFile,"r");
	bool failure = false;
	const size_t maxsz = 200;
	char line[maxsz];
	char paramName[maxsz];
	double paramValue;
	char * s;

	logger->log(UFMF_ERROR,"Reading parameters from file %s\n",paramsFile);

	if(fp == NULL){
		logger->log(UFMF_ERROR,"Error opening parameter file %s for reading.\n",paramsFile);
		return failure;
	}
	while(true){
		if(fgets(line,maxsz,fp) == NULL) break;
		if(line[0] == '#') continue;
		if(sscanf(line,"%200[^,],%lf\n",paramName,&paramValue) < 1){
			if(logger) logger->log(UFMF_WARNING,"could not parse line %s\n",line);
			continue;
		}
		logger->log(UFMF_DEBUG_3,"paramName = %s, paramValue = %lf\n",paramName,paramValue);

		// maximum fraction of pixels that can be foreground to try compressing frame
		if(strcmp(paramName,"UFMFMaxFracFgCompress") == 0){
			this->maxFracFgCompress = paramValue;
		}
		// number of frames the background model should be based on 
		else if(strcmp(paramName,"UFMFMaxBGNFrames") == 0){
			this->MaxBGNFrames = (int)paramValue;
		}
		// number of seconds between updates to the background model
		else if(strcmp(paramName,"UFMFBGUpdatePeriod") == 0){
			this->BGUpdatePeriod = paramValue;
		}
		// number of seconds between spitting out a new background model
		else if(strcmp(paramName,"UFMFBGKeyFramePeriod") == 0){
			this->BGKeyFramePeriod = paramValue;
		}
		// max length of box stored during compression
		else if(strcmp(paramName,"UFMFMaxBoxLength") == 0){
			this->boxLength = (int)paramValue;
		}
		// threshold for background subtraction
		else if(strcmp(paramName,"UFMFBackSubThresh") == 0){
			this->backSubThresh = (float)paramValue;
		}
		// first nFramesInit will be output raw
		else if(strcmp(paramName,"UFMFNFramesInit") == 0){
			this->nFramesInit = (int)paramValue;
		}
		else if(strcmp(paramName,"UFMFBGKeyFramePeriodInit") == 0){
			s = strtok(line,",");
			for(s = strtok(NULL,","), BGKeyFramePeriodInitLength = 0; s != NULL; s = strtok(NULL,","), BGKeyFramePeriodInitLength++){
				sscanf(s,"%lf",&this->BGKeyFramePeriodInit[BGKeyFramePeriodInitLength]);
			}
		}
		// Whether to compute UFMF diagnostics
		else if(strcmp(paramName,"UFMFPrintStats") == 0){
			this->printStats = paramValue != 0;
		}
		// number of frames between outputting per-frame compression statistics: 0 means don't print, 1 means every frame
		else if(strcmp(paramName,"UFMFStatStreamPrintFreq") == 0){
			this->statStreamPrintFreq = (int)paramValue;
		}
		// number of frames between computing statistics of compression error. 0 means don't compute, 1 means every frame
		else if(strcmp(paramName,"UFMFStatComputeFrameErrorFreq") == 0){
			this->statComputeFrameErrorFreq = (int)paramValue;	
		}
		// whether to print information about the time each part of the computation takes
		else if(strcmp(paramName,"UFMFStatPrintTimings") == 0){
			this->statPrintTimings = paramValue != 0;
		}
		else{
			if(logger) logger->log(UFMF_WARNING,"Unknown parameter %s with value %f skipped\n",paramName,paramValue);
		}


	}

	fclose(fp);

	return !failure;

}

// read stats params from a file
void ufmfWriter::setStatsParams(const char * statsName){
	strcpy(this->statFileName,statsName);
	// TODO: update stats parameters
}

// ***** private helper functions *****

// *** writing tools ***

// write a frame
bool ufmfWriter::writeFrame(CompressedFrame * im){

	int i;
	_int64 filePosStart;
	_int64 filePosEnd;
	_int64 frameSizeBytes;
	bool isCompressed;

	logger->log(UFMF_DEBUG_7,"writing compressed frame %d\n",im->frameNumber);

	// location of this frame
	filePosStart = _ftelli64(pFile);

	// add current location to index
	index.push_back(filePosStart);
	index_timestamp.push_back(im->timestamp);

	// write chunk type: 1
	fwrite(&FRAMECHUNK,1,1,pFile);
	// write timestamp: 8
	fwrite(&im->timestamp,8,1,pFile);
	// number of connected components
	fwrite(&im->ncc,4,1,pFile);

	// write each box
	i = 0;
	int area = 0;
	for(unsigned int cc = 0; cc < im->ncc; cc++){
		area = im->writeWidthBuffer[cc]*im->writeHeightBuffer[cc];
		fwrite(&im->writeColBuffer[cc],2,1,pFile);
		fwrite(&im->writeRowBuffer[cc],2,1,pFile);
		fwrite(&im->writeWidthBuffer[cc],2,1,pFile);
		fwrite(&im->writeHeightBuffer[cc],2,1,pFile);
		fwrite(&im->writeDataBuffer[i],1,area,pFile);
		i += area;
	}

	filePosEnd = _ftelli64(pFile);
	frameSizeBytes = filePosEnd - filePosStart;

	//if(logger) logger->log(UFMF_DEBUG_5, "timestamp = %f\n",timestamp);

	//if(stats) {
	//	stats->updateTimings(UTT_COMPUTE_STATS);
	//	stats->update(index, index_timestamp, frameSizeBytes, isCompressed, numFore, numPxWritten, ncc, numBuffered, numDropped, nWrites, wWidth*wHeight, im, BGCenter, UFMF_DEBUG_3);
	//}

	return true;
}

// write the video header
bool ufmfWriter::writeHeader(){

	logger->log(UFMF_DEBUG_7,"Writing video header\n");

	if(stats) {
		//stats->clear();
		stats->updateTimings(UTT_WRITE_HEADER);
	}

	// location of index
	indexLocation = 0;

	unsigned __int32 ufmfVersion = 4; // UFMF version 4

	unsigned __int64 bytesPerChunk = (unsigned __int64)wHeight*(unsigned __int64)wWidth+(unsigned __int64)8;

	// write "ufmf"
	const char ufmfString[] = "ufmf";
	fwrite(ufmfString,1,4,pFile); 
	// write version
	fwrite(&ufmfVersion,4,1,pFile);
	// this is where we write the index location
	indexPtrLocation = ftell(pFile);
	// write index location. 0 for now
	fwrite(&indexLocation,8,1,pFile);

	// max width, height: 2, 2
	if(isFixedSize){
		fwrite(&boxLength,2,1,pFile);
		fwrite(&boxLength,2,1,pFile);
	}
	else{
		fwrite(&wWidth,2,1,pFile);
		fwrite(&wHeight,2,1,pFile);
	}

	// whether it is fixed size patches: 1
	fwrite(&isFixedSize,1,1,pFile);

	// raw coding string length: 1
	fwrite(&colorCodingLength,1,1,pFile);
	// coding: length(coding)
	fwrite(colorCoding,1,colorCodingLength,pFile);

	return true;

}

// write the indexes, pointers, close the movie
bool ufmfWriter::finishWriting(){

	if(stats) stats->updateTimings(UTT_WRITE_FOOTER);
	logger->log(UFMF_DEBUG_3, "writing video footer and closing %s\n", fileName);

	// write the index at the end of the file
	_fseeki64(pFile,0,SEEK_END);

	// write index chunk identifier
	fwrite(&INDEX_DICT_CHUNK,1,1,pFile);

	// save location of index
	indexLocation = _ftelli64(pFile);

	// write index dictionary

	// write a 'd' for dict
	char d = 'd';
	fwrite(&d,1,1,pFile);

	// write the number of keys
	unsigned __int8 nkeys = 2;
	fwrite(&nkeys,1,1,pFile);

		// write index->frame

		const char frameString[] = "frame";
		unsigned __int16 frameStringLength = sizeof(frameString) - 1;

		// write the length of the key
		fwrite(&frameStringLength,2,1,pFile);
		// write the key
		fwrite(frameString,1,frameStringLength,pFile);

		// write a 'd' for dict
		fwrite(&d,1,1,pFile);

		// write the number of keys
		nkeys = 2;
		fwrite(&nkeys,1,1,pFile);

			// write index->frame->loc
			const char locString[] = "loc";
			unsigned __int16 locStringLength = sizeof(locString) - 1;

			// write the length of the key
			fwrite(&locStringLength,2,1,pFile);
			// write the key
			fwrite(locString,1,locStringLength,pFile);

			// write a for array
			char a = 'a';
			fwrite(&a,1,1,pFile);

			// write the data type
			char datatype = 'q';
			fwrite(&datatype,1,1,pFile);

			// write the number of bytes
			unsigned __int32 nbytes = 8*index.size();
			fwrite(&nbytes,4,1,pFile);

			// write the array
			unsigned __int64 loc;
			for (unsigned int i = 0 ; i < index.size(); i++ ){
				loc = index[i];
				fwrite(&loc,8,1,pFile);
			}

			// end of index->frame->loc

			// write index->frame->timestamp
			const char timestampString[] = "timestamp";
			unsigned __int16 timestampStringLength = sizeof(timestampString) - 1;

			// write the length of the key
			fwrite(&timestampStringLength,2,1,pFile);
			// write the key
			fwrite(timestampString,1,timestampStringLength,pFile);

			// write a for array
			fwrite(&a,1,1,pFile);

			// write the data type
			datatype = 'd';
			fwrite(&datatype,1,1,pFile);

			// write the number of bytes
			nbytes = 8*index.size();
			fwrite(&nbytes,4,1,pFile);

			// write the array
			double timestamp;
			for (unsigned int i = 0 ; i < index.size(); i++ ){
				timestamp = index_timestamp[i];
				fwrite(&timestamp,8,1,pFile);
			}

			// end index->frame->timestamp

		// end index->frame

		// write index->keyframe
		const char keyframeString[] = "keyframe";
		unsigned __int16 keyframeStringLength = sizeof(keyframeString) - 1;

		// write the length of the key
		fwrite(&keyframeStringLength,2,1,pFile);
		// write the key
		fwrite(keyframeString,1,keyframeStringLength,pFile);
	
		// write a 'd' for dict
		fwrite(&d,1,1,pFile);

		// write the number of keys
		nkeys = 1;
		fwrite(&nkeys,1,1,pFile);

			// write index->keyframe->mean
			const char meanString[] = "mean";
			unsigned __int16 meanStringLength = sizeof(meanString) - 1;

			// write the length of the key
			fwrite(&meanStringLength,2,1,pFile);
			// write the key
			fwrite(meanString,1,meanStringLength,pFile);

			// write a 'd' for dict
			fwrite(&d,1,1,pFile);

			// write the number of keys
			nkeys = 2;
			fwrite(&nkeys,1,1,pFile);

				// write index->keyframe->mean->loc

				// write the length of the key
				fwrite(&locStringLength,2,1,pFile);
				// write the key
				fwrite(locString,1,locStringLength,pFile);

				// write a for array
				fwrite(&a,1,1,pFile);

				// write the data type
				datatype = 'q';
				fwrite(&datatype,1,1,pFile);
	
				// write the number of bytes
				nbytes = 8*meanindex.size();
				fwrite(&nbytes,4,1,pFile);

				// write the array
				for (unsigned int i = 0 ; i < meanindex.size(); i++ ){
					loc = meanindex[i];
					fwrite(&loc,8,1,pFile);
				}

				// end of index->frame->loc

				// write index->keyframe->mean->timestamp

				// write the length of the key
				fwrite(&timestampStringLength,2,1,pFile);
				// write the key
				fwrite(timestampString,1,timestampStringLength,pFile);

				// write a for array
				fwrite(&a,1,1,pFile);

				// write the data type
				datatype = 'd';
				fwrite(&datatype,1,1,pFile);
	
				// write the number of bytes
				nbytes = 8*meanindex.size();
				fwrite(&nbytes,4,1,pFile);

				// write the array
				for (unsigned int i = 0 ; i < meanindex_timestamp.size(); i++ ){
					timestamp = meanindex_timestamp[i];
					fwrite(&timestamp,8,1,pFile);
				}

				// end index->keyframe->mean->timestamp

			// end index->keyframe->mean

		// end index->keyframe

	// end index

	// write the index location
	_fseeki64(pFile,indexPtrLocation,SEEK_SET);
	fwrite(&indexLocation,8,1,pFile);

	//Close the file
	fclose(pFile);
	pFile = NULL;

	meanindex.clear();
	meanindex_timestamp.clear();
	index.clear();
	index_timestamp.clear();

	return true;
}

bool ufmfWriter::writeBGKeyFrame(){

	int i, j;
	unsigned __int32 countscurr;

	//if(stats) stats->updateTimings(UTT_WRITE_KEYFRAME);
	logger->log(UFMF_DEBUG_7,"writing keyframe\n");

	// add to keyframe index
	meanindex.push_back(_ftelli64(pFile));
	meanindex_timestamp.push_back(keyframeTimestamp);

	// write keyframe chunk identifier
	fwrite(&KEYFRAMECHUNK,1,1,pFile);

	// write the keyframe type
	const char keyFrameType[] = "mean";
	unsigned __int8 keyFrameTypeLength = sizeof(keyFrameType) - 1;
	fwrite(&keyFrameTypeLength,1,1,pFile);
	fwrite(keyFrameType,1,keyFrameTypeLength,pFile);

	// write the data type
	const char dataType = 'f';
	fwrite(&dataType,1,1,pFile);

	// width, height
	fwrite(&wWidth,2,1,pFile);
	fwrite(&wHeight,2,1,pFile);

	// timestamp
	fwrite(&keyframeTimestamp,8,1,pFile);

	// write the frame
	fwrite(bg->BGCenter,4,nPixels,pFile);
	// signal that we've written the key frame
	ReleaseSemaphore(keyFrameWritten,1,NULL);

	Lock();
	nBGKeyFramesWritten++;
	Unlock();

	//if(stats) stats->updateTimings(UTT_NONE);
	return true;
}

// *** compression tools ***

bool ufmfWriter::addToBGModel(unsigned __int8 * frame, double timestamp, unsigned __int64 frameNumber){

	double dt = timestamp - lastBGUpdateTime;

	if((dt < BGUpdatePeriod) && (frameNumber >= nFramesInit)) {
		return true;
	}

	logger->log(UFMF_DEBUG_7,"Adding frame %d to background model counts\n",frameNumber);

	bg->addFrame(frame,timestamp);
	// store update time
	lastBGUpdateTime = timestamp;

	return true;
}

bool ufmfWriter::updateBGModel(unsigned __int8 * frame, double timestamp, unsigned __int64 frameNumber){

	// if the background hasn't been updated, no need to write a new keyframe
	if(lastBGUpdateTime <= lastBGKeyFrameTime){
		return;
	}

	// time since last keyframe
	double dt = timestamp - lastBGKeyFrameTime;
	double BGKeyFramePeriodCurr = BGKeyFramePeriod;
	unsigned __int64 nBGKeyFramesWrittenCurr;

	Lock();
	nBGKeyFramesWrittenCurr = nBGKeyFramesWritten;
	Unlock();

	if(nBGKeyFramesWrittenCurr > 0 && nBGKeyFramesWrittenCurr <= BGKeyFramePeriodInitLength){
		BGKeyFramePeriodCurr = BGKeyFramePeriodInit[nBGKeyFramesWrittenCurr-1];
	}

	// no need to write a new keyframe if it hasn't been long enough
	// TODO: change nInput != nFramesInit to nInput != BGKeyFramePeriodInit
	if((nBGKeyFramesWrittenCurr > 0) && (dt < BGKeyFramePeriodCurr)){// && (nInput != nFramesInit)){
		return;
	}

	logger->log(UFMF_DEBUG_7,"Updating background model at frame %d\n",frameNumber);
	
	// wait until the last key frame has been written
	if(WaitForSingleObject(keyFrameWritten,MAXWAITTIMEMS) != WAIT_OBJECT_0){
		logger->log(UFMF_ERROR,"Timeout waiting for last keyframe to be written.\n");
		return false;
	}
	// update the model
	bg->updateModel();

	Lock();

	// sanity check: no frames should need to be written that are still using bound0
	time_t startTime = time(NULL);
	while(nWritten < minFrameBGModel1){
		Unlock();
		Sleep(100);
		if(difftime(time(NULL),startTime) > MAXWAITTIMEMS/1000.0){
			logger->log(UFMF_ERROR,"Timeout waiting for all frames using background model 0 to be written");
			return false;
		}
		Lock();
	}

	lastBGKeyFrameTime = timestamp;

	// update using old buffer, which is bound0
	float tmp;
	for(int i = 0; i < nPixels; i++){
		tmp = ceil(bg->BGCenter[i] - backSubThresh);
		if(tmp < 0) BGLowerBound0[i] = 0;
		else if(tmp > 255) BGLowerBound0[i] = 255;
		else BGLowerBound0[i] = (unsigned __int8)tmp;
	}

	// swap the background subtraction images
	unsigned char * tmpSwap;
	tmpSwap = BGLowerBound0;
	BGLowerBound0 = BGLowerBound1;
	BGLowerBound1 = tmpSwap;
	tmpSwap = BGUpperBound0;
	BGUpperBound0 = BGUpperBound1;
	BGUpperBound1 = tmpSwap;

	// we start using model 1 at this frame
	minFrameBGModel1 = frameNumber;
	keyframeTimestamp = timestamp;

	Unlock();

	return true;

}

// *** threading tools ***

// create write thread
DWORD WINAPI ufmfWriter::writeThread(void* param){
	ufmfWriter* writer = reinterpret_cast<ufmfWriter*>(param);
	//MSG msg;
	//bool didwrite;

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
	
	// Signal that we are ready to begin writing
	ReleaseSemaphore(writer->writeThreadReadySignal, 1, NULL);  

	// Continuously capture and write frames to disk
	while(writer->ProcessNextWriteFrame())
		;

	//writer->Lock();
	//writer->finishWrite();
	//writer->Unlock();
	
	return 0;
}

// create compression thread
DWORD WINAPI ufmfWriter::compressionThread(void* param){
	ufmfWriter* writer = reinterpret_cast<ufmfWriter*>(param);
	int threadIndex;
	//MSG msg;
	//bool didwrite;

	// get index for this thread
	threadIndex = writer->threadCount++;

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
	
	// Signal that we are ready to begin writing
	ReleaseSemaphore(writer->compressionThreadReadySignals[threadIndex], 1, NULL);  

	// Continuously capture and write frames to disk
	while(writer->ProcessNextCompressFrame(threadIndex))
		;

	//writer->Lock();
	//writer->finishWrite();
	//writer->Unlock();
	
	return 0;
}

// create compression thread
DWORD WINAPI ufmfWriter::compressionThreadManager(void* param){
	ufmfWriter* writer = reinterpret_cast<ufmfWriter*>(param);
	//MSG msg;
	//bool didwrite;

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
	
	// Signal that we are ready to begin writing
	ReleaseSemaphore(writer->compressionThreadManagerReadySignal, 1, NULL);  

	// Continuously capture and write frames to disk
	while(writer->ProcessNextCompressFrame())
		;

	//writer->Lock();
	//writer->finishWrite();
	//writer->Unlock();
	
	return 0;
}

// compress next available uncompressed frame
bool ufmfWriter::ProcessNextCompressFrame() {

	// wait for an uncompressed frame
	int bufferIndex, threadIndex;
	DWORD waittime;
	bool res;

	Lock();
	// wait for any uncompressed frame to be empty
	if(isWriting){
		waittime = MAXWAITTIMEMS;
	}
	else{
		waittime = 100;
	}
	Unlock();

	bufferIndex = (int)WaitForMultipleObjects(nBuffers,uncompressedBufferFilledSemaphores,false,waittime) - (int)WAIT_OBJECT_0;
	if(bufferIndex < 0 || bufferIndex >= nBuffers){
		logger->log(UFMF_ERROR,"Error waiting for an uncompressed frame to compress: %x\n",bufferIndex+WAIT_OBJECT_0);
		return false;
	}

	logger->log(UFMF_DEBUG_7,"starting compression frame manager on buffer %d, frame %u\n",bufferIndex,uncompressedBufferFrameNumbers[bufferIndex]);

	Lock();
	// Check if we were signalled to stop compressing
	if(nUncompressedFramesBuffered == 0) {
		if(isWriting){
			Unlock();
			logger->log(UFMF_ERROR, "Something went wrong... Got signal to compress frame but no frames buffered and compress flag is still on\n");
		}
		else{
			Unlock();
		}
		return false;
	}
	Unlock();

	// find an available compression thread
	threadIndex = (int)WaitForMultipleObjects(nThreads,compressionThreadReadySignals,false,MAXWAITTIMEMS)- (int)WAIT_OBJECT_0;
	if(threadIndex < 0 || threadIndex >= nThreads){
		logger->log(UFMF_ERROR,"Error waiting for an compression thread to be ready: %x\n",threadIndex+WAIT_OBJECT_0);
		return false;
	}

	logger->log(UFMF_DEBUG_7,"sending buffer %d, frame %u to compression thread %d\n",bufferIndex,uncompressedBufferFrameNumbers[bufferIndex],threadIndex);


	// signal this thread
	Lock();
	threadBufferIndex[threadIndex] = bufferIndex;
	Unlock();
	ReleaseSemaphore(compressionThreadStartSignals[threadIndex],1,NULL);

	Lock();
	res = isWriting || nUncompressedFramesBuffered > 0;
	Unlock();
	return(res);

}

// compress frame queued for this thread
bool ufmfWriter::ProcessNextCompressFrame(int threadIndex) {

	int bufferIndex;
	unsigned __int64 frameNumber;
	bool res;

	// wait for start signal
	if(WaitForSingleObject(compressionThreadStartSignals[threadIndex],MAXWAITTIMEMS) != WAIT_OBJECT_0){
		logger->log(UFMF_ERROR,"Error waiting for start signal for thread %d\n",threadIndex);
		return false;
	}

	bufferIndex = threadBufferIndex[threadIndex];
	logger->log(UFMF_DEBUG_7,"starting compression on buffer %d, frame %u\n",bufferIndex,uncompressedBufferFrameNumbers[bufferIndex]);


	// Check if we were signalled to stop compressing
	Lock();
	if(nUncompressedFramesBuffered == 0) {
		if(isWriting) {
			Unlock();
			logger->log(UFMF_ERROR, "Something went wrong in thread %d... Got signal to compress frame but no frames buffered and compress flag is still on\n",threadIndex);
		}
		else{
			Unlock(); 
		}
		return false;
	}
	nUncompressedFramesBuffered--;
	Unlock();

	// compress this frame
	unsigned __int8 * BGLowerBoundCurr;
	unsigned __int8 * BGUpperBoundCurr;
	frameNumber = uncompressedBufferFrameNumbers[bufferIndex];
	Lock();
	if(frameNumber < minFrameBGModel1){
		logger->log(UFMF_DEBUG_7,"using bg model 0 to compress frame %d\n",frameNumber);
		BGLowerBoundCurr = BGLowerBound0;
		BGUpperBoundCurr = BGUpperBound0;
	}
	else{
		logger->log(UFMF_DEBUG_7,"using bg model 1 to compress frame %d\n",frameNumber);
		BGLowerBoundCurr = BGLowerBound1;
		BGUpperBoundCurr = BGUpperBound1;
	}
	Unlock();

	compressedFrames[bufferIndex]->setData(uncompressedFrames[bufferIndex],uncompressedBufferTimestamps[bufferIndex],
		frameNumber,BGLowerBoundCurr,BGUpperBoundCurr);

	Lock(); // lock for nCompressedFramesBuffered
	nCompressedFramesBuffered++;
	Unlock();

	// signal that the compressed buffer is filled
	ReleaseSemaphore(compressedBufferFilledSemaphores[bufferIndex],1,NULL);
	// signal that the uncompressed buffer is free
	ReleaseSemaphore(uncompressedBufferEmptySemaphores[bufferIndex],1,NULL);

	// signal that this thread is ready
	ReleaseSemaphore(compressionThreadReadySignals[threadIndex],1,NULL);

	Lock();
	res = isWriting || nUncompressedFramesBuffered > 0;
	Unlock();

	return(res);
}

bool ufmfWriter::ProcessNextWriteFrame(){

	int bufferIndex;
	unsigned __int64 frameNumber;
	int i;
	time_t startTime = time(NULL);

	Lock();
	nWritten++;
	frameNumber = nWritten;
	Unlock();

	logger->log(UFMF_DEBUG_7,"waiting for frame number %u to be compressed so that we can write it\n",frameNumber);

	while(true){

		// wait for any compressed frame to be filled
		bufferIndex = (int)WaitForMultipleObjects(nBuffers,compressedBufferFilledSemaphores,false,MAXWAITTIMEMS) - (int)WAIT_OBJECT_0;

		// Check if we were signalled to stop writing
		Lock(); // lock because we are accessing isWriting and nCompressedFramesBuffered
		if(nCompressedFramesBuffered == 0) {
			if(isWriting) {
				Unlock();
				logger->log(UFMF_ERROR, "Something went wrong... Got signal to write frame but no compressed frames buffered and write flag is still on\n");
			}
			else{
				Unlock(); 
			}
			return false;
		}
		Unlock();

		logger->log(UFMF_DEBUG_7,"got frame %u when waiting to write frame %u\n",compressedFrames[bufferIndex]->frameNumber,frameNumber);

		// is this the next frame to write?
		if(compressedFrames[bufferIndex]->frameNumber == frameNumber){
			Unlock();
			break;
		}
		Unlock();

		// wrong frame, restore semaphore
		ReleaseSemaphore(compressedBufferFilledSemaphores,1,NULL);

		// see if the compressed frame is available too
		Lock();
		bufferIndex = -1;
		for(int i = 0; i < nBuffers; i++){
			if(compressedFrames[bufferIndex]->frameNumber == frameNumber){
				bufferIndex = i;
				logger->log(UFMF_DEBUG_7,"also found frame %u after getting the signal from %u\n",frameNumber,compressedFrames[bufferIndex]->frameNumber);
				break;
			}
		}
		Unlock();

		// did we find it?
		if(bufferIndex >= 0){
			ReleaseSemaphore(compressedBufferFilledSemaphores[bufferIndex],-1,NULL);
			break;
		}

		if(difftime(time(NULL),startTime) > MAXWAITTIMEMS/1000.0){
			logger->log(UFMF_ERROR,"Timeout waiting for a compressed frame");
			return false;
		}
	}

	// okay, we have a compressed frame
	Lock(); // lock to access nCompressedFramesBuffered
	nCompressedFramesBuffered--;
	Unlock();

	// write background model if nec
	Lock();
	if(frameNumber == minFrameBGModel1){
		Unlock();
		writeBGKeyFrame();
	}
	else{
		Unlock();
	}

	// write the compressed frame
	if(!writeFrame(compressedFrames[bufferIndex])){
		logger->log(UFMF_ERROR,"Error writing frame %u from buffer %d\n",frameNumber,bufferIndex);
		return false;
	}

	// signal that the compressed buffer is free
	ReleaseSemaphore(compressedBufferEmptySemaphores[bufferIndex],1,NULL);

	Lock(); // lock to access isWriting and nCompressedFramesBuffered
	bool res = isWriting || nCompressedFramesBuffered > 0;
	Unlock();

	return(res);

}

bool ufmfWriter::stopThreads(bool waitForFinish){

	long value;

	logger->log(UFMF_DEBUG_7,"stopping threads\n");

	// no need to lock when reading isWriting as this is the only thread that will write to it
	if(isWriting){
		Lock();
		isWriting = false;
		Unlock();

		logger->log(UFMF_DEBUG_7,"stopping compression thread manager\n");
		if(_compressionThreadManager){
			if(!waitForFinish){
				Lock();
				nUncompressedFramesBuffered = 0;
				Unlock();
				for(int j = 0; j < nBuffers; j++){
					ReleaseSemaphore(uncompressedBufferFilledSemaphores[j], 1, NULL);
				}
			}
			if(WaitForSingleObject(_compressionThreadManager,MAXWAITTIMEMS) != WAIT_OBJECT_0){
				logger->log(UFMF_ERROR,"Error shutting down compression thread manager\n");
			}
			CloseHandle(_compressionThreadManager);
			_compressionThreadManager = NULL;
		}

		for(int i = 0; i < nThreads; i++){
			logger->log(UFMF_DEBUG_7,"stopping compression thread %d\n",i);
			if(_compressionThreads[i]){
				if(!waitForFinish){
					// set number of frames buffered to 0
					Lock();
					nUncompressedFramesBuffered = 0;
					Unlock();
					// increment semaphores so that the compression threads don't block forever. 
					// since nUncompressedFramesBuffered == 0 and isWriting == false, we won't try to 
					// compress a frame
					ReleaseSemaphore(compressionThreadStartSignals[i], 1, NULL);
				}
				if(WaitForSingleObject(_compressionThreads[i],MAXWAITTIMEMS) != WAIT_OBJECT_0){
					logger->log(UFMF_ERROR,"Error shutting down compression thread %d\n",i);
				}
				CloseHandle(_compressionThreads[i]);
				_compressionThreads[i] = NULL;
			}
		}

		logger->log(UFMF_DEBUG_7,"stopping write thread\n");
		if(_writeThread){
			if(!waitForFinish){
				// set number of frames buffered to 0
				Lock(); // lock for nCompressedFramesBuffered
				nCompressedFramesBuffered = 0;
				Unlock();
				// increment semaphores so that the compression threads don't block forever. 
				// since nCompressedFramesBuffered == 0 and isWriting == false, we won't try to 
				// compress a frame
				for(int j = 0; j < nBuffers; j++){
					ReleaseSemaphore(compressedBufferFilledSemaphores[j], 1, NULL);
				}
				if(WaitForSingleObject(_writeThread, MAXWAITTIMEMS) != WAIT_OBJECT_0){
					logger->log(UFMF_ERROR,"Error shutting down write thread\n");
				}
			}
			//Close thread handle
			CloseHandle(_writeThread);
			_writeThread = NULL;

		}
	}
}

// lock when accessing global data
bool ufmfWriter::Lock() { 
	if(WaitForSingleObject(lock, MAXWAITTIMEMS) != WAIT_OBJECT_0) { 
		logger->log(UFMF_ERROR,"Waited Too Long For Write Lock\n"); 
		return false;
	} 
	return true;
}
bool ufmfWriter::Unlock() { 
	ReleaseSemaphore(lock, 1, NULL); 
	return true;
}


void ufmfWriter::deallocateBuffers(){

	int i;
	if(uncompressedFrames != NULL){
		for(i = 0; i < nBuffers; i++){
			if(uncompressedFrames[i] != NULL){
				delete [] uncompressedFrames[i];
				uncompressedFrames[i] = NULL;
			}
		}
		delete [] uncompressedFrames;
		uncompressedFrames = NULL;
	}
	nUncompressedFramesBuffered = 0;

	if(compressedFrames != NULL){
		for(i = 0; i < nBuffers; i++){
			if(compressedFrames[i] != NULL){
				delete compressedFrames[i];
				compressedFrames[i] = NULL;
			}
		}
		delete [] compressedFrames;
		compressedFrames = NULL;
	}
	nCompressedFramesBuffered = 0;

	if(uncompressedBufferTimestamps != NULL){
		delete [] uncompressedBufferTimestamps;
		uncompressedBufferTimestamps = NULL;
	}

	if(uncompressedBufferFrameNumbers != NULL){
		delete [] uncompressedBufferFrameNumbers;
		uncompressedBufferFrameNumbers = NULL;
	}
}

void ufmfWriter::deallocateBGModel(){

	delete bg;
	bg = NULL;
	if(BGLowerBound0 != NULL){
		delete [] BGLowerBound0;
		BGLowerBound0 = NULL;
	}
	if(BGUpperBound0 != NULL){
		delete [] BGUpperBound0;
		BGUpperBound0 = NULL;
	}
	if(BGLowerBound1 != NULL){
		delete [] BGLowerBound1;
		BGLowerBound1 = NULL;
	}
	if(BGUpperBound1 != NULL){
		delete [] BGUpperBound1;
		BGUpperBound1 = NULL;
	}
}

void ufmfWriter::deallocateThreadStuff(){

	int i;
	if(_compressionThreads != NULL){
		for(i = 0; i < nThreads; i++){
			if(_compressionThreads[i]){
				CloseHandle(_compressionThreads[i]);
				_compressionThreads[i] = NULL;
			}
		}
		delete [] _compressionThreads;
		_compressionThreads = NULL;
	}

	if(_compressionThreadIDs != NULL){
		delete [] _compressionThreadIDs;
		_compressionThreadIDs = NULL;
	}

	if(compressionThreadReadySignals != NULL){
		for(i = 0; i < nThreads; i++){
			if(compressionThreadReadySignals[i]){
				CloseHandle(compressionThreadReadySignals[i]);
				compressionThreadReadySignals[i] = NULL;
			}
		}
		delete [] compressionThreadReadySignals;
		compressionThreadReadySignals = NULL;
	}

	 if(compressionThreadStartSignals != NULL){
		 for(i = 0; i < nThreads; i++){
			 if(compressionThreadStartSignals[i]){
				 CloseHandle(compressionThreadStartSignals[i]);
				 compressionThreadStartSignals[i] = NULL;
			 }
		 }
		 delete [] compressionThreadStartSignals;
		 compressionThreadStartSignals = NULL;
	 }

	 if(lock){
		CloseHandle(lock);
		lock = NULL;
	}

	if(uncompressedBufferEmptySemaphores != NULL){
		for(i = 0; i < nThreads; i++){
			if(uncompressedBufferEmptySemaphores[i]){
				CloseHandle(uncompressedBufferEmptySemaphores[i]);
				uncompressedBufferEmptySemaphores[i] = NULL;
			}
		}
		delete [] uncompressedBufferEmptySemaphores;
		uncompressedBufferEmptySemaphores = NULL;
	}

	if(uncompressedBufferFilledSemaphores != NULL){
		for(i = 0; i < nThreads; i++){
			if(uncompressedBufferFilledSemaphores[i]){
				CloseHandle(uncompressedBufferFilledSemaphores[i]);
				uncompressedBufferFilledSemaphores[i] = NULL;
			}
		}
		delete [] uncompressedBufferFilledSemaphores;
		uncompressedBufferFilledSemaphores = NULL;
	}

	if(compressedBufferEmptySemaphores != NULL){
		for(i = 0; i < nThreads; i++){
			if(compressedBufferEmptySemaphores[i]){
				CloseHandle(compressedBufferEmptySemaphores[i]);
				compressedBufferEmptySemaphores[i] = NULL;
			}
		}
		delete [] compressedBufferEmptySemaphores;
		compressedBufferEmptySemaphores = NULL;
	}

	if(compressedBufferFilledSemaphores != NULL){
		for(i = 0; i < nThreads; i++){
			if(compressedBufferFilledSemaphores[i]){
				CloseHandle(compressedBufferFilledSemaphores[i]);
				compressedBufferFilledSemaphores[i] = NULL;
			}
		}
		delete [] compressedBufferFilledSemaphores;
		compressedBufferFilledSemaphores = NULL;
	}

	if(threadBufferIndex != NULL){
		delete [] threadBufferIndex;
		threadBufferIndex = NULL;
	}

	if(keyFrameWritten != NULL){
		CloseHandle(keyFrameWritten);
		keyFrameWritten = NULL;
	}

	if(stats){
		delete stats;
		stats = NULL;
		logger->log(UFMF_DEBUG_3,"deleted stats\n");
	}
}
