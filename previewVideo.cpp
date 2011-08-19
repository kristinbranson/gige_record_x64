#include "previewVideo.h"

previewVideo::previewVideo(HANDLE lock){

	this->lock = lock;
	this->frame = NULL;
	frameNumber = 0;
	frameCopy = NULL;
	frameSize = 0;
	lastFrameNumber = 0;

	previewThreadReadySignal = CreateSemaphore(NULL,0,1,NULL);
	_previewThread = CreateThread(NULL,0,previewThread,this,0,&_previewThreadID);
	if (_previewThread == NULL){ 
		fprintf(stderr,"Error starting Preview Thread\n"); 
		return;
	}
	if(WaitForSingleObject(previewThreadReadySignal, 1000) != WAIT_OBJECT_0) { 
		fprintf(stderr,"Error Starting Preview Thread\n"); 
		return; 
	}

	isRunning = true;
}

bool previewVideo::stop(){
	if(!Lock()){
		return false;
	}
	isRunning = false;
	Unlock();
	if(WaitForSingleObject(_previewThread,1000) != WAIT_OBJECT_0){
		fprintf(stderr,"timeout waiting for preview thread to finish\n");
		return false;
	}
	return true;
}

bool previewVideo::Lock(){
	return( WaitForSingleObject(lock,1000) == WAIT_OBJECT_0 );
}

bool previewVideo::Unlock(){
	ReleaseSemaphore(lock,1,NULL);
	return true;
}

bool previewVideo::setFrame(IplImage * frame, unsigned __int64 frameNumber){

	Lock();
	this->frame = frame;
	this->frameNumber = frameNumber;
	Unlock();
	return isRunning;
}

previewVideo::~previewVideo(){
	isRunning = false;
	if(frameCopy){
		cvReleaseImage(&frameCopy);
		frameCopy = NULL;
	}
}
	
bool previewVideo::ProcessNextPreview(){

	if(!Lock()){
		fprintf(stderr,"preview timeout\n");
		return isRunning;
	}

	if(!isRunning){
		fprintf(stderr,"not compressing\n");
		Unlock();
		return false;
	}

	if(frame == NULL || frameNumber == lastFrameNumber){
		Unlock();
		return true;
	}

	if(frameCopy == NULL){
		frameCopy = cvCloneImage(frame);
		frameSize = frame->imageSize;
	}
	else if(frameCopy->imageSize != frame->imageSize){
		cvReleaseImage(&frameCopy);
		frameCopy = cvCloneImage(frame);
	}
	else{
		memcpy(frameCopy->imageData,frame->imageData,frameSize);
	}
	lastFrameNumber = frameNumber;
	Unlock();

	//if(frameNumber % 100 == 0) fprintf(stderr,"Showing frame %llu\n",frameNumber);
	cvShowImage( "Preview", frameCopy );
	char c = cvWaitKey(1);
	if(c == 27){
		if(!Lock()){
			fprintf(stderr,"preview stop timeout\n");
			return false;
		}
		isRunning = false;
		Unlock();
		return false;
	}

	return true;

}

// create preview thread
DWORD WINAPI previewVideo::previewThread(void* param){
	previewVideo * pv = reinterpret_cast<previewVideo*>(param);

	cvNamedWindow( "Preview", CV_WINDOW_AUTOSIZE );

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
	
	// Signal that we are ready to begin writing
	ReleaseSemaphore(pv->previewThreadReadySignal, 1, NULL);  

	// Continuously capture and write frames to disk
	while(pv->ProcessNextPreview())
		;

	fprintf(stderr,"preview thread terminated\n");
	
	cvDestroyWindow("Preview");

	return 0;
}