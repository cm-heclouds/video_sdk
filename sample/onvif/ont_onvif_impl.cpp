#include <stdlib.h>
#include <string>
#include "device_onvif.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "rtmp.h"
#include "rtmp_if.h"
#include "platform.h"
#include "ont_onvif_if.h"
#include "ont_packet_queue.h"

#include "ont_onvif_videoandiohandler.h"
using namespace std;



void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

void subsessionAfterPlaying(void* clientData);
void subsessionByeHandler(void* clientData);
void streamTimerHandler(void* clientData);

// The main streaming routine (for each "rtsp://" URL):
static void* openURL(ont_onvif_playctx *ctx, char const* progName, char const* rtspURL);

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

	void setplayctx(void *ctx) { playctx = ctx; }
	void *getplayctx() { return playctx; }
    void seterrno(int no) { errnum = no; }
    int  geterrno() { return errnum;}

protected:
    ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
        int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~ourRTSPClient();

public:
    int errnum;
	void *playctx;
    StreamClientState scs;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"


static void* openURL(ont_onvif_playctx *ctx,char const* progName, char const* rtspURL) {
    // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
    // to receive (even if more than stream uses the same "rtsp://" URL).
	UsageEnvironment &env = *(UsageEnvironment*)ctx->play_env;
	ourRTSPClient* rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
    if (rtspClient == NULL) {
		env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
        return NULL;
    }

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
    // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
	rtspClient->setplayctx(ctx);
	rtspClient->seterrno(0);
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
    ((ourRTSPClient*)rtspClient)->seterrno(-1);
    //shutdownStream(rtspClient);
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
        scs.subsession->sink = ONTVideoAudioSink::createNew(
            *scs.subsession,
			 (ont_onvif_playctx*)((ourRTSPClient*)rtspClient)->getplayctx(),
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
        ((ourRTSPClient*)rtspClient)->seterrno(-2);
        //shutdownStream(rtspClient);
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
    ((ourRTSPClient*)rtspClient)->seterrno(-3);
    //shutdownStream(rtspClient);
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
    ((ourRTSPClient*)rtspClient)->seterrno(-4);
    //shutdownStream(rtspClient);
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


int ont_onvifdevice_stream_ctrl(void *playctx, int level)
{	
    /*change the source*/
	if (!playctx)
    {
        return -1;
    }
	ont_onvif_playctx *r = (ont_onvif_playctx*)playctx;
    device_onvif_t *deviceptr = ont_getonvifdevice((device_onvif_cluster_t*)r->cluster,r->channel);

    if (r->rtsp_level == level) {
        r->sendmeta[0] = 0;
        r->sendmeta[1] = 0;
        return -2;
    }
	memset(&r->meta, 0x00, sizeof(r->meta));
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
        r->rtsp_client = NULL;
    }
	
    string urlPrefix;
    string playurl;

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

    strcpy(r->rtsp_playurl, playurl.c_str());
    void *rtspClient = openURL(r, NULL, playurl.c_str()); //need free the rtspclient resource
    if (rtspClient){
        r->rtsp_client = rtspClient;
        r->rtsp_level = level;
    }
    else{
        goto _end;
    }
	r->state = 0;
    r->push_model = 1;
    r->sendmeta[0] = 0;
    r->sendmeta[1] = 0;

    if (r->packDataList) {
        ont_list_clear(r->packDataList);
    } else {
        r->packDataList = ont_list_create();
    }
    return 0;
_end:
    return -1;
}


int ont_onvifdevice_live_stream_play(void *playctx, const char* push_url, const char* deviceid)
{
#if 0 /* unused variable */
    int level = 4;
#endif

    struct ont_timeval tv;
    ont_gettimeofday(&tv, NULL);
    unsigned long ts = (tv.tv_usec / 1000 + tv.tv_sec * 1000);

    /*change the source*/
	if (!playctx)
    {
        return -1;
    }
	ont_onvif_playctx *r = (ont_onvif_playctx*)playctx;

    if (r->last_rcvts > 0 && ts > r->last_rcvts && ts > r->last_rcvts+5000)
    {

        if (r->rtsp_client){
            shutdownStream((RTSPClient*)r->rtsp_client, 1);
            r->rtsp_client = NULL;
        }
        void *rtspClient = openURL(r, NULL, r->rtsp_playurl); //need free the rtspclient resource
        r->rtsp_client = rtspClient;
    }

    /*restart RTMP client*/
    if (r->rtmp_client) {
        rtmp_stop_publishstream(r->rtmp_client);      
        ont_disable_rtmp_packet_handle(r);
    }
    if (push_url) {
        r->rtmp_client = (RTMP*)rtmp_create_publishstream(push_url, 10, deviceid);
		r->key_send = 0;
    }
	r->state = 0;
    r->push_model = 1;
    r->sendmeta[0] = 0;
    r->sendmeta[1] = 0;
	r->last_sndts = ts;
    if (!r->packDataList) {
        r->packDataList = ont_list_create();
	}
	else
	{
		/*reset queue*/
		t_rtmp_mode_ctx mode_ctx;
		mode_ctx.type = LIVE_MODE;
		mode_ctx.onvif_ctx = r;
		RTMPPackClearqueue(&mode_ctx);
	}
    return 0;
}


void* ont_onvifdevice_live_stream_start( void        *penv,
                                         void        *cluster,
                                         int          channel,
                                         const  char *push_url,
                                         const char  *deviceid,
                                         int          level )
{
    device_onvif_t *deviceptr = ont_getonvifdevice((device_onvif_cluster_t*)cluster, channel);
    //use the default leve 3.
    string urlPrefix;
    string playurl;
	const char*c_url = NULL;
    // Begin by setting up our usage environment:

	ont_onvif_playctx *r = (ont_onvif_playctx *)ont_platform_malloc(sizeof(ont_onvif_playctx));
	memset(r, 0x00, sizeof(ont_onvif_playctx));

	c_url = ont_geturl_level(deviceptr, level, &r->meta);

	if (c_url == NULL)
	{
		return NULL;
	}
	string url = string(c_url);

    r->cluster = cluster;
    r->tempBuf = NULL;
    r->tempBufSize = 0;
    r->rtsp_client = NULL;
    r->rtmp_client = NULL;
    r->channel = channel;
	r->startts = r->last_sndts = r->last_rcvts = 0;
    r->play_env = penv;
    r->rtsp_level = level;
    r->key_send = 0;
    if (push_url) {
        r->rtmp_client = (RTMP*)rtmp_create_publishstream(push_url, 10, deviceid);
    }
    do
    {
        /*
        if (!r->rtmp_client)
        {
            break;
        }
        */
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

        strcpy(r->rtsp_playurl, playurl.c_str());
        void *rtspClient = openURL(r, NULL, playurl.c_str()); //need free the rtspclient resource
        if (rtspClient)
        {
            r->rtsp_client = rtspClient;
        }
        else
        {
            break;
        }
		r->state = 0;
        r->push_model = 0;
        r->sendmeta[0] = 0;
        r->sendmeta[1] = 0;
        r->packDataList = ont_list_create();
        
        return r;
    }while (0);

    if (r->rtsp_client){
        shutdownStream((RTSPClient*)r->rtsp_client, 1);
    }
    if (r->tempBuf){
        free(r->tempBuf);
    }


    if (r->rtmp_client){
		rtmp_stop_publishstream(r->rtmp_client);
        r->rtmp_client=NULL;
    }
    ont_platform_free(r);
    return NULL;
}


int ont_onvifdevice_live_stream_singlestep(void *penv, int maxdelay)
{
	BasicUsageEnvironment* env = (BasicUsageEnvironment*)penv;
    env->taskScheduler().SingleStep(maxdelay);
	return 0;
}

void ont_onvifdevice_rtmp_stop(void *ctx)
{
    ont_onvif_playctx* r = (ont_onvif_playctx*)ctx;
    
    r->push_model = 0;

    if (r->rtmp_client){
        ont_disable_rtmp_packet_handle(r);
		rtmp_stop_publishstream(r->rtmp_client);
        r->rtmp_client = NULL;
    }
    
	r->state= 0;
    r->sendmeta[0] = 0;
    r->sendmeta[1] = 0;
}


void  ont_onvifdevice_live_stream_stop(void *ctx)
{
	ont_onvif_playctx* r = (ont_onvif_playctx*)ctx;

	if (r->rtsp_client) {
		shutdownStream((RTSPClient*)r->rtsp_client, 1);
		r->rtsp_client = NULL;
	}

	void *rtspClient = openURL(r, NULL, r->rtsp_playurl); //need free the rtspclient resource
	if (rtspClient)
	{
		r->rtsp_client = rtspClient;
		r->last_sndts = 0;
	}

}




extern "C" void _test_live_stream_start(int channel, const char *push_url, const char*deviceid, int level)
{
	const char*c_url = NULL;
	// Begin by setting up our usage environment:
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	BasicUsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	c_url = "rtsp://admin:test123456@192.168.217.133:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1 RTSP/1.0";
	//void *rtspClient = openURL(*env, NULL, c_url); //need free the rtspclient resource
	(void)c_url;
	while (1)
	{
		env->taskScheduler().SingleStep();
	}
}
	

