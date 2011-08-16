#ifndef __PREVIEWVIDEO_H
#define __PREVIEWVIDEO_H

#include "windows.h"
#include "cv.h"
#include "highgui.h"
#include <stdio.h>

class previewVideo{

public:

	bool isRunning;
	previewVideo(HANDLE lock);
	~previewVideo();
	bool setFrame(IplImage * frame, unsigned __int64 frameNumber);
	bool stop();

private:

	bool ProcessNextPreview();
	static DWORD WINAPI previewThread(void* param);
	bool Lock();
	bool Unlock();

	HANDLE lock;
	HANDLE _previewThread;
	DWORD _previewThreadID;
	HANDLE previewThreadReadySignal;
	IplImage * frame, * frameCopy;
	size_t frameSize;
	unsigned __int64 frameNumber, lastFrameNumber;

};


#endif