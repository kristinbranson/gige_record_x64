#include "cv.h"
#include "highgui.h"
#include <PvApi.h>
#include <stdio.h>
#include "fmfWriter.h"

#define _STDCALL __stdcall

#define MAXNFRAMESENQUEUE 100
#define MAXSECONDSWAITFORCAMERA 20
#define CHARARRAYSIZE 256

typedef enum
{
	AVI = 0,
	FMF = 1,
	UFMF = 2
} VideoFormatType;


class GigeRecord{

public:

	// camera info

	// frame size
	unsigned long frameWidth;
	unsigned long frameHeight;
	unsigned long frameSize;
	// pixel format
	char pixelFormat[CHARARRAYSIZE];
	int bitDepth;
	int nChannels;

	// record time
	double recordTimeSeconds;

	// camera id
	unsigned long cameraUID;
	char cameraName[CHARARRAYSIZE];
	// camera handle
    tPvHandle cameraHandle;

	// camera state
	bool isAcquiring;

	// image buffer stuff

	// buffer indexes from firstFrameAvailable through firstFrameQueued-1 
	// are unused
	// buffer indexes processableStart through processableEnd-1 are storing data that needs to be processed
	unsigned long processableStart;
	unsigned long nFramesProcessable;
	// buffer indexes queueableStart through queueableEnd-1 can be requeued
	unsigned long queueableStart;
	unsigned long nFramesQueueable;
	unsigned long nFramesQueued;
	// counts
	unsigned long nFramesGrabbed;
	unsigned long nFramesProcessed;
	unsigned long nFramesDropped;
	// number of frames to buffer at max
	unsigned long nFramesBuffer;

	// opencv image buffer
	IplImage** imageBuffer;
	// pv image buffer
	tPvFrame** pFrameBuffer;
	unsigned long* timestampLoBuffer;
	unsigned long* timestampHiBuffer;

	// output file name
	char videoFileName[CHARARRAYSIZE];

	// color image for testing video out
	IplImage* colorImage;

	// video state
	bool videoOpen;
	bool headerWritten;
	bool footerWritten;
	CvVideoWriter *AVIwriter;
	fmfWriter* FMFwriter;
	VideoFormatType videoFormat;

	// start time
	time_t startTime;


	// whether pv has been initialize
	static bool pvIsInitialized;

	// log file stuff
	char logFileName[CHARARRAYSIZE];
	FILE * logFID;

	// params file stuff
	char cameraParamFileName[CHARARRAYSIZE];
	char experimentParamFileName[CHARARRAYSIZE];

	// constructor
	void initGigeRecord();
	GigeRecord();
	GigeRecord(const char* cameraParamFileName,const char* experimentParamFileName,const char* logFileName);

	// destructor
	~GigeRecord();

	// get a camera
	bool initializeCamera();
	bool uninitializeCamera();

	// read parameter file
	bool readCameraParamFile();
	bool readExperimentParamFile();
	bool writeCameraParamFile(const char* aFile);
	bool writeExperimentParamFile(FILE* lFile);

	// initialize buffers
	bool initializeBuffers();

	// initialize video writer
	bool initializeVideoWriter();
	bool closeVideoWriter();

	// frame grabbed callback
	friend void _STDCALL frameGrabbedCallback(tPvFrame* pFrame);

	// process a grabbed frame
	bool processFrame();

	bool startRecording();

	bool stopRecording();

	bool setCameraAttribute(const char* aLabel,tPvDatatype aType,char* aValue);
	bool getCameraAttribute(const char* aLabel,tPvDatatype aType,char* aString,unsigned long aLength);
	void writeAttribute(const char* aLabel,FILE* aFile);
	void readAttribute(char* aLine);
	void readExperimentAttribute(char* aLine);

	// print diagnostics
	void printInfo();

};


// trim the supplied string left and right
// from CamSetup.cpp in GigESDK examples
char* strtrim(char *aString)
{
    int i;
    int lLength = strlen(aString);
    char* lOut = aString;
    
    // trim right
    for(i=lLength-1;i>=0;i--)   
        if(isspace(aString[i]))
            aString[i]='\0';
        else
            break;
                
    lLength = strlen(aString);    
        
    // trim left
    for(i=0;i<lLength;i++)
        if(isspace(aString[i]))
            lOut = &aString[i+1];    
        else
            break;    
    
    return lOut;
}

// pv not initialized yet
bool GigeRecord::pvIsInitialized = false;

void GigeRecord::initGigeRecord(){

	// initialize camera info to unknown
	frameWidth = 0;
	frameHeight = 0;
	recordTimeSeconds = 60;
	bitDepth = 0;
	nChannels = 0;
	frameSize = 0;
	cameraUID = 0;
	strcpy(cameraName,"");

	// initialize video file name
	strcpy(videoFileName,"test.avi");

	// initialize buffer stuff to be empty
	nFramesBuffer = 1000;
	nFramesGrabbed = 0;
	nFramesProcessed = 0;
	nFramesDropped = 0;
	processableStart = 0;
	nFramesProcessable = 0;
	queueableStart = 0;
	nFramesQueueable = 0;
	nFramesQueued = 0;
	imageBuffer = NULL;
	pFrameBuffer = NULL;
	timestampLoBuffer = NULL;
	timestampHiBuffer = NULL;

	// initialize video state
	videoOpen = false;
	headerWritten = false;
	footerWritten = false;
	AVIwriter = NULL;
	videoFormat = AVI;

	// initialize camera state
	isAcquiring = false;

	// initialize log file
	strcpy(logFileName,"");
	logFID = stderr;

	// initialize Pv if not yet initialized
	if(!pvIsInitialized){
		if(PvInitialize()){ 
			printf("Error initializing Pv\n");
			exit(1);
		}
		pvIsInitialized = true;
	}

}

GigeRecord::GigeRecord(){

	initGigeRecord();

}

GigeRecord::GigeRecord(const char * cameraParamFileName, const char * experimentParamFileName, const char * logFileName){

	initGigeRecord();
	strcpy(this->cameraParamFileName,cameraParamFileName);
	strcpy(this->experimentParamFileName,experimentParamFileName);
	strcpy(this->logFileName,logFileName);
	if(strcmp(logFileName,"")){
		logFID = fopen(logFileName,"w");
		if(logFID == NULL){
			fprintf(stderr,"Error opening log file %s for writing\n",logFileName);
			logFID = stderr;
		}
		else{
			fprintf(stderr,"Diagnostic output written to file %s\n",logFileName);
		}
	}
	else{
		logFID = stderr;
	}
	if(!readExperimentParamFile()){
		fprintf(stderr,"Error reading experiment parameter file\n");
	}
}

bool GigeRecord::readCameraParamFile(){

	fprintf(logFID,"reading camera parameter file\n");

	FILE * lFile = fopen(cameraParamFileName,"r");

    if(lFile)
    {
        char lLine[CHARARRAYSIZE];
        
        while(!feof(lFile))
        {
            if(fgets(lLine,CHARARRAYSIZE,lFile)){
                readAttribute(lLine);
			}
        }
        
        fclose(lFile);
        
        return true;
    }
    else
        return false;
}
bool GigeRecord::readExperimentParamFile(){

	fprintf(logFID,"reading experiment parameter file\n");

	FILE * lFile = fopen(experimentParamFileName,"r");

    if(lFile)
    {
        char lLine[CHARARRAYSIZE];
        
        while(!feof(lFile))
        {
            if(fgets(lLine,CHARARRAYSIZE,lFile)){
                readExperimentAttribute(lLine);
			}
        }
        
        fclose(lFile);
        
        return true;
    }
    else
        return false;
}

bool GigeRecord::initializeBuffers(){

	if(nFramesBuffer == 0){
		fprintf(logFID,"nFramesBuffer == 0\n");
		return false;
	}

	// allocate array of opencv images
	imageBuffer = new IplImage*[nFramesBuffer];
	if(imageBuffer == NULL){
		fprintf(logFID,"Error allocating imageBuffer\n");
		return false;
	}

	// allocate array of pFrames
	pFrameBuffer = new tPvFrame*[nFramesBuffer];
	if(pFrameBuffer == NULL){
		fprintf(logFID,"Error allocate pFrameBuffer\n");
		return false;
	}

	// allocate each opencv image
	for(int i = 0; i < nFramesBuffer; i++){
		imageBuffer[i] = cvCreateImage(cvSize(frameWidth,frameHeight),bitDepth,nChannels);
		if(imageBuffer[i] == NULL){
			fprintf(logFID,"Error allocating imageBuffer[%d]\n",i);
			return false;
		}
	}
	colorImage = cvCreateImage(cvSize(frameWidth,frameHeight),bitDepth,3);

	// allocate each pFrame
	for(int i = 0; i < nFramesBuffer; i++){
		pFrameBuffer[i] = new tPvFrame();
		if(pFrameBuffer[i] == NULL){
			fprintf(logFID,"Error allocating pFrameBuffer[%d]\n",i);
			return false;
		}
        pFrameBuffer[i]->Context[0] = this;
		pFrameBuffer[i]->Context[1] = (void*)i;
		pFrameBuffer[i]->ImageBuffer = imageBuffer[i]->imageData;
		pFrameBuffer[i]->ImageBufferSize = frameSize;
	}

	// initialize timestamps
	timestampLoBuffer = new unsigned long[nFramesBuffer];
	if(timestampLoBuffer == NULL){
		fprintf(logFID,"Error allocating timestampLo buffer\n");
		return false;
	}
	timestampHiBuffer = new unsigned long[nFramesBuffer];
	if(timestampHiBuffer == NULL){
		fprintf(logFID,"Error allocating timestampHi buffer\n");
		return false;
	}

	queueableStart = 0;
	nFramesQueued = 0;
	nFramesQueueable = nFramesBuffer;

	processableStart = 0;
	nFramesProcessable = 0;

	return true;
}

bool GigeRecord::initializeVideoWriter(){

	// check for video file name
	if(!strcmp(videoFileName,"")){
		fprintf(logFID,"Video file name not set.\n");
		return false;
	}

	switch(videoFormat){

	case AVI:

		// get desired frame rate
		tPvFloat32 fps;
		tPvErr errorCode;
		errorCode = PvAttrFloat32Get(cameraHandle,"FrameRate",&fps);
		fprintf(logFID,"frameWidth = %lu, frameHeight = %lu, nChannels = %d, fps = %f\n",frameWidth,frameHeight,nChannels,fps);
		AVIwriter = cvCreateVideoWriter(videoFileName,CV_FOURCC('F','F','D','S'),
			(double)fps,cvSize(frameWidth,frameHeight),nChannels>=3);
		if(AVIwriter == NULL){
			fprintf(logFID,"Error allocating AVIwriter\n");
			return false;
		}

	case FMF:

		FMFwriter = new fmfWriter();
		return FMFwriter->startWrite(videoFileName,frameWidth,frameHeight,logFID);


	case UFMF:

	default:

		fprintf(logFID,"Unknown video format\n");
		return false;
	}

	return true;

}

bool GigeRecord::closeVideoWriter(){

	switch(videoFormat){
		
	case AVI:
	if(AVIwriter == NULL){
		fprintf(logFID,"AVIwriter is NULL\n");
		return false;
	}

	cvReleaseVideoWriter(&AVIwriter);

	case FMF:

		delete FMFwriter;

	case UFMF:

	default:

		fprintf(logFID,"Unknown video format\n");
		return false;

	}

	return true;
}

GigeRecord::~GigeRecord(){

	// uninitialize camera
	if(cameraHandle != NULL){
		if(!uninitializeCamera()){
			fprintf(stderr,"error uninitializing camera\n");
		}
		cameraHandle = NULL;
	}

	// deallocate image buffer
	if(imageBuffer != NULL){
		fprintf(stderr,"deallocating image buffer\n");
		for(int i = 0; i < nFramesBuffer; i++){
			if(imageBuffer[i] != NULL){
				cvReleaseImage(&imageBuffer[i]);
			}
			pFrameBuffer[i]->ImageBuffer = NULL;
			pFrameBuffer[i]->ImageBufferSize = 0;
		}
		delete [] imageBuffer;
		imageBuffer = NULL;
	}
	if(pFrameBuffer != NULL){
		fprintf(stderr,"deallocating pFrame buffer\n");
		for(int i = 0; i < nFramesBuffer; i++){
			if(pFrameBuffer[i] != NULL){
			//	delete pFrameBuffer[i];
				pFrameBuffer[i]->Context[0] = NULL;
				pFrameBuffer[i]->Context[1] = NULL;
			}
		}
		delete [] pFrameBuffer;
		pFrameBuffer = NULL;
	}
	if(timestampLoBuffer != NULL){
		fprintf(stderr,"deallocating timestamplo buffer\n");
		delete [] timestampLoBuffer;
		timestampLoBuffer = NULL;
	}
	if(timestampHiBuffer != NULL){
		fprintf(stderr,"deallocating timestamphi buffer\n");
		delete [] timestampHiBuffer;
		timestampHiBuffer = NULL;
	}
	if( (logFID != NULL) && strcmp(logFileName,"") ){
		fprintf(stderr,"closing log file\n");
		fclose(logFID);
		logFID = NULL;
	}

}

bool GigeRecord::initializeCamera(){

	tPvErr errCode;

	bool cameraDetected;
	// look for camera
    fprintf(logFID,"Waiting for a camera");
	for(int i = 0; i< MAXSECONDSWAITFORCAMERA*10; i++){
        fprintf(logFID,".");
        Sleep(100); // milliseconds
		if(PvCameraCount()){
			cameraDetected = true;
			break;
		}
    }
    fprintf(logFID,"\n");
	if(!cameraDetected){
		return false;
	}
	fprintf(logFID,"Camera detected\n");

	// get camera id
	tPvUint32 count,connected;
    tPvCameraInfo list;

    count = PvCameraList(&list,1,&connected);
    if(count){
        cameraUID = list.UniqueId;
		strcpy(cameraName,list.DisplayName);
        fprintf(logFID,"found camera serial=%s, uid=%u, name=%s\n",list.SerialString,cameraUID,cameraName);
    }
    else{
        return false;
	}

	// open camera
	errCode = PvCameraOpen(cameraUID,ePvAccessMaster,&cameraHandle);
	if(errCode){
		fprintf(logFID,"Error opening camera, code = %d\n",errCode);
		return false;
	}
	fprintf(logFID,"Camera opened\n");

	if(strcmp(cameraParamFileName,"")){
		fprintf(logFID,"setting paramFileName = %s\n",cameraParamFileName);
		if(!readCameraParamFile()){
			fprintf(stderr,"Error reading parameter file\n");
			exit(1);
		}
	}
	else{
		fprintf(logFID,"cameraParamFileName is empty, not reading camera parameters\n");
	}

	// get frame size
	errCode = PvAttrUint32Get(cameraHandle,"TotalBytesPerFrame",&frameSize);
	if(errCode){
		fprintf(logFID,"Error %d getting frame buffer size\n",errCode);
		return false;
	}

	errCode = PvAttrUint32Get(cameraHandle,"Width",&frameWidth);
	if(errCode){
		fprintf(logFID,"Error %d getting frame width\n",errCode);
		return false;
	}

	errCode = PvAttrUint32Get(cameraHandle,"Height",&frameHeight);
	if(errCode){
		fprintf(logFID,"Error %d getting frame height\n",errCode);
		return false;
	}

	errCode = PvAttrEnumGet(cameraHandle,"PixelFormat",pixelFormat,CHARARRAYSIZE,NULL);
	if(errCode){
		fprintf(logFID,"Error %d getting pixel format\n",errCode);
		return false;
	}
	else{
		if(!strcmp(pixelFormat,"Mono8")){
			bitDepth = IPL_DEPTH_8U;
			nChannels = 1;
		}
		else if(!strcmp(pixelFormat,"Mono16")){
			bitDepth = IPL_DEPTH_16U;
			nChannels = 1;
		}
		else if(!strcmp(pixelFormat,"RGB24")){
			bitDepth = IPL_DEPTH_8U;
			nChannels = 3;
		}
		else if(!strcmp(pixelFormat,"RGB48")){
			bitDepth = IPL_DEPTH_16U;
			nChannels = 3;
		}
		else{
			fprintf(logFID,"pixelFormat %s not handled\n",pixelFormat);
			return false;
		}
	}

	return true;

}

void GigeRecord::printInfo(){

	unsigned long Completed,Dropped,Done;
    unsigned long Missed,Errs;
    double Fps;
    float Rate;
    tPvErr Err;

	if((Err = PvAttrUint32Get(cameraHandle,"StatFramesCompleted",&Completed)) ||
		(Err = PvAttrUint32Get(cameraHandle,"StatFramesDropped",&Dropped)) ||
		(Err = PvAttrUint32Get(cameraHandle,"StatPacketsMissed",&Missed)) ||
		(Err = PvAttrUint32Get(cameraHandle,"StatPacketsErroneous",&Errs)) ||
		(Err = PvAttrFloat32Get(cameraHandle,"StatFrameRate",&Rate))){
			fprintf(logFID,"Error getting camera statistic %d\n",Err);
	}

	fprintf(logFID,"BUFFER: grabbed %05u, processed %05u, dropped %05u, qed %05u, qable %05u, pable %05u, qstart %05u, pstart %05u\n",
		nFramesGrabbed,nFramesProcessed,nFramesDropped,nFramesQueued,nFramesQueueable,nFramesProcessable,queueableStart,processableStart);
	fprintf(logFID,"CAMERA: completed %05u, dropped %05u, missed %05u, erroneous %05u, fps %05f\n",
		Completed,Dropped,Missed,Errs,Rate);
}

// callback called when a frame is done
void _STDCALL frameGrabbedCallback(tPvFrame* pFrame)
{
    //printf("[%03lu] Status = %02u, Timestamp = %u,%u\n",(unsigned long)pFrame->Context[0],pFrame->Status,pFrame->TimestampHi,pFrame->TimestampLo);
	// if the frame was completed we re-enqueue it
	GigeRecord * rec = (GigeRecord*)(pFrame->Context[0]);

	if(rec == NULL){
		fprintf(stderr,"frameGrabbedCallback when rec is NULL\n");
		return;
	}

	// removing something from the queue when this callback happens
	rec->nFramesQueued--;

	if(!rec->isAcquiring){
		fprintf(rec->logFID,"frameGrabbedCallback when not acquiring\n");
		return;
	}

	int j = (int)(pFrame->Context[1]);
    if(pFrame->Status != ePvErrUnplugged && pFrame->Status != ePvErrCancelled){

		// store timestamp
		unsigned long i = (rec->processableStart+rec->nFramesProcessable)%rec->nFramesBuffer;
		rec->timestampHiBuffer[i] = pFrame->TimestampHi;
		rec->timestampLoBuffer[i] = pFrame->TimestampLo;

		//fprintf(rec->logFID,"i = %u, context = %d\n",i,(int)pFrame->Context[1]);

		if(j != i){
			fprintf(rec->logFID,"context and i do not match\n");
		}

		if(pFrame != rec->pFrameBuffer[i]){
			fprintf(rec->logFID,"pFrame and pFrameBuffer[%d] do not match\n",i);
		}

		// more frames grabbed
		rec->nFramesGrabbed++;

		// new frame ready to be processed
		rec->nFramesProcessable++;

		//fprintf(rec->logFID,"FrameCount = %u, framesGrabbed = %u\n",pFrame->FrameCount,rec->nFramesGrabbed);
	}
	else{
		fprintf(rec->logFID,"frame at timestamp %u:%u was not completed, not requeuing\n",pFrame->TimestampHi,pFrame->TimestampLo);
	}

	//if(rec->nFramesQueueable > 0 && rec->nFramesQueued < MAXNFRAMESENQUEUE){
	//	PvCaptureQueueFrame(rec->cameraHandle,rec->pFrameBuffer[rec->queueableStart],frameGrabbedCallback);
	//	rec->queueableStart = (rec->queueableStart+1) % rec->nFramesBuffer;
	//	rec->nFramesQueueable--;
	//	rec->nFramesQueued++;
	//}


}

bool GigeRecord::processFrame(){

	//char fileName[CHARARRAYSIZE];

	if(nFramesProcessable == 0){
		return true;
	}

	// skip processing on this frame if we are behind
	if(nFramesQueued > 1 || !isAcquiring){

		//fprintf(logFID,"*******process start*******\n");

		if((nFramesProcessed%30)==0){
			printInfo();
		}

		switch(videoFormat){

		case AVI:

			//cvCvtColor(imageBuffer[processableStart],colorImage,CV_GRAY2RGB);

			//sprintf(fileName,"C:\\Code\\imaq\\gige_record_x64\\out\\test%05d.png",nFramesProcessed);
			//cvSaveImage(fileName,imageBuffer[processableStart]);
			cvWriteFrame(AVIwriter,imageBuffer[processableStart]);
			//cvWriteFrame(AVIwriter,colorImage);

		case FMF:

			return FMFwriter->addFrame(imageBuffer[processableStart]->imageData,
				timestampHiBuffer[processableStart],timestampLoBuffer[processableStart]);

		case UFMF:

		default:

			fprintf(logFID,"Unknown video format\n");
			return false;
		}

		nFramesProcessed++;
	}
	else{
		nFramesDropped++;
		fprintf(logFID,"Dropping frame\n");
	}

	// return this frame to queueable
	nFramesProcessable--;
	processableStart = (processableStart+1) % nFramesBuffer;
	nFramesQueueable++;

	// fill up the queue
	if(isAcquiring){
		while(nFramesQueueable > 0 && nFramesQueued < MAXNFRAMESENQUEUE){
			PvCaptureQueueFrame(cameraHandle,pFrameBuffer[queueableStart],frameGrabbedCallback);
			queueableStart = (queueableStart+1) % nFramesBuffer;
			nFramesQueueable--;
			nFramesQueued++;
		}
	}

	return true;

}

bool GigeRecord::startRecording(){

	if(!initializeVideoWriter()){
		fprintf(logFID,"Error initializing video writer\n");
		return false;
	}

	// initialize the image capture stream
	if(PvCaptureStart(cameraHandle)){
		fprintf(logFID,"Error starting capture\n");
		return false;
	}

	// todo: maybe allow other modes to be set in params file
	// set the camera in continuous acquisition mode
	if(PvAttrEnumSet(cameraHandle,"FrameStartTriggerMode","Freerun")){
		fprintf(logFID,"Error setting camera in continuous acquisition mode\n");
		return false;
	}

	// start acquisition
	if(PvCommandRun(cameraHandle,"AcquisitionStart")){
		// if that fails, we reset the camera to non capture mode
		fprintf(logFID,"Error starting acquisition\n");
		PvCaptureEnd(cameraHandle);
		return false;
	}

	// enqueue frames
	for(int i=0; i<MAXNFRAMESENQUEUE;i++){
		if(i>=nFramesBuffer){
			break;
		}
		if(PvCaptureQueueFrame(cameraHandle,pFrameBuffer[i],frameGrabbedCallback)){
			fprintf(logFID,"Error queueing frame %d\n",i);
			return false;
		}
		queueableStart = (queueableStart+1) % nFramesBuffer;
		nFramesQueueable--;
		nFramesQueued++;
	}

	time(&startTime);
	time_t currTime;
	double dt;
	isAcquiring = true;
	int nFailures = 0;
	while(true){
		time(&currTime);
		dt = difftime(currTime,startTime);
		if(dt > recordTimeSeconds){
			if(isAcquiring){
				stopRecording();
			}
			else if(nFramesProcessable == 0){
				fprintf(logFID,"finished processing all frames\n");
				break;
			}
		}
		if(!processFrame()){
			fprintf(logFID,"error processing frame %d\n",nFramesProcessed);
			return false;
		}
	}

	// pause for 1 second
	Sleep(1000);

	if(!closeVideoWriter()){
		fprintf(logFID,"Error closing video writer\n");
		return false;
	}

	return true;

}

bool GigeRecord::uninitializeCamera(){

	// dequeue all the frame still queued (this will block until they all have been dequeued)
	if(nFramesQueued > 0){
	    if(PvCaptureQueueClear(cameraHandle)){
			fprintf(logFID,"error clearing camera queue\n");
			return false;
		}
	}
	nFramesQueued = 0;
	if(PvCameraClose(cameraHandle)){
		fprintf(logFID,"error closing camera\n");
		return false;
	}

	cameraHandle = NULL;
	return true;
}

// stop streaming
bool GigeRecord::stopRecording()
{
    fprintf(logFID,"stopping streaming\n");
	isAcquiring = false;

    if(PvCommandRun(cameraHandle,"AcquisitionStop")){
		fprintf(logFID,"error sending acquisition stop\n");
		return false;
	}
    if(PvCaptureEnd(cameraHandle)){
		fprintf(logFID,"error ending capture\n");
		return false;
	}

	return true;
}


// set the value of a given attribute from a value encoded in a string
bool GigeRecord::setCameraAttribute(const char* aLabel,tPvDatatype aType,char* aValue)
{
    switch(aType)
    {           
        case ePvDatatypeString:
        {   
            if(!PvAttrStringSet(cameraHandle,aLabel,aValue))
                return true;
            else
                return false;     
        }
        case ePvDatatypeEnum:
        {            
            if(!PvAttrEnumSet(cameraHandle,aLabel,aValue))
                return true;
            else
                return false;
        }
        case ePvDatatypeUint32:
        {
            tPvUint32 lValue = atol(aValue);
            tPvUint32 lMin,lMax;
            
           if(!PvAttrRangeUint32(cameraHandle,aLabel,&lMin,&lMax))
           {
               if(lMin > lValue)
                   lValue = lMin;
               else
               if(lMax < lValue)
                   lValue = lMax;
                                        
               if(!PvAttrUint32Set(cameraHandle,aLabel,lValue))
                   return true;
               else
                   return false;
           }
           else
               return false;
        }
        case ePvDatatypeFloat32:
        {
            tPvFloat32 lValue = (tPvFloat32)atof(aValue);
            tPvFloat32 lMin,lMax;
            
           if(!PvAttrRangeFloat32(cameraHandle,aLabel,&lMin,&lMax))
           {
                if(lMin > lValue)
                   lValue = lMin;
                else
                if(lMax < lValue)
                   lValue = lMax;            
            
                if(!PvAttrFloat32Set(cameraHandle,aLabel,lValue))
                    return true;
                else
                    return false;
           }
           else
               return false;
        }
        default:
            return false;
    }       
}

// encode the value of a given attribute in a string
bool GigeRecord::getCameraAttribute(const char* aLabel,tPvDatatype aType,char* aString,unsigned long aLength)
{   

	tPvErr errorCode;

    switch(aType)
    {           
        case ePvDatatypeString:
        {   
			errorCode = PvAttrStringGet(cameraHandle,aLabel,aString,aLength,NULL);
			fprintf(stderr,"grabbing %s -> errorCode %s\n",aLabel,errorCode);
            if(!errorCode)
                return true;
            else
                return false;     
        }
        case ePvDatatypeEnum:
        {       
			errorCode = PvAttrEnumGet(cameraHandle,aLabel,aString,aLength,NULL);
			fprintf(stderr,"grabbing %s -> errorCode %s\n",aLabel,errorCode);
            if(!errorCode)
                return true;
            else
                return false;
        }
        case ePvDatatypeUint32:
        {
            tPvUint32 lValue;
            errorCode = PvAttrUint32Get(cameraHandle,aLabel,&lValue);
			fprintf(stderr,"grabbing %s -> errorCode %s\n",aLabel,errorCode);
            if(!errorCode)
            {
                sprintf(aString,"%lu",lValue);
                return true;
            }
            else
                return false;
            
        }
        case ePvDatatypeFloat32:
        {
            tPvFloat32 lValue;
            errorCode = PvAttrFloat32Get(cameraHandle,aLabel,&lValue);
			fprintf(stderr,"grabbing %s -> errorCode %s\n",aLabel,errorCode);
            if(!errorCode)
            {
                sprintf(aString,"%g",lValue);
                return true;
            }
            else
                return false;
        }
        default:
            return false;
    }        
}


// write a given attribute in a text file
void GigeRecord::writeAttribute(const char* aLabel,FILE* aFile)
{
    tPvAttributeInfo lInfo;

    if(!PvAttrInfo(cameraHandle,aLabel,&lInfo))
    {
        if(lInfo.Datatype != ePvDatatypeCommand &&
           (lInfo.Flags & ePvFlagWrite))
        {
            char lValue[CHARARRAYSIZE];
            if(getCameraAttribute(aLabel,lInfo.Datatype,lValue,CHARARRAYSIZE))
                fprintf(aFile,"%s = %s\n",aLabel,lValue);
            else
                fprintf(stderr,"attribute %s couldn't be saved\n",aLabel);            
        }   
    }
}

// read the attribute from one of the file's text line
void GigeRecord::readAttribute(char* aLine)
{
    char* lValue = strchr(aLine,'=');
    char* lLabel;

    if(lValue)
    {
        lValue[0] = '\0';
        lValue++;    
    
        lLabel = strtrim(aLine);
        lValue = strtrim(lValue);
        
        if(strlen(lLabel) && strlen(lValue))
        {
            tPvAttributeInfo lInfo;
                           
            if(!PvAttrInfo(cameraHandle,lLabel,&lInfo))
            {
                if(lInfo.Datatype != ePvDatatypeCommand &&
                (lInfo.Flags & ePvFlagWrite))
                {
					fprintf(logFID,"setting %s to %s\n",lLabel,lValue);
                    if(!setCameraAttribute(lLabel,lInfo.Datatype,lValue))
                        fprintf(stderr,"attribute %s couldn't be loaded\n",lLabel);                          
                } 
			}
			else{
				fprintf(logFID,"no attribute info for %s\n",lLabel);
			}
        }
    }
}

// read the attribute from one of the file's text line
void GigeRecord::readExperimentAttribute(char* aLine)
{
    char* lValue = strchr(aLine,'=');
    char* lLabel;

    if(lValue)
    {
        lValue[0] = '\0';
        lValue++;    
    
        lLabel = strtrim(aLine);
        lValue = strtrim(lValue);
        
        if(strlen(lLabel) && strlen(lValue))
        {
			if(!strcmp(lLabel,"recordTimeSeconds")){
				recordTimeSeconds = atof(lValue);
			}
			else if(!strcmp(lLabel,"videoFileName")){
				strcpy(videoFileName,lValue);
			}
			else if(!strcmp(lLabel,"nFramesBuffer")){
				nFramesBuffer = atof(lValue);
			}
			else{
				fprintf(logFID,"Unknown experiment parameter %s\n",videoFileName);
			}
		}
    }
}


bool GigeRecord::writeExperimentParamFile(FILE* lFile){

	//FILE* lFile;
	//if(!strcmp(aFile,"")){
	//	lFile = stdout;
	//}
	//else{
	//    lFile = fopen(aFile,"w+");
	//	if(lFile == NULL)
	//		return false;
	//}
	fprintf(lFile,"recordTimeSeconds: %f\n",recordTimeSeconds);
	fprintf(lFile,"videoFileName: %s\n",videoFileName);
	//if(!strcmp(aFile,"")){
	//	fclose(lFile);
	//}
	return true;
}

// save the setup of a camera from the given file
bool GigeRecord::writeCameraParamFile(const char* aFile)
{
	FILE* lFile;
	if(!strcmp(aFile,"")){
		lFile = stdout;
	}
	else{
	    lFile = fopen(aFile,"w+");
	}
    
    if(lFile)
    {
        bool            lRet = true;
        tPvAttrListPtr  lAttrs; 
        tPvUint32       lCount;    
        
        if(!PvAttrList(cameraHandle,&lAttrs,&lCount))
        {
            for(tPvUint32 i=0;i<lCount;i++)
                writeAttribute(lAttrs[i],lFile);
        }     
        else
            lRet = false;   
        
		if(!strcmp(aFile,"")){
		    fclose(lFile);
		}

        return lRet;
    }
    else
        return false;    
}


bool printUsage(){

	printf("Usage: gige_record_x64.exe cameraParams.txt experimentParams.txt\n");
	GigeRecord * rec = new GigeRecord();
	printf("Camera parameters:\n");
	if(!rec->writeCameraParamFile("")){
		fprintf(stderr,"Error writing camera parameters\n");
		return false;
	}
	printf("\nExperiment parameters:\n");
	if(!rec->writeExperimentParamFile(stdout)){
		fprintf(stderr,"Error writing experiment parameters\n");
		return false;
	}
	delete rec;
	return true;
}

int main(int argc, char* argv[]){

	GigeRecord *rec;
	char cameraParamFileName[CHARARRAYSIZE];
	char experimentParamFileName[CHARARRAYSIZE];
	char logFileName[CHARARRAYSIZE];
	char mode[CHARARRAYSIZE];
	bool success;
	double dt;

	if((argc > 1) && !strcmp(argv[1],"--help")){
		success = printUsage();
		getc(stdin);
		return !success;
	}

	if(argc > 1){
		strcpy(cameraParamFileName,argv[1]);
	}
	else{
		strcpy(cameraParamFileName,"C:\\Code\\imaq\\gige_record_x64\\testCameraParams.txt");
	}
	if(argc > 2){
		strcpy(experimentParamFileName,argv[2]);
	}
	else{
		strcpy(experimentParamFileName,"C:\\Code\\imaq\\gige_record_x64\\testExperimentParams.txt");
	}
	if(argc > 3){
		strcpy(logFileName,argv[3]);
	}
	else{
		strcpy(logFileName,"C:\\Code\\imaq\\gige_record_x64\\out\\log.txt");
	}

	rec = new GigeRecord(cameraParamFileName,experimentParamFileName,logFileName);
	if(!rec->initializeCamera()){
		fprintf(stderr,"Error initializing camera\n");
		getc(stdin);
		return 1;
	}

	if(!rec->initializeBuffers()){
		fprintf(stderr,"Error initializing buffers\n");
		getc(stdin);
		return 1;
	}
	if(!rec->startRecording()){
		fprintf(stderr,"Error recording\n");
		getc(stdin);
		return 1;
	}
	fprintf(stderr,"Successfully completed recording.\n");
	if(!rec->uninitializeCamera()){
		fprintf(stderr,"Error uninitializing camera\n");
		getc(stdin);
		return 1;
	}
	fprintf(stderr,"Uninitialized camera\n");

	delete rec;

	fprintf(stderr,"Finished deallocating\n");

	// uninitialise the API: this seems to cause things to crash!
	PvUnInitialize();
	fprintf(stderr,"Uninitialzed Pv API\n");

	fprintf(stderr,"Enter any character to exit: \n");

	getc(stdin);

	return 0;
}