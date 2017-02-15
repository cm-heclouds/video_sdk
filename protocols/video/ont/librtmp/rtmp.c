/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *      Copyright (C) 2008-2009 Andrej Stepanchuk
 *      Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */



#include <stdlib.h>
#include <string.h>

#include "rtmp_sys.h"
#include "log.h"
#include "ont/platform.h"
#include "ont/error.h"

#ifdef CRYPTO
#include <openssl/ssl.h>
#include <openssl/rc4.h>
TLS_CTX RTMP_TLS_ctx;
#endif

#define RTMP_SIG_SIZE 1536
#define RTMP_LARGE_HEADER_SIZE 12
#define HEX2BIN(a) (((a)&0x40)?((a)&0xf)+9:((a)&0xf))

static const int packetSize[] = { 12, 8, 4, 1 };

int RTMP_ctrlC;

const char RTMPProtocolStrings[][7] = {
    "RTMP",
    "RTMPT",
    "RTMPE",
    "RTMPTE",
    "RTMPS",
    "RTMPTS",
    "",
    "",
    "RTMFP"
};

const char RTMPProtocolStringsLower[][7] = {
    "rtmp",
    "rtmpt",
    "rtmpe",
    "rtmpte",
    "rtmps",
    "rtmpts",
    "",
    "",
    "rtmfp"
};


typedef enum {
    RTMPT_OPEN = 0, RTMPT_SEND, RTMPT_IDLE, RTMPT_CLOSE
} RTMPTCmd;

static int DumpMetaData(AMFObject *obj);
static int HandShake(RTMP *r, int FP9HandShake);

static int SendConnectPacket(RTMP *r, RTMPPacket *cp);
static int SendCheckBW(RTMP *r);
static int SendCheckBWResult(RTMP *r, double txn);
static int SendDeleteStream(RTMP *r, double dStreamId);
static int SendFCSubscribe(RTMP *r, AVal *subscribepath);
static int SendPlay(RTMP *r);
static int SendBytesReceived(RTMP *r);
static int SendUsherToken(RTMP *r, AVal *usherToken);
static int SendCustomCommand(RTMP *r, AVal *Command, int queue);
static int SendGetStreamLength(RTMP *r);
static int strsplit(char *src, int srclen, char delim, char ***params);

static int HandleInvoke(RTMP *r, const char *body, unsigned int nBodySize);
static int HandleMetadata(RTMP *r, char *body, unsigned int len);
static void HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet);
static void HandleAudio(RTMP *r, const RTMPPacket *packet);
static void HandleVideo(RTMP *r, const RTMPPacket *packet);
static void HandleCtrl(RTMP *r, const RTMPPacket *packet);
static void HandleServerBW(RTMP *r, const RTMPPacket *packet);
static void HandleClientBW(RTMP *r, const RTMPPacket *packet);

static int ReadN(RTMP *r, char *buffer, int n);
static int WriteN(RTMP *r, const char *buffer, int n, int drop);

static void DecodeTEA(AVal *key, AVal *text);


uint32_t
RTMP_GetTime()
{
    /* need ms*/
    return ont_platform_time()*1000;
}

void
RTMP_UserInterrupt()
{
    RTMP_ctrlC = TRUE;
}

void
RTMPPacket_Reset(RTMPPacket *p)
{
    p->m_headerType = 0;
    p->m_packetType = 0;
    p->m_nChannel = 0;
    p->m_nTimeStamp = 0;
    p->m_nInfoField2 = 0;
    p->m_hasAbsTimestamp = FALSE;
    p->m_nBodySize = 0;
    p->m_nBytesRead = 0;
}

int
RTMPPacket_Alloc(RTMPPacket *p, int nSize)
{
    char *ptr = ont_platform_malloc(nSize + RTMP_MAX_HEADER_SIZE);
    if (!ptr)
        return FALSE;
    p->m_body = ptr + RTMP_MAX_HEADER_SIZE;
    p->m_nBytesRead = 0;
    return TRUE;
}

void
RTMPPacket_Free(RTMPPacket *p)
{
    if (p->m_body)
    {
        ont_platform_free(p->m_body - RTMP_MAX_HEADER_SIZE);
        p->m_body = NULL;
    }
}

void
RTMPPacket_Dump(RTMPPacket *p)
{
    RTMP_Log(RTMP_LOGDEBUG,
        "RTMP PACKET: packet type: 0x%02x. channel: 0x%02x. info 1: %d info 2: %d. Body size: %u. body: 0x%02x",
        p->m_packetType, p->m_nChannel, p->m_nTimeStamp, p->m_nInfoField2,
        p->m_nBodySize, p->m_body ? (unsigned char)p->m_body[0] : 0);
}

int
RTMP_LibVersion()
{
    return RTMP_LIB_VERSION;
}

void
RTMP_TLS_Init()
{
#ifdef CRYPTO
#ifdef USE_POLARSSL
    /* Do this regardless of NO_SSL, we use havege for rtmpe too */
    RTMP_TLS_ctx = calloc(1,sizeof(struct tls_ctx));
    havege_init(&RTMP_TLS_ctx->hs);
#elif defined(USE_GNUTLS) && !defined(NO_SSL)
    /* Technically we need to initialize libgcrypt ourselves if
     * we're not going to call gnutls_global_init(). Ignoring this
     * for now.
     */
    gnutls_global_init();
    RTMP_TLS_ctx = ont_platform_malloc(sizeof(struct tls_ctx));
    gnutls_certificate_allocate_credentials(&RTMP_TLS_ctx->cred);
    gnutls_priority_init(&RTMP_TLS_ctx->prios, "NORMAL", NULL);
    gnutls_certificate_set_x509_trust_file(RTMP_TLS_ctx->cred,
        "ca.pem", GNUTLS_X509_FMT_PEM);
#elif !defined(NO_SSL) /* USE_OPENSSL */
    /* libcrypto doesn't need anything special */
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_digests();
    RTMP_TLS_ctx = SSL_CTX_new(SSLv23_method());
    SSL_CTX_set_options(RTMP_TLS_ctx, SSL_OP_ALL);
    SSL_CTX_set_default_verify_paths(RTMP_TLS_ctx);
#endif
#endif
}

RTMP *
RTMP_Alloc()
{
    int size = sizeof(RTMP);
    return ont_platform_malloc(size);
}

void
RTMP_Free(RTMP *r)
{
    ont_platform_free(r);
}

void
RTMP_Init(RTMP *r)
{
#ifdef CRYPTO
    if (!RTMP_TLS_ctx)
        RTMP_TLS_Init();
#endif
    int size = sizeof(RTMP);
    memset(r, 0, size);
    r->m_sb.ont_sock = NULL;
    r->m_inChunkSize = RTMP_DEFAULT_CHUNKSIZE;
    r->m_outChunkSize = RTMP_DEFAULT_OUT_CHUNKSIZE;
    r->m_nBufferMS = 30000;
    r->m_nClientBW = 2500000;
    r->m_nClientBW2 = 2;
    r->m_nServerBW = 1000000;
    r->m_fAudioCodecs = 3191.0;
    r->m_fVideoCodecs = 252.0;
    r->Link.timeout = 30;
    r->Link.swfAge = 30;
    r->Link.CombineConnectPacket = TRUE;
    r->Link.ConnectPacket = FALSE;
    r->pause_notify = NULL;
    r->seek_notify = NULL;
}

void
RTMP_EnableWrite(RTMP *r)
{
    r->Link.protocol |= RTMP_FEATURE_WRITE;
}

double
RTMP_GetDuration(RTMP *r)
{
    return r->m_fDuration;
}

int
RTMP_IsConnected(RTMP *r)
{
    return r->m_sb.ont_sock !=NULL;
}


int
RTMP_IsTimedout(RTMP *r)
{
    return r->m_sb.sb_timedout;
}

void
RTMP_SetBufferMS(RTMP *r, int size)
{
    r->m_nBufferMS = size;
}

void
RTMP_UpdateBufferMS(RTMP *r)
{
    RTMP_SendCtrl(r, 3, r->m_stream_id, r->m_nBufferMS);
}

#undef OSS
#ifdef _WIN32
#define OSS "WIN"
#elif defined(__sun__)
#define OSS "SOL"
#elif defined(__APPLE__)
#define OSS "MAC"
#elif defined(__linux__)
#define OSS "LNX"
#else
#define OSS "GNU"
#endif
#define DEF_VERSTR  OSS " 10,0,32,18"
static const char DEFAULT_FLASH_VER[] = DEF_VERSTR;
const AVal RTMP_DefaultFlashVer =
{ (char *)DEFAULT_FLASH_VER, sizeof(DEFAULT_FLASH_VER) - 1 };

void
RTMP_SetupStream(RTMP *r,
int protocol,
AVal *host,
unsigned int port,
AVal *sockshost,
AVal *playpath,
AVal *tcUrl,
AVal *swfUrl,
AVal *pageUrl,
AVal *app,
AVal *auth,
AVal *swfSHA256Hash,
uint32_t swfSize,
AVal *flashVer,
AVal *subscribepath,
AVal *usherToken,
AVal *WeebToken,
int dStart,
int dStop, int bLiveStream, long int timeout)
{
    RTMP_Log(RTMP_LOGDEBUG, "Protocol : %s", RTMPProtocolStrings[protocol & 7]);
    RTMP_Log(RTMP_LOGDEBUG, "Hostname : %.*s", host->av_len, host->av_val);
    RTMP_Log(RTMP_LOGDEBUG, "Port     : %d", port);
    RTMP_Log(RTMP_LOGDEBUG, "Playpath : %s", playpath->av_val);

    if (tcUrl && tcUrl->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "tcUrl    : %s", tcUrl->av_val);
    if (swfUrl && swfUrl->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "swfUrl   : %s", swfUrl->av_val);
    if (pageUrl && pageUrl->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "pageUrl  : %s", pageUrl->av_val);
    if (app && app->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "app      : %.*s", app->av_len, app->av_val);
    if (auth && auth->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "auth     : %s", auth->av_val);
    if (subscribepath && subscribepath->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "subscribepath : %s", subscribepath->av_val);
    if (usherToken && usherToken->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "NetStream.Authenticate.UsherToken : %s", usherToken->av_val);
    if (WeebToken && WeebToken->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "WeebToken: %s", WeebToken->av_val);
    if (flashVer && flashVer->av_val)
        RTMP_Log(RTMP_LOGDEBUG, "flashVer : %s", flashVer->av_val);
    if (dStart > 0)
        RTMP_Log(RTMP_LOGDEBUG, "StartTime     : %d msec", dStart);
    if (dStop > 0)
        RTMP_Log(RTMP_LOGDEBUG, "StopTime      : %d msec", dStop);

    RTMP_Log(RTMP_LOGDEBUG, "live     : %s", bLiveStream ? "yes" : "no");
    RTMP_Log(RTMP_LOGDEBUG, "timeout  : %ld sec", timeout);

#ifdef CRYPTO
    if (swfSHA256Hash != NULL && swfSize > 0)
    {
        memcpy(r->Link.SWFHash, swfSHA256Hash->av_val, sizeof(r->Link.SWFHash));
        r->Link.SWFSize = swfSize;
        RTMP_Log(RTMP_LOGDEBUG, "SWFSHA256:");
        RTMP_LogHex(RTMP_LOGDEBUG, r->Link.SWFHash, sizeof(r->Link.SWFHash));
        RTMP_Log(RTMP_LOGDEBUG, "SWFSize  : %u", r->Link.SWFSize);
    }
    else
    {
        r->Link.SWFSize = 0;
    }
#endif

    if (sockshost->av_len)
    {
        const char *socksport = strchr(sockshost->av_val, ':');
        
        char *hostname = ont_platform_malloc( strlen(sockshost->av_val)+1);
        ont_platform_snprintf(hostname, strlen(sockshost->av_val)+1, "%s", sockshost->av_val);

        if (socksport)
            hostname[socksport - sockshost->av_val] = '\0';
        r->Link.sockshost.av_val = hostname;
        r->Link.sockshost.av_len = strlen(hostname);

        r->Link.socksport = socksport ? atoi(socksport + 1) : 1935;
        RTMP_Log(RTMP_LOGDEBUG, "Connecting via SOCKS proxy: %s:%d", r->Link.sockshost.av_val,
            r->Link.socksport);
    }
    else
    {
        r->Link.sockshost.av_val = NULL;
        r->Link.sockshost.av_len = 0;
        r->Link.socksport = 0;
    }

    if (tcUrl && tcUrl->av_len)
        r->Link.tcUrl = *tcUrl;
    if (swfUrl && swfUrl->av_len)
        r->Link.swfUrl = *swfUrl;
    if (pageUrl && pageUrl->av_len)
        r->Link.pageUrl = *pageUrl;
    if (app && app->av_len)
        r->Link.app = *app;
    if (auth && auth->av_len)
    {
        r->Link.auth = *auth;
        r->Link.lFlags |= RTMP_LF_AUTH;
    }
    if (flashVer && flashVer->av_len)
        r->Link.flashVer = *flashVer;
    else
        r->Link.flashVer = RTMP_DefaultFlashVer;
    if (subscribepath && subscribepath->av_len)
        r->Link.subscribepath = *subscribepath;
    if (usherToken && usherToken->av_len)
        r->Link.usherToken = *usherToken;
    if (WeebToken && WeebToken->av_len)
        r->Link.WeebToken = *WeebToken;
    r->Link.seekTime = dStart;
    r->Link.stopTime = dStop;
    if (bLiveStream)
        r->Link.lFlags |= RTMP_LF_LIVE;
    r->Link.timeout = timeout;

    r->Link.protocol = protocol;
    r->Link.hostname = *host;
    r->Link.port = port;
    r->Link.playpath = *playpath;

    if (r->Link.port == 0)
    {
        if (protocol & RTMP_FEATURE_SSL)
            r->Link.port = 443;
        else if (protocol & RTMP_FEATURE_HTTP)
            r->Link.port = 80;
        else
            r->Link.port = 1935;
    }
}

enum { OPT_STR = 0, OPT_INT, OPT_BOOL, OPT_CONN };
static const char *optinfo[] = {
    "string", "integer", "boolean", "AMF" };

#define OFF(x)  offsetof(struct RTMP,x)

static struct urlopt {
    AVal name;
    size_t off;
    int otype;
    int omisc;
    char *use;
} options[] = {
    { AVC("socks"), OFF(Link.sockshost), OPT_STR, 0,
    "Use the specified SOCKS proxy" },
    { AVC("app"), OFF(Link.app), OPT_STR, 0,
    "Name of target app on server" },
    { AVC("tcUrl"), OFF(Link.tcUrl), OPT_STR, 0,
    "URL to played stream" },
    { AVC("pageUrl"), OFF(Link.pageUrl), OPT_STR, 0,
    "URL of played media's web page" },
    { AVC("swfUrl"), OFF(Link.swfUrl), OPT_STR, 0,
    "URL to player SWF file" },
    { AVC("flashver"), OFF(Link.flashVer), OPT_STR, 0,
    "Flash version string (default " DEF_VERSTR ")" },
    { AVC("conn"), OFF(Link.extras), OPT_CONN, 0,
    "Append arbitrary AMF data to Connect message" },
    { AVC("playpath"), OFF(Link.playpath), OPT_STR, 0,
    "Path to target media on server" },
    { AVC("playlist"), OFF(Link.lFlags), OPT_BOOL, RTMP_LF_PLST,
    "Set playlist before play command" },
    { AVC("live"), OFF(Link.lFlags), OPT_BOOL, RTMP_LF_LIVE,
    "Stream is live, no seeking possible" },
    { AVC("subscribe"), OFF(Link.subscribepath), OPT_STR, 0,
    "Stream to subscribe to" },
    { AVC("jtv"), OFF(Link.usherToken), OPT_STR, 0,
    "Justin.tv authentication token" },
    { AVC("weeb"), OFF(Link.WeebToken), OPT_STR, 0,
    "Weeb.tv authentication token" },
    { AVC("token"), OFF(Link.token), OPT_STR, 0,
    "Key for SecureToken response" },
    { AVC("swfVfy"), OFF(Link.lFlags), OPT_BOOL, RTMP_LF_SWFV,
    "Perform SWF Verification" },
    { AVC("swfAge"), OFF(Link.swfAge), OPT_INT, 0,
    "Number of days to use cached SWF hash" },
#ifdef CRYPTO
    { AVC("swfsize"),   OFF(Link.swfSize),       OPT_INT, 0,
    "Size of the decompressed SWF file"},
    { AVC("swfhash"),   OFF(Link.swfHash),       OPT_STR, 0,
    "SHA256 hash of the decompressed SWF file"},
#endif
    { AVC("start"), OFF(Link.seekTime), OPT_INT, 0,
    "Stream start position in milliseconds" },
    { AVC("stop"), OFF(Link.stopTime), OPT_INT, 0,
    "Stream stop position in milliseconds" },
    { AVC("buffer"), OFF(m_nBufferMS), OPT_INT, 0,
    "Buffer time in milliseconds" },
    { AVC("timeout"), OFF(Link.timeout), OPT_INT, 0,
    "Session timeout in seconds" },
    { { NULL, 0 }, 0, 0 }
};

static const AVal truth[] = {
    AVC("1"),
    AVC("on"),
    AVC("yes"),
    AVC("true"),
    { 0, 0 }
};

static void RTMP_OptUsage()
{
    int i;

    RTMP_Log(RTMP_LOGERROR, "Valid RTMP options are:\n");
    for (i = 0; options[i].name.av_len; i++) {
        RTMP_Log(RTMP_LOGERROR, "%10s %-7s  %s\n", options[i].name.av_val,
            optinfo[options[i].otype], options[i].use);
    }
}

static int
parseAMF(AMFObject *obj, AVal *av, int *depth)
{
    AMFObjectProperty prop = { { 0, 0 } };
    int i;
    char *p, *arg = av->av_val;

    if (arg[1] == ':')
    {
        p = (char *)arg + 2;
        switch (arg[0])
        {
        case 'B':
            prop.p_type = AMF_BOOLEAN;
            prop.p_vu.p_number = atoi(p);
            break;
        case 'S':
            prop.p_type = AMF_STRING;
            prop.p_vu.p_aval.av_val = p;
            prop.p_vu.p_aval.av_len = av->av_len - (p - arg);
            break;
        case 'N':
            prop.p_type = AMF_NUMBER;
            prop.p_vu.p_number = strtod(p, NULL);
            break;
        case 'Z':
            prop.p_type = AMF_NULL;
            break;
        case 'O':
            i = atoi(p);
            if (i)
            {
                prop.p_type = AMF_OBJECT;
            }
            else
            {
                (*depth)--;
                return 0;
            }
            break;
        default:
            return -1;
        }
    }
    else if (arg[2] == ':' && arg[0] == 'N')
    {
        p = strchr(arg + 3, ':');
        if (!p || !*depth)
            return -1;
        prop.p_name.av_val = (char *)arg + 3;
        prop.p_name.av_len = p - (arg + 3);

        p++;
        switch (arg[1])
        {
        case 'B':
            prop.p_type = AMF_BOOLEAN;
            prop.p_vu.p_number = atoi(p);
            break;
        case 'S':
            prop.p_type = AMF_STRING;
            prop.p_vu.p_aval.av_val = p;
            prop.p_vu.p_aval.av_len = av->av_len - (p - arg);
            break;
        case 'N':
            prop.p_type = AMF_NUMBER;
            prop.p_vu.p_number = strtod(p, NULL);
            break;
        case 'O':
            prop.p_type = AMF_OBJECT;
            break;
        default:
            return -1;
        }
    }
    else
        return -1;

    if (*depth)
    {
        AMFObject *o2;
        for (i = 0; i < *depth; i++)
        {
            o2 = &obj->o_props[obj->o_num - 1].p_vu.p_object;
            obj = o2;
        }
    }
    AMF_AddProp(obj, &prop);
    if (prop.p_type == AMF_OBJECT)
        (*depth)++;
    return 0;
}

int RTMP_SetOpt(RTMP *r, const AVal *opt, AVal *arg)
{
    int i;
    void *v;

    for (i = 0; options[i].name.av_len; i++) {
        if (opt->av_len != options[i].name.av_len) continue;
        if (strcasecmp(opt->av_val, options[i].name.av_val)) continue;
        v = (char *)r + options[i].off;
        switch (options[i].otype) {
        case OPT_STR: {
            AVal *aptr = v;
            *aptr = *arg; }
                      break;
        case OPT_INT: {
            long l = strtol(arg->av_val, NULL, 0);
            *(int *)v = l; }
                      break;
        case OPT_BOOL: {
            int j, fl;
            fl = *(int *)v;
            for (j = 0; truth[j].av_len; j++) {
                if (arg->av_len != truth[j].av_len) continue;
                if (strcasecmp(arg->av_val, truth[j].av_val)) continue;
                fl |= options[i].omisc; break;
            }
            *(int *)v = fl;
        }
                       break;
        case OPT_CONN:
            if (parseAMF(&r->Link.extras, arg, &r->Link.edepth))
                return FALSE;
            break;
        }
        break;
    }
    if (!options[i].name.av_len) {
        RTMP_Log(RTMP_LOGERROR, "Unknown option %s", opt->av_val);
        RTMP_OptUsage();
        return FALSE;
    }

    return TRUE;
}

int RTMP_SetupURL(RTMP *r, char *url)
{
    AVal opt, arg;
    char *p1, *p2, *ptr = strchr(url, ' ');
    int ret, len;
    unsigned int port = 0;

    if (ptr)
        *ptr = '\0';

    len = strlen(url);
    ret = RTMP_ParseURL(url, &r->Link.protocol, &r->Link.hostname,
        &port, &r->Link.playpath0, &r->Link.app);
    if (!ret)
        return ret;
    r->Link.port = port;
    r->Link.playpath = r->Link.playpath0;

    while (ptr) {
        *ptr++ = '\0';
        p1 = ptr;
        p2 = strchr(p1, '=');
        if (!p2)
            break;
        opt.av_val = p1;
        opt.av_len = p2 - p1;
        *p2++ = '\0';
        arg.av_val = p2;
        ptr = strchr(p2, ' ');
        if (ptr) {
            *ptr = '\0';
            arg.av_len = ptr - p2;
            /* skip repeated spaces */
            while (ptr[1] == ' ')
                *ptr++ = '\0';
        }
        else {
            arg.av_len = strlen(p2);
        }

        /* unescape */
        port = arg.av_len;
        for (p1 = p2; port > 0;) {
            if (*p1 == '\\') {
                unsigned int c;
                if (port < 3)
                    return FALSE;
                sscanf(p1 + 1, "%02x", &c);
                *p2++ = c;
                port -= 3;
                p1 += 3;
            }
            else {
                *p2++ = *p1++;
                port--;
            }
        }
        arg.av_len = p2 - arg.av_val;

        ret = RTMP_SetOpt(r, &opt, &arg);
        if (!ret)
            return ret;
    }

    if (!r->Link.tcUrl.av_len)
    {
        r->Link.tcUrl.av_val = url;
        if (r->Link.app.av_len)
        {
            if (r->Link.app.av_val < url + len)
            {
                /* if app is part of original url, just use it */
                r->Link.tcUrl.av_len = r->Link.app.av_len + (r->Link.app.av_val - url);
            }
            else
            {
                len = r->Link.hostname.av_len + r->Link.app.av_len +
                    sizeof("rtmpte://:65535/");
                r->Link.tcUrl.av_val = ont_platform_malloc(len);
                r->Link.tcUrl.av_len = ont_platform_snprintf(r->Link.tcUrl.av_val, len,
                    "%s://%.*s:%d/%.*s",
                    RTMPProtocolStringsLower[r->Link.protocol],
                    r->Link.hostname.av_len, r->Link.hostname.av_val,
                    r->Link.port,
                    r->Link.app.av_len, r->Link.app.av_val);
                r->Link.lFlags |= RTMP_LF_FTCU;
            }
        }
        else
        {
            r->Link.tcUrl.av_len = strlen(url);
        }
    }

#ifdef CRYPTO
    RTMP_Log(RTMP_LOGDEBUG, "Khalsa: %d %d %s\n", r->Link.swfSize, r->Link.swfHash.av_len, r->Link.swfHash.av_val);
    if (r->Link.swfSize && r->Link.swfHash.av_len)
    {
        int i, j = 0;
        for (i = 0; i < r->Link.swfHash.av_len; i += 2)
            r->Link.SWFHash[j++] = (HEX2BIN(r->Link.swfHash.av_val[i]) << 4) | HEX2BIN(r->Link.swfHash.av_val[i + 1]);
        r->Link.SWFSize = (uint32_t) r->Link.swfSize;
    }
    else if ((r->Link.lFlags & RTMP_LF_SWFV) && r->Link.swfUrl.av_len)
        RTMP_HashSWF(r->Link.swfUrl.av_val, &r->Link.SWFSize, (unsigned char *) r->Link.SWFHash, r->Link.swfAge);
#endif

    if (r->Link.port == 0)
    {
        if (r->Link.protocol & RTMP_FEATURE_SSL)
            r->Link.port = 443;
        else if (r->Link.protocol & RTMP_FEATURE_HTTP)
            r->Link.port = 80;
        else
            r->Link.port = 1935;
    }
    return TRUE;
}


int
RTMP_Connect1(RTMP *r, RTMPPacket *cp)
{
    if (r->Link.protocol & RTMP_FEATURE_SSL)
    {
#if defined(CRYPTO) && !defined(NO_SSL)
        TLS_client(RTMP_TLS_ctx, r->m_sb.sb_ssl);
        TLS_setfd(r->m_sb.sb_ssl, r->m_sb.sb_socket);
        if (TLS_connect(r->m_sb.sb_ssl) < 0)
        {
            RTMP_Log(RTMP_LOGERROR, "%s, TLS_Connect failed", __FUNCTION__);
            RTMP_Close(r);
            return FALSE;
        }
#else
        RTMP_Log(RTMP_LOGERROR, "%s, no SSL/TLS support", __FUNCTION__);
        RTMP_Close(r);
        return FALSE;

#endif
    }

    RTMP_Log(RTMP_LOGDEBUG, "%s, ... connected, handshaking", __FUNCTION__);
    if (!HandShake(r, TRUE))
    {
        RTMP_Log(RTMP_LOGERROR, "%s, handshake failed.", __FUNCTION__);
        RTMP_Close(r);
        return FALSE;
    }
    RTMP_Log(RTMP_LOGDEBUG, "%s, handshaked", __FUNCTION__);

    if (!SendConnectPacket(r, cp))
    {
        RTMP_Log(RTMP_LOGERROR, "%s, RTMP connect failed.", __FUNCTION__);
        RTMP_Close(r);
        return FALSE;
    }
    return TRUE;
}

int RTMP_Connect(RTMP *r, RTMPPacket *cp)
{
    char *hostname;
    int conret;
    int now, last;
    if (!r->Link.hostname.av_len)
        return FALSE;
    if (ont_platform_tcp_create(&r->m_sb.ont_sock) < 0)
    {
        return FALSE;
    }
    hostname = ont_platform_malloc(r->Link.hostname.av_len + 1);
    memcpy(hostname, r->Link.hostname.av_val, r->Link.hostname.av_len);
    hostname[r->Link.hostname.av_len] = '\0';


    /* check tcp connect  */
    last = ont_platform_time();
    do
    {
        now = ont_platform_time();
        conret = ont_platform_tcp_connect(r->m_sb.ont_sock, hostname, r->Link.port);
        if (conret == ONT_ERR_OK ||
            conret == ONT_ERR_SOCKET_ISCONN)
        {
            break;
        }
        if (conret == ONT_ERR_SOCKET_INPROGRESS && now - last < r->Link.timeout)
        {
            ont_platform_sleep(100);
            continue;
        }
        /*other situation, error*/
        ont_platform_tcp_close(r->m_sb.ont_sock);
        r->m_sb.ont_sock = NULL;
        ont_platform_free(hostname);
        return FALSE;
    } while (1);

    r->m_bSendCounter = TRUE;
    ont_platform_free(hostname);
    return RTMP_Connect1(r, cp);
}


int
RTMP_ConnectStream(RTMP *r, int seekTime)
{
    RTMPPacket packet = { 0 };

    /* seekTime was already set by SetupStream / SetupURL.
     * This is only needed by ReconnectStream.
     */
    if (seekTime > 0)
        r->Link.seekTime = seekTime;

    r->m_mediaChannel = 0;

    while (!r->m_bPlaying && RTMP_IsConnected(r) && RTMP_ReadPacket(r, &packet))
    {
        if (RTMPPacket_IsReady(&packet))
        {
            if (!packet.m_nBodySize)
                continue;
            if ((packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) ||
                (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) ||
                (packet.m_packetType == RTMP_PACKET_TYPE_INFO))
            {
                RTMP_Log(RTMP_LOGWARNING, "Received FLV packet before play()! Ignoring.");
                RTMPPacket_Free(&packet);
                continue;
            }

            RTMP_ClientPacket(r, &packet);
            RTMPPacket_Free(&packet);
        }
    }

    return r->m_bPlaying;
}

int
RTMP_ReconnectStream(RTMP *r, int seekTime)
{
    RTMP_DeleteStream(r);

    RTMP_SendCreateStream(r);

    return RTMP_ConnectStream(r, seekTime);
}

int
RTMP_ToggleStream(RTMP *r)
{
    int res;

    if (!r->m_pausing)
    {
        if (RTMP_IsTimedout(r) && r->m_read.status == RTMP_READ_EOF)
            r->m_read.status = 0;

        res = RTMP_SendPause(r, TRUE, r->m_pauseStamp);
        if (!res)
            return res;

        r->m_pausing = 1;
        ont_platform_sleep(1000);
    }
    res = RTMP_SendPause(r, FALSE, r->m_pauseStamp);
    r->m_pausing = 3;
    return res;
}

void
RTMP_DeleteStream(RTMP *r)
{
    if (r->m_stream_id < 0)
        return;

    r->m_bPlaying = FALSE;

    SendDeleteStream(r, r->m_stream_id);
    r->m_stream_id = -1;
}

int
RTMP_ClientPacket(RTMP *r, RTMPPacket *packet)
{
    int bHasMediaPacket = 0;
    switch (packet->m_packetType)
    {
    case RTMP_PACKET_TYPE_CHUNK_SIZE:
        /* chunk size */
        HandleChangeChunkSize(r, packet);
        break;

    case RTMP_PACKET_TYPE_BYTES_READ_REPORT:
        /* bytes read report */
        RTMP_Log(RTMP_LOGDEBUG, "%s, received: bytes read report", __FUNCTION__);
        break;

    case RTMP_PACKET_TYPE_CONTROL:
        /* ctrl */
        HandleCtrl(r, packet);
        break;

    case RTMP_PACKET_TYPE_SERVER_BW:
        /* server bw */
        HandleServerBW(r, packet);
        break;

    case RTMP_PACKET_TYPE_CLIENT_BW:
        /* client bw */
        HandleClientBW(r, packet);
        break;

    case RTMP_PACKET_TYPE_AUDIO:
        /* audio data */
        /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: audio %lu bytes", __FUNCTION__, packet.m_nBodySize); */
        HandleAudio(r, packet);
        bHasMediaPacket = 1;
        if (!r->m_mediaChannel)
            r->m_mediaChannel = packet->m_nChannel;
        if (!r->m_pausing)
            r->m_mediaStamp = packet->m_nTimeStamp;
        break;

    case RTMP_PACKET_TYPE_VIDEO:
        /* video data */
        /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet.m_nBodySize); */
        HandleVideo(r, packet);
        bHasMediaPacket = 1;
        if (!r->m_mediaChannel)
            r->m_mediaChannel = packet->m_nChannel;
        if (!r->m_pausing)
            r->m_mediaStamp = packet->m_nTimeStamp;
        break;

    case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
        /* flex stream send */
        RTMP_Log(RTMP_LOGDEBUG,
            "%s, flex stream send, size %u bytes, not supported, ignoring",
            __FUNCTION__, packet->m_nBodySize);
        break;

    case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
        /* flex shared object */
        RTMP_Log(RTMP_LOGDEBUG,
            "%s, flex shared object, size %u bytes, not supported, ignoring",
            __FUNCTION__, packet->m_nBodySize);
        break;

    case RTMP_PACKET_TYPE_FLEX_MESSAGE:
        /* flex message */
    {
        RTMP_Log(RTMP_LOGDEBUG,
            "%s, flex message, size %u bytes, not fully supported",
            __FUNCTION__, packet->m_nBodySize);
        /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

        /* some DEBUG code */
#if 0
        RTMP_LIB_AMFObject obj;
        int nRes = obj.Decode(packet.m_body+1, packet.m_nBodySize-1);
        if(nRes < 0) {
            RTMP_Log(RTMP_LOGERROR, "%s, error decoding AMF3 packet", __FUNCTION__);
            /*return; */
        }

        obj.Dump();
#endif

        if (HandleInvoke(r, packet->m_body + 1, packet->m_nBodySize - 1) == 1)
            bHasMediaPacket = 2;
        break;
    }
    case RTMP_PACKET_TYPE_INFO:
        /* metadata (notify) */
        RTMP_Log(RTMP_LOGDEBUG, "%s, received: notify %u bytes", __FUNCTION__,
            packet->m_nBodySize);
        if (HandleMetadata(r, packet->m_body, packet->m_nBodySize))
            bHasMediaPacket = 1;
        break;

    case RTMP_PACKET_TYPE_SHARED_OBJECT:
        RTMP_Log(RTMP_LOGDEBUG, "%s, shared object, not supported, ignoring",
            __FUNCTION__);
        break;

    case RTMP_PACKET_TYPE_INVOKE:
        /* invoke */
        RTMP_Log(RTMP_LOGDEBUG, "%s, received: invoke %u bytes", __FUNCTION__,
            packet->m_nBodySize);
        /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

        if (HandleInvoke(r, packet->m_body, packet->m_nBodySize) == 1)
            bHasMediaPacket = 2;
        break;

    case RTMP_PACKET_TYPE_FLASH_VIDEO:
    {
        /* go through FLV packets and handle metadata packets */
        unsigned int pos = 0;
        uint32_t nTimeStamp = packet->m_nTimeStamp;

        while (pos + 11 < packet->m_nBodySize)
        {
            uint32_t dataSize = AMF_DecodeInt24(packet->m_body + pos + 1);  /* size without header (11) and prevTagSize (4) */

            if (pos + 11 + dataSize + 4 > packet->m_nBodySize)
            {
                RTMP_Log(RTMP_LOGWARNING, "Stream corrupt?!");
                break;
            }
            if (packet->m_body[pos] == 0x12)
            {
                HandleMetadata(r, packet->m_body + pos + 11, dataSize);
            }
            else if (packet->m_body[pos] == 8 || packet->m_body[pos] == 9)
            {
                nTimeStamp = AMF_DecodeInt24(packet->m_body + pos + 4);
                nTimeStamp |= (packet->m_body[pos + 7] << 24);
            }
            pos += (11 + dataSize + 4);
        }
        if (!r->m_pausing)
            r->m_mediaStamp = nTimeStamp;

        /* FLV tag(s) */
        /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: FLV tag(s) %lu bytes", __FUNCTION__, packet.m_nBodySize); */
        bHasMediaPacket = 1;
        break;
    }
    default:
        RTMP_Log(RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
            packet->m_packetType);
#ifdef _DEBUG
        RTMP_LogHex(RTMP_LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
    }

    return bHasMediaPacket;
}

#ifdef RTMP_NETSTACKDUMP
FILE *netstackdump = NULL; 
#endif

static int
ReadN(RTMP *r, char *buffer, int n)
{
    int nOriginalSize = n;
    int avail;
    char *ptr;
    int last, now;

    r->m_sb.sb_timedout = FALSE;

#ifdef _DEBUG
    memset(buffer, 0, n);
#endif

    ptr = buffer;
    last = RTMP_GetTime();
    while (n > 0)
    {
        int nBytes = 0, nRead;
        {
            avail = r->m_sb.sb_size;
            if (avail == 0)
            {
                if (RTMPSockBuf_Fill(&r->m_sb) < 1)
                {
                    if (!r->m_sb.sb_timedout)
                    {
                        RTMP_Close(r);
                        return 0;
                    }
                }
                avail = r->m_sb.sb_size;
            }
        }
        nRead = ((n < avail) ? n : avail);
        if (nRead > 0)
        {
            memcpy(ptr, r->m_sb.sb_start, nRead);
            r->m_sb.sb_start += nRead;
            r->m_sb.sb_size -= nRead;
            nBytes = nRead;
            r->m_nBytesIn += nRead;
            if (r->m_bSendCounter && r->m_nBytesIn > (r->m_nBytesInSent + r->m_nClientBW / 10))
                if (!SendBytesReceived(r))
                    return FALSE;
        }
        /*RTMP_Log(RTMP_LOGDEBUG, "%s: %d bytes\n", __FUNCTION__, nBytes); */
#ifdef RTMP_NETSTACKDUMP
        /*fwrite(ptr, 1, nBytes, netstackdump_read);*/
#endif

#ifdef CRYPTO
        if (r->Link.rc4keyIn)
        {
            RC4_encrypt(r->Link.rc4keyIn, nBytes, ptr);
        }
#endif

        n -= nBytes;
        ptr += nBytes;

        now = RTMP_GetTime();
        if (n > 0 && now-last>r->Link.timeout*1000)
        {
            RTMP_Log(RTMP_LOGDEBUG, "%s, timeout ", __FUNCTION__);
            /*goto again; */
            RTMP_Close(r);
            break;
        }

    }

    return nOriginalSize - n;
}

static int
WriteN(RTMP *r, const char *buffer, int n, int drop)
{
    const char *ptr = buffer;
    char *ConnectPacket =NULL;
    int now, last;
    int needfree = 0;
#ifdef CRYPTO
    char *encrypted = 0;
    char buf[RTMP_BUFFER_CACHE_SIZE];

    if (r->Link.rc4keyOut)
    {
        if (n > sizeof(buf))
            encrypted = (char *)ont_platform_malloc(n);
        else
            encrypted = (char *)buf;
        ptr = encrypted;
        RC4_encrypt2(r->Link.rc4keyOut, n, buffer, ptr);
    }
#endif
    last = RTMP_GetTime();
    if (r->Link.ConnectPacket)
    {
        ConnectPacket = ont_platform_malloc(r->Link.HandshakeResponse.av_len + n);
        needfree = 1;
        memcpy(ConnectPacket, r->Link.HandshakeResponse.av_val, r->Link.HandshakeResponse.av_len);
        memcpy(ConnectPacket + r->Link.HandshakeResponse.av_len, ptr, n);
        ptr = ConnectPacket;
        n += r->Link.HandshakeResponse.av_len;
        r->Link.ConnectPacket = FALSE;
    }

    while (n > 0)
    {
        int nBytes;
        nBytes = RTMPSockBuf_Send(&r->m_sb, ptr, n);
        /*RTMP_Log(RTMP_LOGDEBUG, "%s: %d\n", __FUNCTION__, nBytes); */

        if (nBytes < 0)
        {
#ifndef ONT_PROTOCOL_VIDEO
            int sockerr = GetSockError();
            RTMP_Log(RTMP_LOGERROR, "%s, RTMP send error %d (%d bytes)", __FUNCTION__,
                sockerr, n);

            if (sockerr == EINTR && !RTMP_ctrlC)
                continue;
#endif
            RTMP_Close(r);
            n = 1;
            break;
        }
        n -= nBytes;
        ptr += nBytes;
        now = RTMP_GetTime();
        if (now-last>r->Link.timeout*1000)
        {
            n=1; /*time out*/
            break; 
        }
        else if (drop)
        {
            /*drop the packet*/
            n = 0;
            break;
        }
    }

    if (needfree)
    {
        ont_platform_free(ConnectPacket);
    }
#ifdef CRYPTO
    if (encrypted && encrypted != buf)
        ont_platform_free(encrypted);
#endif

    return n == 0;
}

#define SAVC(x) static const AVal av_##x = AVC(#x)

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(secureToken);
SAVC(secureTokenResponse);
SAVC(type);
SAVC(nonprivate);

static int
SendConnectPacket(RTMP *r, RTMPPacket *cp)
{
    RTMPPacket packet;
    char pbuf[4096], *pend = pbuf + sizeof(pbuf);
    char *enc;

    if (r->Link.CombineConnectPacket)
        r->Link.ConnectPacket = TRUE;

    if (cp)
        return RTMP_SendPacket(r, cp, TRUE, 0);

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_connect);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_OBJECT;

    enc = AMF_EncodeNamedString(enc, pend, &av_app, &r->Link.app);
    if (!enc)
        return FALSE;
    if (r->Link.protocol & RTMP_FEATURE_WRITE)
    {
        enc = AMF_EncodeNamedString(enc, pend, &av_type, &av_nonprivate);
        if (!enc)
            return FALSE;
    }
    if (r->Link.flashVer.av_len)
    {
        enc = AMF_EncodeNamedString(enc, pend, &av_flashVer, &r->Link.flashVer);
        if (!enc)
            return FALSE;
    }
    if (r->Link.swfUrl.av_len)
    {
        enc = AMF_EncodeNamedString(enc, pend, &av_swfUrl, &r->Link.swfUrl);
        if (!enc)
            return FALSE;
    }
    if (r->Link.tcUrl.av_len)
    {
        enc = AMF_EncodeNamedString(enc, pend, &av_tcUrl, &r->Link.tcUrl);
        if (!enc)
            return FALSE;
    }
    if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
    {
        enc = AMF_EncodeNamedBoolean(enc, pend, &av_fpad, FALSE);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 15.0);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_audioCodecs, r->m_fAudioCodecs);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videoCodecs, r->m_fVideoCodecs);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videoFunction, 1.0);
        if (!enc)
            return FALSE;
        if (r->Link.pageUrl.av_len)
        {
            enc = AMF_EncodeNamedString(enc, pend, &av_pageUrl, &r->Link.pageUrl);
            if (!enc)
                return FALSE;
        }
    }
    if (r->m_fEncoding != 0.0 || r->m_bSendEncoding)
    {   /* AMF0, AMF3 not fully supported yet */
        enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
        if (!enc)
            return FALSE;
    }
    if (enc + 3 >= pend)
        return FALSE;
    *enc++ = 0;
    *enc++ = 0;         /* end of object - 0x00 0x00 0x09 */
    *enc++ = AMF_OBJECT_END;

    /* add auth string */
    if (r->Link.auth.av_len)
    {
        enc = AMF_EncodeBoolean(enc, pend, r->Link.lFlags & RTMP_LF_AUTH);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeString(enc, pend, &r->Link.auth);
        if (!enc)
            return FALSE;
    }
    if (r->Link.extras.o_num)
    {
        int i;
        for (i = 0; i < r->Link.extras.o_num; i++)
        {
            enc = AMFProp_Encode(&r->Link.extras.o_props[i], enc, pend);
            if (!enc)
                return FALSE;
        }
    }
    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}


SAVC(createStream);

int
RTMP_SendCreateStream(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_createStream);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;      /* NULL */

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

SAVC(FCSubscribe);

static int
SendFCSubscribe(RTMP *r, AVal *subscribepath)
{
    RTMPPacket packet;
    char pbuf[512], *pend = pbuf + sizeof(pbuf);
    char *enc;
    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    RTMP_Log(RTMP_LOGDEBUG, "FCSubscribe: %s", subscribepath->av_val);
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_FCSubscribe);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, subscribepath);

    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

/* Justin.tv specific authentication */
static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");

static int
SendUsherToken(RTMP *r, AVal *usherToken)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;
    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    RTMP_Log(RTMP_LOGDEBUG, "UsherToken: %s", usherToken->av_val);
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_NetStream_Authenticate_UsherToken);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, usherToken);

    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}
/******************************************/

SAVC(releaseStream);

static int
SendReleaseStream(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_releaseStream);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(FCPublish);

static int
SendFCPublish(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_FCPublish);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(FCUnpublish);

static int
SendFCUnpublish(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_FCUnpublish);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(publish);
SAVC(live);
SAVC(record);

static int
SendPublish(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x04;   /* source channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = r->m_stream_id;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_publish);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    if (!enc)
        return FALSE;

    /* FIXME: should we choose live based on Link.lFlags & RTMP_LF_LIVE? */
    enc = AMF_EncodeString(enc, pend, &av_live);
    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

SAVC(deleteStream);

static int
SendDeleteStream(RTMP *r, double dStreamId)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_deleteStream);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, dStreamId);

    packet.m_nBodySize = enc - packet.m_body;

    /* no response expected */
    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(pause);

int
RTMP_SendPause(RTMP *r, int DoPause, int iTime)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x08;   /* video channel */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_pause);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeBoolean(enc, pend, DoPause);
    enc = AMF_EncodeNumber(enc, pend, (double)iTime);

    packet.m_nBodySize = enc - packet.m_body;

    RTMP_Log(RTMP_LOGDEBUG, "%s, %d, pauseTime=%d", __FUNCTION__, DoPause, iTime);
    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

int RTMP_Pause(RTMP *r, int DoPause)
{
    if (DoPause)
        r->m_pauseStamp = r->m_channelTimestamp[r->m_mediaChannel];
    return RTMP_SendPause(r, DoPause, r->m_pauseStamp);
}

SAVC(seek);

int
RTMP_SendSeek(RTMP *r, int iTime)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x08;   /* video channel */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_seek);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, (double)iTime);

    packet.m_nBodySize = enc - packet.m_body;

    r->m_read.flags |= RTMP_READ_SEEKING;
    r->m_read.nResumeTS = 0;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

int
RTMP_SendServerBW(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);

    packet.m_nChannel = 0x02;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_SERVER_BW;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    packet.m_nBodySize = 4;

    AMF_EncodeInt32(packet.m_body, pend, r->m_nServerBW);
    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

int
RTMP_SendClientBW(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);

    packet.m_nChannel = 0x02;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_CLIENT_BW;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    packet.m_nBodySize = 5;

    AMF_EncodeInt32(packet.m_body, pend, r->m_nClientBW);
    packet.m_body[4] = r->m_nClientBW2;
    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

static int
SendBytesReceived(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);

    packet.m_nChannel = 0x02;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_BYTES_READ_REPORT;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    packet.m_nBodySize = 4;

    AMF_EncodeInt32(packet.m_body, pend, r->m_nBytesIn);    /* hard coded for now */
    r->m_nBytesInSent = r->m_nBytesIn;

    /*RTMP_Log(RTMP_LOGDEBUG, "Send bytes report. 0x%x (%d bytes)", (unsigned int)m_nBytesIn, m_nBytesIn); */
    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(_checkbw);

static int
SendCheckBW(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;    /* RTMP_GetTime(); */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__checkbw);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;

    packet.m_nBodySize = enc - packet.m_body;

    /* triggers _onbwcheck and eventually results in _onbwdone */
    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(_result);

static int
SendCheckBWResult(RTMP *r, double txn)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0x16 * r->m_nBWCheckCounter;  /* temp inc value. till we figure it out. */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__result);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, (double)r->m_nBWCheckCounter++);

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(ping);
SAVC(pong);

static int
SendPong(RTMP *r, double txn)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0x16 * r->m_nBWCheckCounter;  /* temp inc value. till we figure it out. */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_pong);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_NULL;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

SAVC(play);

static int
SendPlay(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x08;   /* we make 8 our stream channel */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = r->m_stream_id;  /*0x01000000; */
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_play);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;

    RTMP_Log(RTMP_LOGDEBUG, "%s, seekTime=%d, stopTime=%d, sending play: %s",
        __FUNCTION__, r->Link.seekTime, r->Link.stopTime,
        r->Link.playpath.av_val);
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    if (!enc)
        return FALSE;

    /* Optional parameters start and len.
     *
     * start: -2, -1, 0, positive number
     *  -2: looks for a live stream, then a recorded stream,
     *      if not found any open a live stream
     *  -1: plays a live stream
     * >=0: plays a recorded streams from 'start' milliseconds
     */
    if (r->Link.lFlags & RTMP_LF_LIVE)
        enc = AMF_EncodeNumber(enc, pend, -1000.0);
    else
    {
        if (r->Link.seekTime > 0.0 || r->Link.stopTime)
            enc = AMF_EncodeNumber(enc, pend, r->Link.seekTime); /* resume from here */
    }
    if (!enc)
        return FALSE;

    /* len: -1, 0, positive number
     *  -1: plays live or recorded stream to the end (default)
     *   0: plays a frame 'start' ms away from the beginning
     *  >0: plays a live or recoded stream for 'len' milliseconds
     */
    /*enc += EncodeNumber(enc, -1.0); */ /* len */
    if (r->Link.stopTime)
    {
        enc = AMF_EncodeNumber(enc, pend, r->Link.stopTime - r->Link.seekTime);
        if (!enc)
            return FALSE;
    }

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

SAVC(set_playlist);
SAVC(0);

static int
SendPlaylist(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x08;   /* we make 8 our stream channel */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = r->m_stream_id;  /*0x01000000; */
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_set_playlist);
    enc = AMF_EncodeNumber(enc, pend, 0);
    *enc++ = AMF_NULL;
    *enc++ = AMF_ECMA_ARRAY;
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT;
    enc = AMF_EncodeNamedString(enc, pend, &av_0, &r->Link.playpath);
    if (!enc)
        return FALSE;
    if (enc + 3 >= pend)
        return FALSE;
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, TRUE, 0);
}

static int
SendSecureTokenResponse(RTMP *r, AVal *resp)
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x03;   /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_secureTokenResponse);
    enc = AMF_EncodeNumber(enc, pend, 0.0);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, resp);
    if (!enc)
        return FALSE;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet. In summary, Ping message is used as a special command that are exchanged between client and server. This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters. The first parameter is the type of Ping and in short integer. The second parameter is the target of the ping. As Ping is always sent in Channel 2 (control channel) and the target object in RTMP header is always 0 which means the Connection object, it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to. The second parameter takes this responsibility. The value has the same meaning as the target object field in RTMP header. (The second value could also be used as other purposes, like RTT Ping/Pong. It is used as the timestamp.) The third and fourth parameters are optional and could be looked upon as the parameter of the Ping packet. Below is an unexhausted list of Ping messages.

* type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0. After the connection is established, a Ping 0,0 will be sent from server to client. The message will also be sent to client on the start of Play and in response of a Seek or Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp of the next packet server sends.
* type 1: Tell the stream to clear the playing buffer.
* type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
* type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
* type 6: Ping the client from server. The second parameter is the current time.
* type 7: Pong reply from client. The second parameter is the time the server sent with his ping request.
* type 26: SWFVerification request
* type 27: SWFVerification response
*/
int
RTMP_SendCtrl(RTMP *r, short nType, unsigned int nObject, unsigned int nTime)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    int nSize;
    char *buf;

    RTMP_Log(RTMP_LOGDEBUG, "sending ctrl, type: 0x%04x", (unsigned short)nType);

    packet.m_nChannel = 0x02;   /* control channel (ping) */
    if (nType == 0 || nType == 1) /*for stream being and stream EOF, use format 0 and 1*/
    {
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    }
    else
    {
        packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    }
    packet.m_packetType = RTMP_PACKET_TYPE_CONTROL;
    packet.m_nTimeStamp = 0;    /* RTMP_GetTime(); */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    switch (nType) {
    case 0x03: nSize = 10; break;   /* buffer time */
    case 0x1A: nSize = 3; break;    /* SWF verify request */
    case 0x1B: nSize = 44; break;   /* SWF verify response */
    default: nSize = 6; break;
    }

    packet.m_nBodySize = nSize;

    buf = packet.m_body;
    buf = AMF_EncodeInt16(buf, pend, nType);

    if (nType == 0x1B)
    {
#ifdef CRYPTO
        memcpy(buf, r->Link.SWFVerificationResponse, 42);
        RTMP_Log(RTMP_LOGDEBUG, "Sending SWFVerification response: ");
        RTMP_LogHex(RTMP_LOGDEBUG, (uint8_t *)packet.m_body, packet.m_nBodySize);
#endif
    }
    else if (nType == 0x1A)
    {
        *buf = nObject & 0xff;
    }
    else
    {
        if (nSize > 2)
            buf = AMF_EncodeInt32(buf, pend, nObject);

        if (nSize > 6)
            buf = AMF_EncodeInt32(buf, pend, nTime);
    }

    return RTMP_SendPacket(r, &packet, FALSE, 0);
}

static void
AV_erase(RTMP_METHOD *vals, int *num, int i, int freeit)
{
    if (freeit)
        ont_platform_free(vals[i].name.av_val);
    (*num)--;
    for (; i < *num; i++)
    {
        vals[i] = vals[i + 1];
    }
    vals[i].name.av_val = NULL;
    vals[i].name.av_len = 0;
    vals[i].num = 0;
}

void
RTMP_DropRequest(RTMP *r, int i, int freeit)
{
    AV_erase(r->m_methodCalls, &r->m_numCalls, i, freeit);
}

static void
AV_queue(RTMP_METHOD **vals, int *num, AVal *av, int txn)
{
    char *tmp;
    if (!(*num & 0x0f))
        *vals = realloc(*vals, (*num + 16) * sizeof(RTMP_METHOD));
    tmp = ont_platform_malloc(av->av_len + 1);
    memcpy(tmp, av->av_val, av->av_len);
    tmp[av->av_len] = '\0';
    (*vals)[*num].num = txn;
    (*vals)[*num].name.av_len = av->av_len;
    (*vals)[(*num)++].name.av_val = tmp;
}

static void
AV_clear(RTMP_METHOD *vals, int num)
{
    int i;
    for (i = 0; i < num; i++)
        ont_platform_free(vals[i].name.av_val);
    ont_platform_free(vals);
}

SAVC(onBWCheck);
SAVC(onBWDone);
SAVC(onFCSubscribe);
SAVC(onFCUnsubscribe);
SAVC(_onbwcheck);
SAVC(_onbwdone);
SAVC(_error);
SAVC(close);
SAVC(code);
SAVC(level);
SAVC(onStatus);
SAVC(playlist_ready);
static const AVal av_NetStream_Failed = AVC("NetStream.Failed");
static const AVal av_NetStream_Play_Failed = AVC("NetStream.Play.Failed");
static const AVal av_NetStream_Play_StreamNotFound = AVC("NetStream.Play.StreamNotFound");
static const AVal av_NetConnection_Connect_InvalidApp = AVC("NetConnection.Connect.InvalidApp");
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_NetStream_Play_Complete = AVC("NetStream.Play.Complete");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_NetStream_Seek_Notify = AVC("NetStream.Seek.Notify");
static const AVal av_NetStream_Pause_Notify = AVC("NetStream.Pause.Notify");
static const AVal av_NetStream_UnPause_Notify = AVC("NetStream.Unpause.Notify");
static const AVal av_NetStream_Play_PublishNotify = AVC("NetStream.Play.PublishNotify");
static const AVal av_NetStream_Play_UnpublishNotify = AVC("NetStream.Play.UnpublishNotify");
static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_verifyClient = AVC("verifyClient");
static const AVal av_sendStatus = AVC("sendStatus");
static const AVal av_getStreamLength = AVC("getStreamLength");
static const AVal av_ReceiveCheckPublicStatus = AVC("ReceiveCheckPublicStatus");


static const AVal av_NETStream_PauseRaw = AVC("pauseraw");
static const AVal av_NETStream_Pause = AVC("pause");
static const AVal av_NETStream_Seek = AVC("seek");




/* Returns 0 for OK/Failed/error, 1 for 'Stop or Complete' */
static int
HandleInvoke(RTMP *r, const char *body, unsigned int nBodySize)
{
    AMFObject obj;
    AVal method;
    double txn;
    int ret = 0, nRes;
    char pbuf[256], *pend = pbuf + sizeof(pbuf), *enc, **params = NULL;
    char *host = r->Link.hostname.av_len ? r->Link.hostname.av_val : "";
    char *pageUrl = r->Link.pageUrl.av_len ? r->Link.pageUrl.av_val : "";
    int param_count;
    AVal av_Command;

    if (body[0] != 0x02)        /* make sure it is a string method name we start with */
    {
        RTMP_Log(RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
            __FUNCTION__);
        return 0;
    }



    nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
    if (nRes < 0)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
        return 0;
    }

    AMF_Dump(&obj);
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
    txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
    RTMP_Log(RTMP_LOGDEBUG, "%s, server invoking <%s>", __FUNCTION__, method.av_val);

    if (AVMATCH(&method, &av__result))
    {
        AVal methodInvoked = { 0 };
        int i;

        for (i = 0; i < r->m_numCalls; i++) {
            if (r->m_methodCalls[i].num == (int)txn) {
                methodInvoked = r->m_methodCalls[i].name;
                AV_erase(r->m_methodCalls, &r->m_numCalls, i, FALSE);
                break;
            }
        }
        if (!methodInvoked.av_val) {
            RTMP_Log(RTMP_LOGDEBUG, "%s, received result id %f without matching request",
                __FUNCTION__, txn);
            goto leave;
        }

        RTMP_Log(RTMP_LOGDEBUG, "%s, received result for method call <%s>", __FUNCTION__,
            methodInvoked.av_val);

        if (AVMATCH(&methodInvoked, &av_connect))
        {
            if (r->Link.token.av_len)
            {
                AMFObjectProperty p;
                if (RTMP_FindFirstMatchingProperty(&obj, &av_secureToken, &p))
                {
                    DecodeTEA(&r->Link.token, &p.p_vu.p_aval);
                    SendSecureTokenResponse(r, &p.p_vu.p_aval);
                }
            }
            if (r->Link.protocol & RTMP_FEATURE_WRITE)
            {
                SendReleaseStream(r);
                SendFCPublish(r);
            }
            else
            {
                RTMP_SendServerBW(r);
                RTMP_SendCtrl(r, 3, 0, 300);
            }
            if (strstr(host, "tv-stream.to") || strstr(pageUrl, "tv-stream.to"))
            {
                AVal av_requestAccess = AVC("requestAccess");
                AVal av_auth = AVC("0000000000");
                enc = pbuf;
                enc = AMF_EncodeString(enc, pend, &av_requestAccess);
                enc = AMF_EncodeNumber(enc, pend, 0);
                *enc++ = AMF_NULL;
                enc = AMF_EncodeString(enc, pend, &av_auth);
                av_Command.av_val = pbuf;
                av_Command.av_len = enc - pbuf;
                SendCustomCommand(r, &av_Command, FALSE);

                AVal av_getConnectionCount = AVC("getConnectionCount");
                enc = pbuf;
                enc = AMF_EncodeString(enc, pend, &av_getConnectionCount);
                enc = AMF_EncodeNumber(enc, pend, 0);
                *enc++ = AMF_NULL;
                av_Command.av_val = pbuf;
                av_Command.av_len = enc - pbuf;
                SendCustomCommand(r, &av_Command, FALSE);

                SendGetStreamLength(r);
            }
            else if (strstr(host, "jampo.com.ua") || strstr(pageUrl, "jampo.com.ua"))
            {
                SendGetStreamLength(r);
            }
            else if (strstr(host, "streamscene.cc") || strstr(pageUrl, "streamscene.cc")
                || strstr(host, "tsboard.tv") || strstr(pageUrl, "teamstream.in"))
            {
                AVal av_r = AVC("r");
                enc = pbuf;
                enc = AMF_EncodeString(enc, pend, &av_r);
                enc = AMF_EncodeNumber(enc, pend, 0);
                *enc++ = AMF_NULL;
                av_Command.av_val = pbuf;
                av_Command.av_len = enc - pbuf;
                SendCustomCommand(r, &av_Command, FALSE);

                SendGetStreamLength(r);
            }
            else if (strstr(host, "chaturbate.com") || strstr(pageUrl, "chaturbate.com"))
            {
                AVal av_ModelName;
                AVal av_CheckPublicStatus = AVC("CheckPublicStatus");

                if (strlen(pageUrl) > 7)
                {
                    strsplit(pageUrl + 7, FALSE, '/', &params);
                    av_ModelName.av_val = params[1];
                    av_ModelName.av_len = strlen(params[1]);

                    enc = pbuf;
                    enc = AMF_EncodeString(enc, pend, &av_CheckPublicStatus);
                    enc = AMF_EncodeNumber(enc, pend, 0);
                    *enc++ = AMF_NULL;
                    enc = AMF_EncodeString(enc, pend, &av_ModelName);
                    av_Command.av_val = pbuf;
                    av_Command.av_len = enc - pbuf;

                    SendCustomCommand(r, &av_Command, FALSE);
                }
            }
            /* Weeb.tv specific authentication */
            else if (r->Link.WeebToken.av_len)
            {
                AVal av_Token, av_Username, av_Password;
                AVal av_determineAccess = AVC("determineAccess");

                param_count = strsplit(r->Link.WeebToken.av_val, FALSE, ';', &params);
                if (param_count >= 1)
                {
                    av_Token.av_val = params[0];
                    av_Token.av_len = strlen(params[0]);
                }
                if (param_count >= 2)
                {
                    av_Username.av_val = params[1];
                    av_Username.av_len = strlen(params[1]);
                }
                if (param_count >= 3)
                {
                    av_Password.av_val = params[2];
                    av_Password.av_len = strlen(params[2]);
                }

                enc = pbuf;
                enc = AMF_EncodeString(enc, pend, &av_determineAccess);
                enc = AMF_EncodeNumber(enc, pend, 0);
                *enc++ = AMF_NULL;
                enc = AMF_EncodeString(enc, pend, &av_Token);
                enc = AMF_EncodeString(enc, pend, &av_Username);
                enc = AMF_EncodeString(enc, pend, &av_Password);
                av_Command.av_val = pbuf;
                av_Command.av_len = enc - pbuf;

                RTMP_Log(RTMP_LOGDEBUG, "WeebToken: %s", r->Link.WeebToken.av_val);
                SendCustomCommand(r, &av_Command, FALSE);
            }
            else
                RTMP_SendCreateStream(r);
        }
        else if (AVMATCH(&methodInvoked, &av_getStreamLength))
        {
            RTMP_SendCreateStream(r);
        }
        else if (AVMATCH(&methodInvoked, &av_createStream))
        {
            r->m_stream_id = (int)AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));

            if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
            {
                /* Authenticate on Justin.tv legacy servers before sending FCSubscribe */
                if (r->Link.usherToken.av_len)
                    SendUsherToken(r, &r->Link.usherToken);
                /* Send the FCSubscribe if live stream or if subscribepath is set */
                if (r->Link.subscribepath.av_len)
                    SendFCSubscribe(r, &r->Link.subscribepath);
                else if ((r->Link.lFlags & RTMP_LF_LIVE) && (!r->Link.WeebToken.av_len))
                    SendFCSubscribe(r, &r->Link.playpath);
            }

            if (r->Link.protocol & RTMP_FEATURE_WRITE)
            {
                SendPublish(r);
            }
            else
            {
                if (r->Link.lFlags & RTMP_LF_PLST)
                    SendPlaylist(r);
                SendPlay(r);
                RTMP_SendCtrl(r, 3, r->m_stream_id, r->m_nBufferMS);
            }
        }
        else if (AVMATCH(&methodInvoked, &av_play) ||
            AVMATCH(&methodInvoked, &av_publish))
        {
            r->m_bPlaying = TRUE;
        }
        ont_platform_free(methodInvoked.av_val);
    }
    else if (AVMATCH(&method, &av_onBWDone))
    {
        if (!r->m_nBWCheckCounter)
            SendCheckBW(r);
    }
    else if (AVMATCH(&method, &av_onFCSubscribe))
    {
        /* SendOnFCSubscribe(); */
    }
    else if (AVMATCH(&method, &av_onFCUnsubscribe))
    {
        RTMP_Close(r);
        ret = 1;
    }
    else if (AVMATCH(&method, &av_ping))
    {
        SendPong(r, txn);
    }
    else if (AVMATCH(&method, &av__onbwcheck) || AVMATCH(&method, &av_onBWCheck))
    {
        SendCheckBWResult(r, txn);
    }
    else if (AVMATCH(&method, &av__onbwdone))
    {
        int i;
        for (i = 0; i < r->m_numCalls; i++)
            if (AVMATCH(&r->m_methodCalls[i].name, &av__checkbw))
            {
                AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
                break;
            }
    }
    else if (AVMATCH(&method, &av__error))
    {
        RTMP_Log(RTMP_LOGERROR, "rtmp server sent error");
    }
    else if (AVMATCH(&method, &av_close))
    {
        RTMP_Log(RTMP_LOGERROR, "rtmp server requested close");
        RTMP_Close(r);
    }
    else if (AVMATCH(&method, &av_onStatus))
    {
        AMFObject obj2;
        AVal code, level;
        AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &obj2);
        AMFProp_GetString(AMF_GetProp(&obj2, &av_code, -1), &code);
        AMFProp_GetString(AMF_GetProp(&obj2, &av_level, -1), &level);

        RTMP_Log(RTMP_LOGDEBUG, "%s, onStatus: %s", __FUNCTION__, code.av_val);
        if (AVMATCH(&code, &av_NetStream_Failed)
            || AVMATCH(&code, &av_NetStream_Play_Failed)
            || AVMATCH(&code, &av_NetStream_Play_StreamNotFound)
            || AVMATCH(&code, &av_NetConnection_Connect_InvalidApp))
        {
            r->m_stream_id = -1;
            RTMP_Close(r);
            RTMP_Log(RTMP_LOGERROR, "Closing connection: %s", code.av_val);
        }

        else if (AVMATCH(&code, &av_NetStream_Play_Start)
            || AVMATCH(&code, &av_NetStream_Play_PublishNotify))
        {
            int i;
            r->m_bPlaying = TRUE;
            for (i = 0; i < r->m_numCalls; i++)
            {
                if (AVMATCH(&r->m_methodCalls[i].name, &av_play))
                {
                    AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
                    break;
                }
            }
        }

        else if (AVMATCH(&code, &av_NetStream_Publish_Start))
        {
            int i;
            r->m_bPlaying = TRUE;
            for (i = 0; i < r->m_numCalls; i++)
            {
                if (AVMATCH(&r->m_methodCalls[i].name, &av_publish))
                {
                    AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
                    break;
                }
            }
        }

        /* Return 1 if this is a Play.Complete or Play.Stop */
        else if (AVMATCH(&code, &av_NetStream_Play_Complete)
            || AVMATCH(&code, &av_NetStream_Play_Stop)
            || AVMATCH(&code, &av_NetStream_Play_UnpublishNotify))
        {
            RTMP_Close(r);
            if (r->stop_notify)
            {
                r->stop_notify(r->rvodCtx);
            }
            ret = 1;
        }

        else if (AVMATCH(&code, &av_NetStream_Seek_Notify))
        {
            r->m_read.flags &= ~RTMP_READ_SEEKING;
        }

        else if (AVMATCH(&code, &av_NetStream_Pause_Notify))
        {
            if (r->m_pausing == 1 || r->m_pausing == 2)
            {
                RTMP_SendPause(r, FALSE, r->m_pauseStamp);
                r->m_pausing = 3;
            }
        }
    }
    else if (AVMATCH(&method, &av_playlist_ready))
    {
        int i;
        for (i = 0; i < r->m_numCalls; i++)
        {
            if (AVMATCH(&r->m_methodCalls[i].name, &av_set_playlist))
            {
                AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
                break;
            }
        }
    }
    else if (AVMATCH(&method, &av_verifyClient))
    {
#if 0
        double VerificationNumber = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));
        RTMP_Log(RTMP_LOGDEBUG, "VerificationNumber: %.2f", VerificationNumber);

        enc = pbuf;
        enc = AMF_EncodeString(enc, pend, &av__result);
        enc = AMF_EncodeNumber(enc, pend, AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1)));
        *enc++ = AMF_NULL;
        enc = AMF_EncodeNumber(enc, pend, exp(atan(sqrt(VerificationNumber))) + 1);
        av_Response.av_val = pbuf;
        av_Response.av_len = enc - pbuf;

        AMF_Decode(&obj, av_Response.av_val, av_Response.av_len, FALSE);
        AMF_Dump(&obj);
        SendCustomCommand(r, &av_Response, FALSE);
#endif
    }
    else if (AVMATCH(&method, &av_sendStatus))
    {
        if (r->Link.WeebToken.av_len)
        {
            AVal av_Code = AVC("code");
            AVal av_Authorized = AVC("User.hasAccess");
            AVal av_TransferLimit = AVC("User.noPremium.limited");
            AVal av_UserLimit = AVC("User.noPremium.tooManyUsers");
            AVal av_TimeLeft = AVC("timeLeft");
            AVal av_Status, av_ReconnectionTime;

            AMFObject Status;
            AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &Status);
            AMFProp_GetString(AMF_GetProp(&Status, &av_Code, -1), &av_Status);
            RTMP_Log(RTMP_LOGINFO, "%.*s", av_Status.av_len, av_Status.av_val);
            if (AVMATCH(&av_Status, &av_Authorized))
            {
                RTMP_Log(RTMP_LOGINFO, "Weeb.tv authentication successful");
                RTMP_SendCreateStream(r);
            }
            else if (AVMATCH(&av_Status, &av_UserLimit))
            {
                RTMP_Log(RTMP_LOGINFO, "No free slots available");
                RTMP_Close(r);
            }
            else if (AVMATCH(&av_Status, &av_TransferLimit))
            {
                AMFProp_GetString(AMF_GetProp(&Status, &av_TimeLeft, -1), &av_ReconnectionTime);
                RTMP_Log(RTMP_LOGINFO, "Viewing limit exceeded. try again in %.*s minutes.", av_ReconnectionTime.av_len, av_ReconnectionTime.av_val);
                RTMP_Close(r);
            }
        }
    }
    else if (AVMATCH(&method, &av_ReceiveCheckPublicStatus))
    {
        AVal Status;
        AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &Status);
        param_count = strsplit(Status.av_val, Status.av_len, ',', &params);
        if (strcmp(params[0], "0") == 0)
        {
            RTMP_Log(RTMP_LOGINFO, "Model status is %s", params[1]);
            RTMP_Close(r);
        }
        else
        {
            AVal Playpath;
            char *str = ont_platform_malloc(sizeof("mp4:") + strlen(params[1]));
            str = strcpy(str, "mp4:");
            Playpath.av_val = strcat(str, params[1]);
            Playpath.av_len = strlen(Playpath.av_val);
            r->Link.playpath = Playpath;
            RTMP_SendCreateStream(r);
        }
    }
    else if (AVMATCH(&method, &av_NETStream_Pause) || AVMATCH(&method, &av_NETStream_PauseRaw))
    {
        unsigned char Status;
        double ts;
        static AVal av_status = AVC("status");
        static AVal av_pause_descvalue = AVC("Paused video on demand");
        static AVal av_unpause_descvalue = AVC("Unpaused video on demand");

        Status =  AMFProp_GetBoolean(AMF_GetProp(&obj, NULL, 3));
        ts = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 4));
        if (r->pause_notify == NULL)
        {
            goto leave;
        }

        if (Status)
        {
            (void)RTMP_SendStatus(r, &av_status, (AVal*)&av_NetStream_Pause_Notify, &av_pause_descvalue);
        }
        else
        {
            (void)RTMP_SendStatus(r, &av_status, (AVal*)&av_NetStream_UnPause_Notify, &av_unpause_descvalue);
        }

        /*sent pause notify.*/
        if (r->pause_notify)
        {
            (r->pause_notify)(r->rvodCtx, Status, ts);
        }

    }
    else if (AVMATCH(&method, &av_NETStream_Seek))
    {
        double ts;
        static const AVal av_status = AVC("status");
        static const AVal av_seek_descvalue = AVC("Seeking");

        ts = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));
        /*send stream EOF*/
        RTMP_SendCtrl(r, 1,0,0);
        
        RTMP_SendStatus(r, (AVal*)&av_status, (AVal*)&av_NetStream_Seek_Notify, (AVal*)&av_seek_descvalue);

        if (r->seek_notify != NULL)
        {
            (r->seek_notify)(r->rvodCtx, ts);
        }
        int i = 0;
        for (; i < RTMP_CHANNELS; i++)
        {
            /*clear the previous packet.*/
            char *prevPacket = (char *)r->m_vecChannelsOut[i];
            if (prevPacket)
            {
                ont_platform_free(prevPacket);
                r->m_vecChannelsOut[i] = NULL;
            }
        }

        RTMP_SendCtrl(r, 0, 0, 0);
    }
    else
    {

    }
leave:
    AMF_Reset(&obj);
    return ret;
}

int
RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal *name,
AMFObjectProperty * p)
{
    int n;
    /* this is a small object search to locate the "duration" property */
    for (n = 0; n < obj->o_num; n++)
    {
        AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);

        if (AVMATCH(&prop->p_name, name))
        {
            memcpy(p, prop, sizeof(*prop));
            return TRUE;
        }

        if (prop->p_type == AMF_OBJECT)
        {
            if (RTMP_FindFirstMatchingProperty(&prop->p_vu.p_object, name, p))
                return TRUE;
        }
    }
    return FALSE;
}

/* Like above, but only check if name is a prefix of property */
int
RTMP_FindPrefixProperty(AMFObject *obj, const AVal *name,
AMFObjectProperty * p)
{
    int n;
    for (n = 0; n < obj->o_num; n++)
    {
        AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);

        if (prop->p_name.av_len > name->av_len &&
            !memcmp(prop->p_name.av_val, name->av_val, name->av_len))
        {
            memcpy(p, prop, sizeof(*prop));
            return TRUE;
        }

        if (prop->p_type == AMF_OBJECT)
        {
            if (RTMP_FindPrefixProperty(&prop->p_vu.p_object, name, p))
                return TRUE;
        }
    }
    return FALSE;
}

static int
DumpMetaData(AMFObject *obj)
{
    AMFObjectProperty *prop;
    int n;
    for (n = 0; n < obj->o_num; n++)
    {
        prop = AMF_GetProp(obj, NULL, n);
        if (prop->p_type != AMF_OBJECT)
        {
            char str[256] = "";
            switch (prop->p_type)
            {
            case AMF_NUMBER:
                ont_platform_snprintf(str, 255, "%.2f", prop->p_vu.p_number);
                break;
            case AMF_BOOLEAN:
                ont_platform_snprintf(str, 255, "%s",
                    prop->p_vu.p_number != 0. ? "TRUE" : "FALSE");
                break;
            case AMF_STRING:
                ont_platform_snprintf(str, 255, "%.*s", prop->p_vu.p_aval.av_len,
                    prop->p_vu.p_aval.av_val);
                break;
            case AMF_DATE:
                ont_platform_snprintf(str, 255, "timestamp:%.2f", prop->p_vu.p_number);
                break;
            default:
                ont_platform_snprintf(str, 255, "INVALID TYPE 0x%02x",
                    (unsigned char)prop->p_type);
            }
            if (prop->p_name.av_len)
            {
                /* chomp */
                if (strlen(str) >= 1 && str[strlen(str) - 1] == '\n')
                    str[strlen(str) - 1] = '\0';
                RTMP_Log(RTMP_LOGINFO, "  %-22.*s%s", prop->p_name.av_len,
                    prop->p_name.av_val, str);
            }
        }
        else
        {
            if (prop->p_name.av_len)
                RTMP_Log(RTMP_LOGINFO, "%.*s:", prop->p_name.av_len, prop->p_name.av_val);
            DumpMetaData(&prop->p_vu.p_object);
        }
    }
    return FALSE;
}

SAVC(onMetaData);
SAVC(duration);
SAVC(video);
SAVC(audio);

static int
HandleMetadata(RTMP *r, char *body, unsigned int len)
{
    /* allright we get some info here, so parse it and print it */
    /* also keep duration or filesize to make a nice progress bar */

    AMFObject obj;
    AVal metastring;
    int ret = FALSE;

    int nRes = AMF_Decode(&obj, body, len, FALSE);
    if (nRes < 0)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, error decoding meta data packet", __FUNCTION__);
        return FALSE;
    }

    AMF_Dump(&obj);
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &metastring);

    if (AVMATCH(&metastring, &av_onMetaData))
    {
        AMFObjectProperty prop;
        /* Show metadata */
        RTMP_Log(RTMP_LOGINFO, "Metadata:");
        DumpMetaData(&obj);
        if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop))
        {
            r->m_fDuration = prop.p_vu.p_number;
            /*RTMP_Log(RTMP_LOGDEBUG, "Set duration: %.2f", m_fDuration); */
        }
        /* Search for audio or video tags */
        if (RTMP_FindPrefixProperty(&obj, &av_video, &prop))
            r->m_read.dataType |= 1;
        if (RTMP_FindPrefixProperty(&obj, &av_audio, &prop))
            r->m_read.dataType |= 4;
        ret = TRUE;
    }
    AMF_Reset(&obj);
    return ret;
}

static void
HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet)
{
    if (packet->m_nBodySize >= 4)
    {
        r->m_inChunkSize = AMF_DecodeInt32(packet->m_body);
        RTMP_Log(RTMP_LOGDEBUG, "%s, received: chunk size change to %d", __FUNCTION__,
            r->m_inChunkSize);
    }
}

static void
HandleAudio(RTMP *r, const RTMPPacket *packet)
{
}

static void
HandleVideo(RTMP *r, const RTMPPacket *packet)
{
}

static void
HandleCtrl(RTMP *r, const RTMPPacket *packet)
{
    short nType = -1;
    unsigned int tmp;
    if (packet->m_body && packet->m_nBodySize >= 2)
        nType = AMF_DecodeInt16(packet->m_body);
    RTMP_Log(RTMP_LOGDEBUG, "%s, received ctrl, type: %d, len: %d", __FUNCTION__, nType,
        packet->m_nBodySize);
    /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

    if (packet->m_nBodySize >= 6)
    {
        switch (nType)
        {
        case 0:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Begin %d", __FUNCTION__, tmp);
            break;

        case 1:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream EOF %d", __FUNCTION__, tmp);
            if (r->m_pausing == 1)
                r->m_pausing = 2;
            break;

        case 2:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Dry %d", __FUNCTION__, tmp);
            break;

        case 4:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream IsRecorded %d", __FUNCTION__, tmp);
            break;

        case 6:     /* server ping. reply with pong. */
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Ping %d", __FUNCTION__, tmp);
            RTMP_SendCtrl(r, 0x07, tmp, 0);
            break;

            /* FMS 3.5 servers send the following two controls to let the client
             * know when the server has sent a complete buffer. I.e., when the
             * server has sent an amount of data equal to m_nBufferMS in duration.
             * The server meters its output so that data arrives at the client
             * in realtime and no faster.
             *
             * The rtmpdump program tries to set m_nBufferMS as large as
             * possible, to force the server to send data as fast as possible.
             * In practice, the server appears to cap this at about 1 hour's
             * worth of data. After the server has sent a complete buffer, and
             * sends this BufferEmpty message, it will wait until the play
             * duration of that buffer has passed before sending a new buffer.
             * The BufferReady message will be sent when the new buffer starts.
             * (There is no BufferReady message for the very first buffer;
             * presumably the Stream Begin message is sufficient for that
             * purpose.)
             *
             * If the network speed is much faster than the data bitrate, then
             * there may be long delays between the end of one buffer and the
             * start of the next.
             *
             * Since usually the network allows data to be sent at
             * faster than realtime, and rtmpdump wants to download the data
             * as fast as possible, we use this RTMP_LF_BUFX hack: when we
             * get the BufferEmpty message, we send a Pause followed by an
             * Unpause. This causes the server to send the next buffer immediately
             * instead of waiting for the full duration to elapse. (That's
             * also the purpose of the ToggleStream function, which rtmpdump
             * calls if we get a read timeout.)
             *
             * Media player apps don't need this hack since they are just
             * going to play the data in realtime anyway. It also doesn't work
             * for live streams since they obviously can only be sent in
             * realtime. And it's all moot if the network speed is actually
             * slower than the media bitrate.
             */
        case 31:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
            if (!(r->Link.lFlags & RTMP_LF_BUFX))
                break;
            if (!r->m_pausing)
            {
                r->m_pauseStamp = r->m_channelTimestamp[r->m_mediaChannel];
                RTMP_SendPause(r, TRUE, r->m_pauseStamp);
                r->m_pausing = 1;
            }
            else if (r->m_pausing == 2)
            {
                RTMP_SendPause(r, FALSE, r->m_pauseStamp);
                r->m_pausing = 3;
            }
            break;

        case 32:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
            break;

        default:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            RTMP_Log(RTMP_LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
            break;
        }

    }

    if (nType == 0x1A)
    {
        RTMP_Log(RTMP_LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
        if (packet->m_nBodySize > 2 && packet->m_body[2] > 0x01)
        {
            RTMP_Log(RTMP_LOGERROR,
                "%s: SWFVerification Type %d request not supported, attempting to use SWFVerification Type 1! Patches welcome...",
                __FUNCTION__, packet->m_body[2]);
        }
#ifdef CRYPTO
        /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

        /* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
        if (r->Link.SWFSize)
        {
            RTMP_SendCtrl(r, 0x1B, 0, 0);
        }
        else
        {
            RTMP_Log(RTMP_LOGERROR,
                "%s: Ignoring SWFVerification request, use --swfVfy!",
                __FUNCTION__);
        }
#else
        RTMP_Log(RTMP_LOGERROR,
            "%s: Ignoring SWFVerification request, no CRYPTO support!",
            __FUNCTION__);
#endif
    }
}

static void
HandleServerBW(RTMP *r, const RTMPPacket *packet)
{
    r->m_nServerBW = AMF_DecodeInt32(packet->m_body);
    RTMP_Log(RTMP_LOGDEBUG, "%s: server BW = %d", __FUNCTION__, r->m_nServerBW);
}

static void
HandleClientBW(RTMP *r, const RTMPPacket *packet)
{
    r->m_nClientBW = AMF_DecodeInt32(packet->m_body);
    if (packet->m_nBodySize > 4)
        r->m_nClientBW2 = packet->m_body[4];
    else
        r->m_nClientBW2 = -1;
    RTMP_Log(RTMP_LOGDEBUG, "%s: client BW = %d %d", __FUNCTION__, r->m_nClientBW,
        r->m_nClientBW2);
}

static int
DecodeInt32LE(const char *data)
{
    unsigned char *c = (unsigned char *)data;
    unsigned int val;

    val = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
    return val;
}

static int
EncodeInt32LE(char *output, int nVal)
{
    output[0] = nVal;
    nVal >>= 8;
    output[1] = nVal;
    nVal >>= 8;
    output[2] = nVal;
    nVal >>= 8;
    output[3] = nVal;
    return 4;
}

int
RTMP_ReadPacket(RTMP *r, RTMPPacket *packet)
{
    uint8_t hbuf[RTMP_MAX_HEADER_SIZE] = { 0 };
    char *header = (char *)hbuf;
    int nSize, hSize, nToRead, nChunk;
    int didAlloc = FALSE;

    RTMP_Log(RTMP_LOGDEBUG2, "%s: sock=%p", __FUNCTION__, r->m_sb.ont_sock);


    if (ReadN(r, (char *)hbuf, 1) == 0)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header", __FUNCTION__);
        return FALSE;
    }

    packet->m_headerType = (hbuf[0] & 0xc0) >> 6;
    packet->m_nChannel = (hbuf[0] & 0x3f);
    header++;
    if (packet->m_nChannel == 0)
    {
        if (ReadN(r, (char *)&hbuf[1], 1) != 1)
        {
            RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 2nd byte",
                __FUNCTION__);
            return FALSE;
        }
        packet->m_nChannel = hbuf[1];
        packet->m_nChannel += 64;
        header++;
    }
    else if (packet->m_nChannel == 1)
    {
        int tmp;
        if (ReadN(r, (char *)&hbuf[1], 2) != 2)
        {
            RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 3nd byte",
                __FUNCTION__);
            return FALSE;
        }
        tmp = (hbuf[2] << 8) + hbuf[1];
        packet->m_nChannel = tmp + 64;
        RTMP_Log(RTMP_LOGDEBUG, "%s, m_nChannel: %0x", __FUNCTION__, packet->m_nChannel);
        header += 2;
    }

    nSize = packetSize[packet->m_headerType];

    if (nSize == RTMP_LARGE_HEADER_SIZE)    /* if we get a full header the timestamp is absolute */
        packet->m_hasAbsTimestamp = TRUE;

    else if (nSize < RTMP_LARGE_HEADER_SIZE)
    {               /* using values from the last message of this channel */
        if (r->m_vecChannelsIn[packet->m_nChannel])
            memcpy(packet, r->m_vecChannelsIn[packet->m_nChannel],
            sizeof(RTMPPacket));
    }

    nSize--;

    if (nSize > 0 && ReadN(r, header, nSize) != nSize)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header. type: %x",
            __FUNCTION__, (unsigned int)hbuf[0]);
        return FALSE;
    }

    hSize = nSize + (header - (char *)hbuf);

    if (nSize >= 3)
    {
        packet->m_nTimeStamp = AMF_DecodeInt24(header);

        /*RTMP_Log(RTMP_LOGDEBUG, "%s, reading RTMP packet chunk on channel %x, headersz %i, timestamp %i, abs timestamp %i", __FUNCTION__, packet.m_nChannel, nSize, packet.m_nTimeStamp, packet.m_hasAbsTimestamp); */

        if (nSize >= 6)
        {
            packet->m_nBodySize = AMF_DecodeInt24(header + 3);
            packet->m_nBytesRead = 0;
            RTMPPacket_Free(packet);

            if (nSize > 6)
            {
                packet->m_packetType = header[6];

                if (nSize == 11)
                    packet->m_nInfoField2 = DecodeInt32LE(header + 7);
            }
        }
        if (packet->m_nTimeStamp == 0xffffff)
        {
            if (ReadN(r, header + nSize, 4) != 4)
            {
                RTMP_Log(RTMP_LOGERROR, "%s, failed to read extended timestamp",
                    __FUNCTION__);
                return FALSE;
            }
            packet->m_nTimeStamp = AMF_DecodeInt32(header + nSize);
            hSize += 4;
        }
    }

    RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)hbuf, hSize);

    if (packet->m_nBodySize > 0 && packet->m_body == NULL)
    {
        if (!RTMPPacket_Alloc(packet, packet->m_nBodySize))
        {
            RTMP_Log(RTMP_LOGDEBUG, "%s, failed to allocate packet", __FUNCTION__);
            return FALSE;
        }
        didAlloc = TRUE;
        packet->m_headerType = (hbuf[0] & 0xc0) >> 6;
    }

    nToRead = packet->m_nBodySize - packet->m_nBytesRead;
    nChunk = r->m_inChunkSize;
    if (nToRead < nChunk)
        nChunk = nToRead;

    /* Does the caller want the raw chunk? */
    if (packet->m_chunk)
    {
        packet->m_chunk->c_headerSize = hSize;
        memcpy(packet->m_chunk->c_header, hbuf, hSize);
        packet->m_chunk->c_chunk = packet->m_body + packet->m_nBytesRead;
        packet->m_chunk->c_chunkSize = nChunk;
    }

    if (ReadN(r, packet->m_body + packet->m_nBytesRead, nChunk) != nChunk)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet body. len: %u",
            __FUNCTION__, packet->m_nBodySize);
        return FALSE;
    }

    RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)packet->m_body + packet->m_nBytesRead, nChunk);

    packet->m_nBytesRead += nChunk;

    /* keep the packet as ref for other packets on this channel */
    if (!r->m_vecChannelsIn[packet->m_nChannel])
        r->m_vecChannelsIn[packet->m_nChannel] = ont_platform_malloc(sizeof(RTMPPacket));
    memcpy(r->m_vecChannelsIn[packet->m_nChannel], packet, sizeof(RTMPPacket));

    if (RTMPPacket_IsReady(packet))
    {
        /* make packet's timestamp absolute */
        if (!packet->m_hasAbsTimestamp)
            packet->m_nTimeStamp += r->m_channelTimestamp[packet->m_nChannel];  /* timestamps seem to be always relative!! */

        r->m_channelTimestamp[packet->m_nChannel] = packet->m_nTimeStamp;

        /* reset the data from the stored packet. we keep the header since we may use it later if a new packet for this channel */
        /* arrives and requests to re-use some info (small packet header) */
        r->m_vecChannelsIn[packet->m_nChannel]->m_body = NULL;
        r->m_vecChannelsIn[packet->m_nChannel]->m_nBytesRead = 0;
        r->m_vecChannelsIn[packet->m_nChannel]->m_hasAbsTimestamp = FALSE;  /* can only be false if we reuse header */
    }
    else
    {
        if (didAlloc)
        {
            ont_platform_free(packet->m_body - RTMP_MAX_HEADER_SIZE);
            packet->m_body = NULL;  /* so it won't be erased on free */
        }
    }

    return TRUE;
}

#ifndef CRYPTO
static int
HandShake(RTMP *r, int FP9HandShake)
{
    int i;
    uint32_t uptime, suptime;
    int bMatch;
    char type;
    char clientbuf[RTMP_SIG_SIZE + 1], *clientsig = clientbuf + 1;
    char serversig[RTMP_SIG_SIZE];

    clientbuf[0] = 0x03;        /* not encrypted */
#ifdef ONT_PROTOCOL_VIDEO
    uptime = RTMP_GetTime();
#else
    uptime = htonl(RTMP_GetTime());
#endif    
    memcpy(clientsig, &uptime, 4);

    memset(&clientsig[4], 0, 4);

#ifdef ONT_PROTOCOL_VIDEO
    for (i = 8; i < RTMP_SIG_SIZE; i++)
        clientsig[i] = 0xff;
#else
    for (i = 8; i < RTMP_SIG_SIZE; i++)
        clientsig[i] = (char)(rand() % 256);
#endif

    if (!WriteN(r, clientbuf, RTMP_SIG_SIZE + 1, 0))
        return FALSE;

    if (ReadN(r, &type, 1) != 1)    /* 0x03 or 0x06 */
        return FALSE;

    RTMP_Log(RTMP_LOGDEBUG, "%s: Type Answer   : %02X", __FUNCTION__, type);

    if (type != clientbuf[0])
        RTMP_Log(RTMP_LOGWARNING, "%s: Type mismatch: client sent %d, server answered %d",
        __FUNCTION__, clientbuf[0], type);

    if (ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
        return FALSE;

    /* decode server response */

    memcpy(&suptime, serversig, 4);
    
#ifdef ONT_PROTOCOL_VIDEO
	(void)bMatch;
#else
    suptime = ntohl(suptime);
#endif

    RTMP_Log(RTMP_LOGDEBUG, "%s: Server Uptime : %d", __FUNCTION__, suptime);
    RTMP_Log(RTMP_LOGDEBUG, "%s: FMS Version   : %d.%d.%d.%d", __FUNCTION__,
        serversig[4], serversig[5], serversig[6], serversig[7]);

    /* 2nd part of handshake */
    if (!WriteN(r, serversig, RTMP_SIG_SIZE, 0))
        return FALSE;

    if (ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
        return FALSE;
#ifdef ONT_PROTOCOL_VIDEO

#else
    bMatch = (memcmp(serversig, clientsig, RTMP_SIG_SIZE) == 0);
    if (!bMatch)
    {
        RTMP_Log(RTMP_LOGWARNING, "%s, client signature does not match!", __FUNCTION__);
    }
#endif
    return TRUE;
}


int
RTMP_SendChunk(RTMP *r, RTMPChunk *chunk)
{
    int wrote;
    char hbuf[RTMP_MAX_HEADER_SIZE];

    RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)chunk->c_header, chunk->c_headerSize);
    if (chunk->c_chunkSize)
    {
        char *ptr = chunk->c_chunk - chunk->c_headerSize;
        RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)chunk->c_chunk, chunk->c_chunkSize);
        /* save header bytes we're about to overwrite */
        memcpy(hbuf, ptr, chunk->c_headerSize);
        memcpy(ptr, chunk->c_header, chunk->c_headerSize);
        wrote = WriteN(r, ptr, chunk->c_headerSize + chunk->c_chunkSize, 0);
        memcpy(ptr, hbuf, chunk->c_headerSize);
    }
    else
        wrote = WriteN(r, chunk->c_header, chunk->c_headerSize, 0);
    return wrote;
}

int
RTMP_SendPacket(RTMP *r, RTMPPacket *packet, int queue, int drop)
{
    const RTMPPacket *prevPacket = r->m_vecChannelsOut[packet->m_nChannel];
    uint32_t last = 0;
    int nSize;
    int hSize, cSize;
    char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
    uint32_t t;
    char *buffer, *tbuf = NULL, *toff = NULL;
    int nChunkSize;
    int tlen;

    if (prevPacket && packet->m_headerType != RTMP_PACKET_SIZE_LARGE)
    {
        /* compress a bit by using the prev packet's attributes */
        if (prevPacket->m_nBodySize == packet->m_nBodySize
            && prevPacket->m_packetType == packet->m_packetType
            && packet->m_headerType == RTMP_PACKET_SIZE_MEDIUM)
            packet->m_headerType = RTMP_PACKET_SIZE_SMALL;

        if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
            && packet->m_headerType == RTMP_PACKET_SIZE_SMALL)
            packet->m_headerType = RTMP_PACKET_SIZE_MINIMUM;
        last = prevPacket->m_nTimeStamp;
    }

    if (packet->m_headerType > 3)   /* sanity */
    {
        RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.",
            (unsigned char)packet->m_headerType);
        return FALSE;
    }

    nSize = packetSize[packet->m_headerType];
    hSize = nSize; cSize = 0;

    t = packet->m_nTimeStamp - last;
    /*t = packet->m_nTimeStamp;*/

    if (packet->m_body)
    {
        header = packet->m_body - nSize;
        hend = packet->m_body;
    }
    else
    {
        header = hbuf + 6;
        hend = hbuf + sizeof(hbuf);
    }

    if (packet->m_nChannel > 319)
        cSize = 2;
    else if (packet->m_nChannel > 63)
        cSize = 1;
    if (cSize)
    {
        header -= cSize;
        hSize += cSize;
    }

    if (nSize > 1 && t >= 0xffffff)
    {
        header -= 4;
        hSize += 4;
    }

    hptr = header;
    c = packet->m_headerType << 6;
    switch (cSize)
    {
    case 0:
        c |= packet->m_nChannel;
        break;
    case 1:
        break;
    case 2:
        c |= 1;
        break;
    }
    *hptr++ = c;
    if (cSize)
    {
        int tmp = packet->m_nChannel - 64;
        *hptr++ = tmp & 0xff;
        if (cSize == 2)
            *hptr++ = tmp >> 8;
    }

    if (nSize > 1)
    {
        hptr = AMF_EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
    }

    if (nSize > 4)
    {
        hptr = AMF_EncodeInt24(hptr, hend, packet->m_nBodySize);
        *hptr++ = packet->m_packetType;
    }

    if (nSize > 8)
        hptr += EncodeInt32LE(hptr, packet->m_nInfoField2);

    if (nSize > 1 && t >= 0xffffff)
        hptr = AMF_EncodeInt32(hptr, hend, t);

    nSize = packet->m_nBodySize;
    buffer = packet->m_body;
    nChunkSize = r->m_outChunkSize;

    RTMP_Log(RTMP_LOGDEBUG2, "%s: sock=%p, size=%d", __FUNCTION__, r->m_sb.ont_sock,
        nSize);
    /* send all chunks in one HTTP request */
    if (r->Link.protocol & RTMP_FEATURE_HTTP)
    {
        int chunks = (nSize + nChunkSize - 1) / nChunkSize;
        if (chunks > 1)
        {
            tlen = chunks * (cSize + 1) + nSize + hSize;
            tbuf = ont_platform_malloc(tlen);
            if (!tbuf)
                return FALSE;
            toff = tbuf;
        }
    }
    while (nSize + hSize)
    {
        int wrote;

        if (nSize < nChunkSize)
            nChunkSize = nSize;
        RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
        RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)buffer, nChunkSize);
        if (tbuf)
        {
            memcpy(toff, header, nChunkSize + hSize);
            toff += nChunkSize + hSize;
        }
        else
        {
            wrote = WriteN(r, header, nChunkSize + hSize, 0);
            if (!wrote)
                return FALSE;
        }
        nSize -= nChunkSize;
        buffer += nChunkSize;
        hSize = 0;

        if (nSize > 0)
        {
            header = buffer - 1;
            hSize = 1;
            if (cSize)
            {
                header -= cSize;
                hSize += cSize;
            }

            hptr = header;
            *hptr++ = (0xc0 | c);
            if (cSize)
            {
                int tmp = packet->m_nChannel - 64;
                *hptr++ = tmp & 0xff;
                if (cSize == 2)
                    *hptr++ = tmp >> 8;
            }
        }
    }
    if (tbuf)
    {
        int wrote = WriteN(r, tbuf, toff - tbuf, drop);
        ont_platform_free(tbuf);
        tbuf = NULL;
        if (!wrote)
            return FALSE;
    }

    /* we invoked a remote method */
    if (packet->m_packetType == RTMP_PACKET_TYPE_INVOKE)
    {
        AVal method;
        char *ptr;
        ptr = packet->m_body + 1;
        AMF_DecodeString(ptr, &method);
        RTMP_Log(RTMP_LOGDEBUG, "Invoking %s", method.av_val);
        /* keep it in call queue till result arrives */
        if (queue) {
            int txn;
            ptr += 3 + method.av_len;
            txn = (int)AMF_DecodeNumber(ptr);
            AV_queue(&r->m_methodCalls, &r->m_numCalls, &method, txn);
        }
    }

    if (!r->m_vecChannelsOut[packet->m_nChannel])
        r->m_vecChannelsOut[packet->m_nChannel] = ont_platform_malloc(sizeof(RTMPPacket));
    memcpy(r->m_vecChannelsOut[packet->m_nChannel], packet, sizeof(RTMPPacket));
    return TRUE;
}

int RTMP_SendStatus(RTMP *r, AVal *level, AVal *code, AVal *desc)
{
    char pbuf[256], *pend = pbuf + sizeof(pbuf), *enc;
    AVal av_Response;
    AVal av_onStatus = AVC("onStatus");  
    AVal av_level = AVC("level");
    AVal av_code = AVC("code");
    AVal av_desc = AVC("description");

    enc = pbuf;
    enc = AMF_EncodeString(enc, pend, &av_onStatus);
    enc = AMF_EncodeNumber(enc, pend, 0);
    *enc++ = AMF_NULL;
    *enc++ = AMF_OBJECT;
    enc = AMF_EncodeNamedString(enc, pend, &av_level, level);

    enc = AMF_EncodeNamedString(enc, pend, &av_code, code);
    enc = AMF_EncodeNamedString(enc, pend, &av_desc, desc);

    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    av_Response.av_val = pbuf;
    av_Response.av_len = enc - pbuf;

    return SendCustomCommand(r, &av_Response, FALSE);
}


int RTMP_SendPlayStatus(RTMP *r, AVal *level, AVal *code,  unsigned int ts, unsigned int bytes)
{
    char pbuf[256], *pend = pbuf + sizeof(pbuf), *enc;
    AVal av_Response;
    AVal av_onStatus = AVC("onPlayStatus");
    AVal av_level = AVC("level");
    AVal av_code = AVC("code");
    AVal av_dur = AVC("duration");
    AVal av_bytes = AVC("bytes");
    
    enc = pbuf;
    enc = AMF_EncodeString(enc, pend, &av_onStatus);
    enc = AMF_EncodeNumber(enc, pend, 0);
    *enc++ = AMF_NULL;
    *enc++ = AMF_OBJECT;
    enc = AMF_EncodeNamedString(enc, pend, &av_code, code);

    enc = AMF_EncodeNamedString(enc, pend, &av_level, level);

    enc = AMF_EncodeNamedNumber(enc, pend, &av_dur, ts);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_bytes, bytes);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    av_Response.av_val = pbuf;
    av_Response.av_len = enc - pbuf;

    return SendCustomCommand(r, &av_Response, FALSE);
}

void
RTMP_Close(RTMP *r)
{
    int i;

    if (RTMP_IsConnected(r))
    {
        if (r->m_stream_id > 0)
        {
            i = r->m_stream_id;
            r->m_stream_id = 0;
            if ((r->Link.protocol & RTMP_FEATURE_WRITE))
                SendFCUnpublish(r);
            SendDeleteStream(r, i);
        }
        RTMPSockBuf_Close(&r->m_sb);
    }

    r->m_stream_id = -1;
    r->m_sb.ont_sock =NULL;
    r->m_nBWCheckCounter = 0;
    r->m_nBytesIn = 0;
    r->m_nBytesInSent = 0;

    if (r->m_read.flags & RTMP_READ_HEADER) {
        ont_platform_free(r->m_read.buf);
        r->m_read.buf = NULL;
    }
    r->m_read.dataType = 0;
    r->m_read.flags = 0;
    r->m_read.status = 0;
    r->m_read.nResumeTS = 0;
    r->m_read.nIgnoredFrameCounter = 0;
    r->m_read.nIgnoredFlvFrameCounter = 0;

    r->m_write.m_nBytesRead = 0;
    RTMPPacket_Free(&r->m_write);

    for (i = 0; i < RTMP_CHANNELS; i++)
    {
        if (r->m_vecChannelsIn[i])
        {
            RTMPPacket_Free(r->m_vecChannelsIn[i]);
            ont_platform_free(r->m_vecChannelsIn[i]);
            r->m_vecChannelsIn[i] = NULL;
        }
        if (r->m_vecChannelsOut[i])
        {
            ont_platform_free(r->m_vecChannelsOut[i]);
            r->m_vecChannelsOut[i] = NULL;
        }
    }
    AV_clear(r->m_methodCalls, r->m_numCalls);
    r->m_methodCalls = NULL;
    r->m_numCalls = 0;
    r->m_numInvokes = 0;

    r->m_bPlaying = FALSE;
    r->m_sb.sb_size = 0;

    r->m_msgCounter = 0;
    r->m_resplen = 0;
    r->m_unackd = 0;

    ont_platform_free(r->Link.playpath0.av_val);
    r->Link.playpath0.av_val = NULL;

    if (r->Link.lFlags & RTMP_LF_FTCU)
    {
        ont_platform_free(r->Link.tcUrl.av_val);
        r->Link.tcUrl.av_val = NULL;
        r->Link.lFlags ^= RTMP_LF_FTCU;
    }

#ifdef CRYPTO
    if (r->Link.dh)
    {
        MDH_free(r->Link.dh);
        r->Link.dh = NULL;
    }
    if (r->Link.rc4keyIn)
    {
        RC4_free(r->Link.rc4keyIn);
        r->Link.rc4keyIn = NULL;
    }
    if (r->Link.rc4keyOut)
    {
        RC4_free(r->Link.rc4keyOut);
        r->Link.rc4keyOut = NULL;
    }
#endif
}

int
RTMPSockBuf_Fill(RTMPSockBuf *sb)
{
    int nBytes;

    if (!sb->sb_size)
        sb->sb_start = sb->sb_buf;

    while (1)
    {
        nBytes = sizeof(sb->sb_buf) - sb->sb_size - (sb->sb_start - sb->sb_buf);
#if defined(CRYPTO)
        if (sb->sb_ssl)
        {
            nBytes = TLS_read(sb->sb_ssl, sb->sb_start + sb->sb_size, nBytes);
        }
        else
#endif
        if (ont_platform_tcp_recv(sb->ont_sock, sb->sb_start + sb->sb_size, nBytes, &nBytes) < 0)
        {
            nBytes = -1;
            break;
        }
        else if (nBytes == 0) /*fake timeout, comment by fu.jianbo*/
        {
            sb->sb_timedout = TRUE;
        }

        if (nBytes != -1)
        {
            sb->sb_size += nBytes;
        }
        else
        {
#ifndef ONT_PROTOCOL_VIDEO
            int sockerr = GetSockError();
            RTMP_Log(RTMP_LOGDEBUG, "%s, recv returned %d. GetSockError(): %d (%s)",
                __FUNCTION__, nBytes, sockerr, strerror(sockerr));
            if (sockerr == EINTR && !RTMP_ctrlC)
                continue;
            if (sockerr == EWOULDBLOCK || sockerr == EAGAIN)
            {
                sb->sb_timedout = TRUE;
                nBytes = 0;
            }
#endif

        }
        break;
    }

    return nBytes == -1 ? nBytes : sb->sb_size;
}

int
RTMPSockBuf_Send(RTMPSockBuf *sb, const char *buf, int len)
{
    int rc;
#ifdef RTMP_NETSTACKDUMP
    static char splitchars[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n' };
    fwrite(buf, 1, len, netstackdump);
    fwrite(splitchars, 1, sizeof(splitchars), netstackdump);
#endif

#if defined(CRYPTO) && !defined(NO_SSL)
    if (sb->sb_ssl)
    {
        rc = TLS_write(sb->sb_ssl, buf, len);
    }
    else
#endif
    {
        if (ont_platform_tcp_send(sb->ont_sock, buf, len, &rc) < 0)
        {
            return -1;
        }
    }
    return rc;
}

int
RTMPSockBuf_Close(RTMPSockBuf *sb)
{
#if defined(CRYPTO) && !defined(NO_SSL)
    if (sb->sb_ssl)
    {
        TLS_shutdown(sb->sb_ssl);
        TLS_close(sb->sb_ssl);
        sb->sb_ssl = NULL;
    }
#endif
    if (sb->ont_sock)
    {
        ont_platform_tcp_close(sb->ont_sock);
        sb->ont_sock = NULL;
    }
    return 0;
}

#define HEX2BIN(a)  (((a)&0x40)?((a)&0xf)+9:((a)&0xf))

static void
DecodeTEA(AVal *key, AVal *text)
{
    uint32_t *v, k[4] = { 0 }, u;
    uint32_t z, y, sum = 0, e, DELTA = 0x9e3779b9;
    int32_t p, q;
    int i, n;
    unsigned char *ptr, *out;

    /* prep key: pack 1st 16 chars into 4 LittleEndian ints */
    ptr = (unsigned char *)key->av_val;
    u = 0;
    n = 0;
    v = k;
    p = key->av_len > 16 ? 16 : key->av_len;
    for (i = 0; i < p; i++)
    {
        u |= ptr[i] << (n * 8);
        if (n == 3)
        {
            *v++ = u;
            u = 0;
            n = 0;
        }
        else
        {
            n++;
        }
    }
    /* any trailing chars */
    if (u)
        *v = u;

    /* prep text: hex2bin, multiples of 4 */
    n = (text->av_len + 7) / 8;
    out = ont_platform_malloc(n * 8);
    ptr = (unsigned char *)text->av_val;
    v = (uint32_t *)out;
    for (i = 0; i < n; i++)
    {
        u = (HEX2BIN(ptr[0]) << 4) + HEX2BIN(ptr[1]);
        u |= ((HEX2BIN(ptr[2]) << 4) + HEX2BIN(ptr[3])) << 8;
        u |= ((HEX2BIN(ptr[4]) << 4) + HEX2BIN(ptr[5])) << 16;
        u |= ((HEX2BIN(ptr[6]) << 4) + HEX2BIN(ptr[7])) << 24;
        *v++ = u;
        ptr += 8;
    }
    v = (uint32_t *)out;

    /* http://www.movable-type.co.uk/scripts/tea-block.html */
#define MX (((z>>5)^(y<<2)) + ((y>>3)^(z<<4))) ^ ((sum^y) + (k[(p&3)^e]^z));
    z = v[n - 1];
    y = v[0];
    q = 6 + 52 / n;
    sum = q * DELTA;
    while (sum != 0)
    {
        e = sum >> 2 & 3;
        for (p = n - 1; p > 0; p--)
            z = v[p - 1], y = v[p] -= MX;
        z = v[n - 1];
        y = v[0] -= MX;
        sum -= DELTA;
    }

    text->av_len /= 2;
    memcpy(text->av_val, out, text->av_len);
    ont_platform_free(out);
}


#define MAX_IGNORED_FRAMES  50


static const char flvHeader[] = { 'F', 'L', 'V', 0x01,
0x00,               /* 0x04 == audio, 0x01 == video */
0x00, 0x00, 0x00, 0x09,
0x00, 0x00, 0x00, 0x00
};

#define HEADERBUF   (128*1024)

static const AVal av_setDataFrame = AVC("@setDataFrame");

int
RTMP_Write(RTMP *r, const char *buf, int size)
{
    RTMPPacket *pkt = &r->m_write;
    char *pend, *enc;
    int s2 = size, ret, num;

    pkt->m_nChannel = 0x04; /* source channel */
    pkt->m_nInfoField2 = r->m_stream_id;

    while (s2)
    {
        if (!pkt->m_nBytesRead)
        {
            if (size < 11) {
                /* FLV pkt too small */
                return 0;
            }

            if (buf[0] == 'F' && buf[1] == 'L' && buf[2] == 'V')
            {
                buf += 13;
                s2 -= 13;
            }

            pkt->m_packetType = *buf++;
            pkt->m_nBodySize = AMF_DecodeInt24(buf);
            buf += 3;
            pkt->m_nTimeStamp = AMF_DecodeInt24(buf);
            buf += 3;
            pkt->m_nTimeStamp |= *buf++ << 24;
            buf += 3;
            s2 -= 11;

            if (((pkt->m_packetType == RTMP_PACKET_TYPE_AUDIO
                || pkt->m_packetType == RTMP_PACKET_TYPE_VIDEO) &&
                !pkt->m_nTimeStamp) || pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
            {
                pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
                if (pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
                    pkt->m_nBodySize += 16;
            }
            else
            {
                pkt->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
            }

            if (!RTMPPacket_Alloc(pkt, pkt->m_nBodySize))
            {
                RTMP_Log(RTMP_LOGDEBUG, "%s, failed to allocate packet", __FUNCTION__);
                return FALSE;
            }
            enc = pkt->m_body;
            pend = enc + pkt->m_nBodySize;
            if (pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
            {
                enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
                pkt->m_nBytesRead = enc - pkt->m_body;
            }
        }
        else
        {
            enc = pkt->m_body + pkt->m_nBytesRead;
        }
        num = pkt->m_nBodySize - pkt->m_nBytesRead;
        if (num > s2)
            num = s2;
        memcpy(enc, buf, num);
        pkt->m_nBytesRead += num;
        s2 -= num;
        buf += num;
        if (pkt->m_nBytesRead == pkt->m_nBodySize)
        {
            ret = RTMP_SendPacket(r, pkt, FALSE, 0);
            RTMPPacket_Free(pkt);
            pkt->m_nBytesRead = 0;
            if (!ret)
                return -1;
            buf += 4;
            s2 -= 4;
            if (s2 < 0)
                break;
        }
    }
    return size + s2;
}

static int
SendCustomCommand(RTMP *r, AVal *Command, int queue)
{
    RTMPPacket packet;
    char pbuf[512], *enc;

    packet.m_nChannel = 0x03; /* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    if (Command->av_len)
    {
        memcpy(enc, Command->av_val, Command->av_len);
        enc += Command->av_len;
    }
    else
        return FALSE;
    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, queue, 0);
}

static int
strsplit(char *src, int srclen, char delim, char ***params)
{
    char *sptr, *srcbeg, *srcend, *dstr;
    int count = 1, i = 0, len = 0;

    if (src == NULL)
        return 0;
    if (!srclen)
        srclen = strlen(src);
    srcbeg = src;
    srcend = srcbeg + srclen;
    sptr = srcbeg;

    /* count the delimiters */
    while (sptr < srcend)
    {
        if (*sptr++ == delim)
            count++;
    }
    sptr = srcbeg;
    *params = calloc(count, sizeof(size_t));
    char **param = *params;

    for (i = 0; i < (count - 1); i++)
    {
        dstr = strchr(sptr, delim);
        len = dstr - sptr;
        param[i] = calloc(len + 1, sizeof(char));
        strncpy(param[i], sptr, len);
        sptr += len + 1;
    }

    /* copy the last string */
    if (sptr <= srcend)
    {
        len = srclen - (sptr - srcbeg);
        param[i] = calloc(len + 1, sizeof(char));
        strncpy(param[i], sptr, len);
    }
    return count;
}

static int
SendGetStreamLength(RTMP *r)
{
    char pbuf[256], *pend = pbuf + sizeof(pbuf), *enc;
    AVal av_Command;

    enc = pbuf;
    enc = AMF_EncodeString(enc, pend, &av_getStreamLength);
    enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
    av_Command.av_val = pbuf;
    av_Command.av_len = enc - pbuf;

    return SendCustomCommand(r, &av_Command, TRUE);
}

#endif
