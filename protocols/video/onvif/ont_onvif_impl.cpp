#include <stdlib.h>
#include <string>
#include "device_onvif.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "librtmp/rtmp.h"

#include "ont/video.h"
#include "ont_onvif_if.h"


#include "ont_onvif_videoandiohandler.h"

using namespace std;


void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

void subsessionAfterPlaying(void* clientData);
void subsessionByeHandler(void* clientData);
void streamTimerHandler(void* clientData);

// The main streaming routine (for each "rtsp://" URL):
static void* openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

class StreamClientState;


// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
    return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
    return env << subsession.mediumName() << "/" << subsession.codecName();
}




// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
    StreamClientState();
    virtual ~StreamClientState();

public:
    MediaSubsessionIterator* iter;
    MediaSession* session;
    MediaSubsession* subsession;
    TaskToken streamTimerTask;
    H264VideoStreamFramer *streamFramer;
    double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient : public RTSPClient {
public:
    static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
        int verbosityLevel = 0,
        char const* applicationName = NULL,
        portNumBits tunnelOverHTTPPortNum = 0);
protected:
    ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
        int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~ourRTSPClient();

public:
    StreamClientState scs;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"


static void* openURL(UsageEnvironment& env, char const* progName, char const* rtspURL) {
    // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
    // to receive (even if more than stream uses the same "rtsp://" URL).
    RTSPClient* rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
    if (rtspClient == NULL) {
        env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
        return NULL;
    }

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
    return rtspClient;
}

// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) 
{
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
            delete[] resultString;
            break;
        }

        char* const sdpDescription = resultString;
        env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
            env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
            break;
        }
        else if (!scs.session->hasSubsessions()) {
            env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
            break;
        }


        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP TRUE


void setupNextSubsession(RTSPClient* rtspClient) {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        }
        else {
            env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
            if (scs.subsession->rtcpIsMuxed()) {
                env << "client port " << scs.subsession->clientPortNum();
            }
            else {
                env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
            }
            env << ")\n";

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
			//rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, FALSE);
        }
        return;
    }

    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    }
    else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
            break;
        }

        env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
        if (scs.subsession->rtcpIsMuxed()) {
            env << "client port " << scs.subsession->clientPortNum();
        }
        else {
            env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
        }
        env << ")\n";

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        //
        scs.subsession->sink = ONTVideoAudioSink::createNew(env, 
            *scs.subsession,
            rtspClient->url());

        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
                << "\" subsession: " << env.getResultMsg() << "\n";
            break;
        }

        env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
        scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession 
        //check the type.
#if RTPVIDEOSUBCLASS
        scs.streamFramer = H264VideoStreamFramer::createNew(env, scs.subsession->readSource(), 1);
#endif
        scs.subsession->sink->startPlaying(
#if RTPVIDEOSUBCLASS
            *scs.streamFramer,
#else
            *scs.subsession->readSource(),
#endif
            subsessionAfterPlaying, scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
    Boolean success = False;
    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
        }

        env << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
            env << " (for up to " << scs.duration << " seconds)";
        }
        env << "...\n";

        success = True;

    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    UsageEnvironment& env = rtspClient->envir(); // alias

    env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
    ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
    StreamClientState& scs = rtspClient->scs; // alias

    scs.streamTimerTask = NULL;

    // Shut down the stream:
    shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    // First, check whether any subsessions have still to be closed:
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                Medium::close(subsession->sink);
                subsession->sink = NULL;

                if (subsession->rtcpInstance() != NULL) {
                    subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                }

                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
            // Don't bother handling the response to the "TEARDOWN".
            rtspClient->sendTeardownCommand(*scs.session, NULL);
        }
    }

    env << *rtspClient << "Closing the stream.\n";
    Medium::close(rtspClient);

}


// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
    return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
    : RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
}


// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
    : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment& env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
}


int ont_onvifdevice_stream_ctrl(void *ctx, int level)
{	
    /*change the source*/
    if (!ctx)
    {
        return -1;
    }
    BasicUsageEnvironment* env = (BasicUsageEnvironment*)ctx;
	ont_onvif_playctx *r = (ont_onvif_playctx*)env->livertmp;
    device_onvif_t *deviceptr = ont_getonvifdevice(r->channel);
    const char *c_url =ont_geturl_level(deviceptr, level, &r->meta);
    if (!c_url)
    {
        return -1;
    }
	string url = string(c_url);
	if (url.empty())
	{
		return -1;
	}

    if (r->rtsp_client){
        shutdownStream((RTSPClient*)r->rtsp_client, 1);
    }
	
	memset(&r->meta, 0x00, sizeof(r->meta));


    string urlPrefix;
    string playurl;
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = &env->taskScheduler();

    if (strlen(deviceptr->strUser) > 0)
    {
        urlPrefix = string(deviceptr->strUser);
    }
    if (strlen(deviceptr->strPass) > 0)
    {
        urlPrefix += ":" + string(deviceptr->strPass);
    }
    playurl = "rtsp://" + urlPrefix + "@" + url.substr(7);
    //env->rtmp = r;
    env->livertmp = r;
	
    void *rtspClient = openURL(*env, NULL, playurl.c_str()); //need free the rtspclient resource
    if (rtspClient){
        r->rtsp_client = rtspClient;
    }
    else{
        goto _end;
    }
    //openURL(*env, NULL, deviceptr->strPlayurl);
    r->eventLoopWatchVariable = 0;
    return 0;
_end:
    return -1;
}


void* ont_onvifdevice_live_stream_start(int channel, const char *push_url, const char*deviceid, int level)
{
    device_onvif_t *deviceptr = ont_getonvifdevice(channel);
    //use the default leve 3.
    string urlPrefix;
    string playurl;
	const char*c_url = NULL;
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    BasicUsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	ont_onvif_playctx *r = new ont_onvif_playctx;
	memset(r, 0x00, sizeof(ont_onvif_playctx));
	
	c_url = ont_geturl_level(deviceptr, level, &r->meta);

	if (c_url == NULL)
	{
		return NULL;
	}
	string url = string(c_url);

    r->tempBuf = NULL;
    r->tempBufSize = 0;
    r->rtsp_client = NULL;
    r->rtmp_client = NULL;
    r->channel = channel;
	r->startts = 0;
    r->rtmp_client = (RTMP*)rtmp_create_publishstream(push_url, 10, deviceid);
    do
    {
        if (!r->rtmp_client)
        {
            break;
        }
        if (strlen(deviceptr->strUser) > 0)
        {
            urlPrefix = string(deviceptr->strUser);
        }
        if (strlen(deviceptr->strPass) > 0)
        {
            urlPrefix += ":"+string(deviceptr->strPass);
        }
        playurl = "rtsp://" + urlPrefix + "@" + url.substr(7);
        //env->rtmp = r;
        env->livertmp = r;

        void *rtspClient = openURL(*env, NULL, playurl.c_str()); //need free the rtspclient resource
        if (rtspClient)
        {
            r->rtsp_client = rtspClient;
        }
        else
        {
            break;
        }
        
        //openURL(*env, NULL, deviceptr->strPlayurl);
        r->eventLoopWatchVariable = 0;
        return env;
    }while (0);
    
    if (r->rtsp_client){
        shutdownStream((RTSPClient*)r->rtsp_client, 1);
    }
    if (r->tempBuf){
        free(r->tempBuf);
    }
    if (r->rtmp_client){
        RTMP_Close((RTMP*)r->rtmp_client);
        RTMP_Free((RTMP*)r->rtmp_client);
    }
    delete r;
    delete env;
    delete scheduler;
    return NULL;
}


int ont_onvifdevice_live_stream_singlestep(void *ctx)
{
    BasicUsageEnvironment* env = (BasicUsageEnvironment*)ctx;
	ont_onvif_playctx *r = (ont_onvif_playctx*)env->livertmp;
	if (rtmp_check_rcv_handler(r->rtmp_client) < 0)
	{
		return -1;
	}
    env->taskScheduler().SingleStep();
	if (r->eventLoopWatchVariable != 0)
	{
		return -1;
	}
	return 0;
}


void ont_onvifdevice_live_stream_stop(void *ctx)
{
    BasicUsageEnvironment* env = (BasicUsageEnvironment*)ctx;
	ont_onvif_playctx *r = (ont_onvif_playctx*)env->livertmp;
    TaskScheduler *scheduler = &env->taskScheduler();

    if (r->rtsp_client){
        shutdownStream((RTSPClient*)r->rtsp_client, 1);
    }
    if (r->tempBuf){
        free(r->tempBuf);
    }
    if (r->rtmp_client){
        RTMP_Close((RTMP*)r->rtmp_client);
        RTMP_Free((RTMP*)r->rtmp_client);
    }
    delete r;
    delete scheduler;
    delete env;
}




extern "C" void* _test_live_stream_start(int channel, const char *push_url, const char*deviceid, int level)
{
	const char*c_url = NULL;
	// Begin by setting up our usage environment:
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	BasicUsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	env->livertmp = NULL;
	c_url = "rtsp://admin:test123456@192.168.217.133:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1 RTSP/1.0";
	void *rtspClient = openURL(*env, NULL, c_url); //need free the rtspclient resource
	while (1)
	{
		env->taskScheduler().SingleStep();
	}
}
	

