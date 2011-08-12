#ifndef __UFMFWRITER_H
#define __UFMFWRITER_H

#include "windows.h"
#include "ufmfWriterStats.h"
#include "ufmfLogger.h"
#include <vector>
#include <math.h>
#include <time.h>
#define MAXWAITTIMEMS 10000

class BackgroundModel {

public:

	void init();
	BackgroundModel();
	BackgroundModel(unsigned __int32 nPixels, int minNFramesReset = 200);
	~BackgroundModel();
	bool addFrame(unsigned char * im, double timestamp);
	bool updateModel();

private:

	// parameters
	int minNFramesReset; // minimum number of frames that must have been added to the model before we reset the counts
	int BGNBins;
	int BGBinSize;
	float BGHalfBin;

	int nPixels; // frame size

	// counts
	unsigned __int64 nFramesAdded; // Number of frames added to the background model

	// buffers
	unsigned __int8 ** BGCounts; // counts per bin: note the limited resolution
	float * BGCenter; // current background model
	float BGZ;

	friend class ufmfWriter;

};

// class to hold buffered compressed frames
class CompressedFrame {

public:

	void init();
	CompressedFrame();
	CompressedFrame(unsigned short wWidth, unsigned short wHeight, unsigned __int32 boxLength = 30, double maxFracFgCompress = 1.0);
	bool setData(unsigned __int8 * im, double timestamp, unsigned __int64, 
		unsigned __int8 * BGLowerBound, unsigned __int8 * BGUpperBound);
	~CompressedFrame();

private:

	unsigned short wWidth; //Image Width
	unsigned short wHeight; //Image Height
	int nPixels;
	bool * isFore; // whether each pixel is foreground or not
	//bool * debugWasFore; // TODO: remove after debugging
	unsigned __int16 * writeRowBuffer; // ymins
	unsigned __int16 * writeColBuffer; // xmins
	unsigned __int16 * writeHeightBuffer; // heights
	unsigned __int16 * writeWidthBuffer; // widths
	unsigned __int8 * writeDataBuffer; // image data
	unsigned __int16 * nWrites; // number of times a pixel has been written
	unsigned __int32 ncc;
	double timestamp;
	unsigned __int64 frameNumber;

	// parameters
	unsigned __int32 boxLength; // length of boxes of foreground pixels to store
	int boxArea; // boxLength^2
	double maxFracFgCompress; // max fraction of pixels that can be foreground in order for us to compress
	int maxNFgCompress; // max number of pixels that can be foreground in order for us to compress


	friend class ufmfWriter;

};

// main ufmf writer class
class ufmfWriter {
public:

	// constructors

	// common code for both the empty constructor and the parameter-filled constructor
	void init();

	// empty constructor:
	// initializes values to defaults
	ufmfWriter();

	// parameter-filled constructor
	// set parameters
	// allocate buffers
	//
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
	// [compression stats parameters:]
	// statFileName: name of file to write compression statistics to. If NULL, then statistics are combined into debug file
	// printStats: whether to print compression statistics
	// statStreamPrintFreq: number of frames between outputting per-frame compression statistics
	// statPrintFrameErrors: whether to compute and print statistics of compression error. Currently, box-averaged and per-pixel errors are either both
	// computed or both not computed. 
	// statPrintTimings: whether to print information about the time each part of the computation takes. 
	ufmfWriter(const char * fileName, unsigned __int32 pWidth, unsigned __int32 pHeight, FILE* logFID, unsigned __int32 nBuffers = 10,
		int MaxBGNFrames = 100, double BGUpdatePeriod = 1.0, double BGKeyFramePeriod = 100, unsigned __int32 boxLength = 30,
		double backSubThresh = 10.0, unsigned __int32 nFramesInit = 100, double* BGKeyFramePeriodInit = NULL, int BGKeyFramePeriodInitLength = 0, 
		double maxFracFgCompress = 1.0, const char *statFileName=NULL, bool printStats=true, int statStreamPrintFreq=1, bool statPrintFrameErrors=true, 
		bool statPrintTimings=true, int statComputeFrameErrorFreq=1, int nThreads=4);


	// destructor
	~ufmfWriter();

	// public API

	// start writing
	// open file
	// write header
	// start threads
	bool startWrite();

	// stop writing
	// close all threads
	// deallocate buffers
	// write footers
	// close file
	unsigned __int64 stopWrite();

	// add a frame to be processed
	bool addFrame(unsigned char * frame, double timestamp);

	// set video file name, width, height
	// todo: resize buffers if already allocated
	void setVideoParams(char * fileName, int wWidth, int wHeight);

	// set video compression parameters
	bool readParamsFile(char * paramsFile);

	// read stats params from a file
	void setStatsParams(const char * statsName);

    // get number of frames written
	unsigned __int64 NumWritten() { return nWritten; }

private:

	// ***** helper functions *****

	// *** writing tools ***

	// write a frame
	bool writeFrame(CompressedFrame * im);

	// write the video header
	bool writeHeader();

	// finish writing
	bool finishWriting();

	// write a background keyframe to file
	bool writeBGKeyFrame();

	// *** compression tools ***

	// add to bg model counts
	bool addToBGModel(unsigned __int8 * frame, double timestamp, unsigned __int64 frameNumber);

	// reset background model
	bool updateBGModel(unsigned __int8 * frame, double timestamp, unsigned __int64 frameNumber);

	// *** threading tools ***

	// start writeThread
	static DWORD WINAPI writeThread(void* param);  //write function declaration

	// start compression thread
	static DWORD WINAPI compressionThread(void* param);  //compression function declaration

	// start compression thread manager
	static DWORD WINAPI compressionThreadManager(void* param);  //compression function declaration

	// compress next available frame
	bool ProcessNextCompressFrame();

	// compress frame available in buffer threadBufferIndex[threadIndex] with thread threadIndex 
	bool ProcessNextCompressFrame(int threadIndex);

	// write next frame
	bool ProcessNextWriteFrame();

	// stop all threads
	bool stopThreads(bool waitForFinish);

	// lock when accessing global data
	bool Lock();
	bool Unlock();

	// deallocate buffers
	void deallocateBuffers();

	// deallocate background model
	void deallocateBGModel();

	// deallocate and release all thread stuff
	void deallocateThreadStuff();

	// ***** state *****

	// *** output ufmf state ***

	FILE * pFile; //File Target
	unsigned __int64 indexLocation; // Location of index in file
	unsigned __int64 indexPtrLocation; // Location in file of pointer to index location
	std::vector<__int64> index; // Location of each frame in the file
	std::vector<__int64> meanindex; // Location of each bg center in the file
	std::vector<double> index_timestamp; // timestamp of each frame in the file
	std::vector<double> meanindex_timestamp; // timestamp for each bg center in the file

	// *** writing state ***

	unsigned __int64 nGrabbed; // Number of frames for which addframe  has been called
	unsigned __int64 nWritten; //Track number of frames written to disk
	unsigned __int64 nBGKeyFramesWritten; // Number of background images written
	bool isWriting; // Whether we are still compressing, still writing

	// *** threading/buffering state ***

	// buffer for grabbed, uncompressed frames
	unsigned char ** uncompressedFrames;
	// timestamps of buffered frames
	double * uncompressedBufferTimestamps;
	// buffer for compressed frames
	CompressedFrame ** compressedFrames;
	// number of uncompressed frames in the buffer
	int nUncompressedFramesBuffered;
	// number of compressed frames in the buffer
	int nCompressedFramesBuffered;
	unsigned __int64 * uncompressedBufferFrameNumbers; // which grabbed frame is stored in each buffer -- used for writing frames in order

	HANDLE _writeThread; // write ThreadVariable
	DWORD _writeThreadID; //write thread ID returned by Windows
	HANDLE* _compressionThreads; // compression ThreadVariables
	HANDLE _compressionThreadManager; // compression manager ThreadVariables
	DWORD* _compressionThreadIDs; //compression thread IDs returned by Windows
	DWORD _compressionThreadManagerID; // compression thread manager ID returned by windows
	int threadCount; // current number of compression threads
	HANDLE writeThreadReadySignal; // signal that write thread is set up
	HANDLE * compressionThreadReadySignals; // signals that compression threads are set up and ready to process a frame
	HANDLE compressionThreadManagerReadySignal; // signal that the compression manager thread is set up
	HANDLE * compressionThreadStartSignals; // signals to compression threads to start processing a frame
	HANDLE lock; // semaphore for keeping different threads from accessing the same global variables at the same time
	// note that these two semaphores can both be 0, indicating that the uncompressed buffer is either being filled or emptied
	HANDLE* uncompressedBufferEmptySemaphores; // signal that the uncompressed buffer is empty
	HANDLE* uncompressedBufferFilledSemaphores; 
	// note that these two semaphores can both be 0, indicating that the compressed buffer is either being filled or emptied
	HANDLE* compressedBufferEmptySemaphores; // signal that the compressed buffer is empty
	HANDLE* compressedBufferFilledSemaphores; // signal that the compressed buffer is filled
	int * threadBufferIndex; // which buffer each thread works on
	HANDLE keyFrameWritten; // whether the last computed key frame has been written

	// *** background subtraction state ***

	//unsigned __int64 nBGUpdates; // Number of updates the background model
	//int BGNFrames; // approx number of frames in background computation so far
	double lastBGUpdateTime; // last time the background was updated
	double lastBGKeyFrameTime; // last time a keyframe was written
	BackgroundModel * bg;
	// lower and upper bounds available for thresholding
	unsigned __int8 * BGLowerBound0; // per-pixel lower bound on background
	unsigned __int8 * BGUpperBound0; // per-pixel upper bound on background
	unsigned __int8 * BGLowerBound1; // per-pixel lower bound on background
	unsigned __int8 * BGUpperBound1; // per-pixel upper bound on background
	unsigned __int64 minFrameBGModel1; // first frame that can be used with background model 1
	double keyframeTimestamp; // timestamp for this key frame
	//unsigned __int8 ** BGCounts; // counts per bin: note the limited resolution
	//float * BGCenter; // current background model
	//unsigned __int8 * BGLowerBound; // per-pixel lower bound on background
	//unsigned __int8 * BGUpperBound; // per-pixel upper bound on background
	//float BGZ;

	// *** logging state ***

	ufmfWriterStats * stats;
	FILE * logFID;
	ufmfLogger * logger;

	// ***** parameters *****

	// *** threading parameters ***

	int nThreads; // number of compression threads
	unsigned __int32 nBuffers; // number of frames buffered

	// *** video parameters ***

	char fileName[1000]; // output video file name
	unsigned short wWidth; //Image Width
	unsigned short wHeight; //Image Height
	int nPixels; // Number of pixels in image
	char colorCoding[10]; // video color format
	unsigned __int8 colorCodingLength;

	// *** compression parameters ***

	// * background subtraction parameters *

	int MaxBGNFrames; // approximate number of frames used in background computation
	double BGUpdatePeriod; // seconds between updates to the background model
	double BGKeyFramePeriod; // seconds between background keyframes
	float backSubThresh; // threshold above which we store a pixel
	unsigned __int32 nFramesInit; // for the first nFramesInit, we will always update the background model
	double BGKeyFramePeriodInit[100]; // seconds before we output a new background model initially while ramping up the background model
	int BGKeyFramePeriodInitLength;
	int nBGUpdatesPerKeyFrame;
	float MaxBGZ;
	//int BGBinSize;
	//int BGNBins; 
	//float BGHalfBin;

	// * ufmf parameters *
	unsigned __int8 isFixedSize; // whether patches are of a fixed size
	unsigned __int32 boxLength; // length of boxes of foreground pixels to store
	double maxFracFgCompress; // max fraction of pixels that can be foreground in order for us to compress

	// chunk identifiers
	static const unsigned __int8 KEYFRAMECHUNK = 0;
	static const unsigned __int8 FRAMECHUNK = 1;
	static const unsigned __int8 INDEX_DICT_CHUNK = 2;

	// *** statistics parameters ***
	char statFileName[256];
	bool printStats;
	int statStreamPrintFreq;
	bool statPrintFrameErrors;
	bool statPrintTimings; 
	int statComputeFrameErrorFreq;

};

#endif