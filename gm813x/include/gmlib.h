#ifndef __GMLIB_H__
#define __GMLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GM_VERSION_CODE 0x0034

/* return value definition */
#define GM_SUCCESS               0
#define GM_FD_NOT_EXIST         -1
#define GM_BS_BUF_TOO_SMALL     -2
#define GM_MV_BUF_TOO_SMALL     -3
#define GM_TIMEOUT              -4

/* Dimention definition */
typedef struct gm_dim {
    int width;
    int height;
} gm_dim_t;

/* Rectancle definition */
typedef struct gm_rect {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} gm_rect_t;

/***********************************
 *        System Information
 ***********************************/
typedef struct {
    int valid;
    int framerate;
    gm_dim_t dim;
    int duplicate; ///< duplicate=-1 (not duplicate) duplicate=vch (duplicate of vch)
                   ///< duplicate为-1表示LCD单独工作，duplicate为vch时表示与LCD vch播放内容相同
    unsigned int chipid;
    int reserved[9];         ///< Reserved words
} gm_lcd_sys_info_t;

typedef enum {
    GM_INTERLACED,
    GM_PROGRESSIVE,
    GM_RGB888,
    GM_ISP,
} gm_video_scan_method_t;

typedef struct {
    int valid;
    unsigned int number_of_path;
    gm_video_scan_method_t scan_method;
    int framerate;
    gm_dim_t dim;
    unsigned int chipid;
    int is_present;
    int reserved[8];         ///< Reserved words
} gm_cap_sys_info_t;

typedef enum {
    GM_MONO = 1,
    GM_STEREO,
} gm_audio_channel_type_t;

typedef struct {
    int valid;
    gm_audio_channel_type_t channel_type;
    int sample_rate;  /* 8000, 16000, 32000, 44100... */
    int sample_size;  /* bit number per sample, 8bit, 16bit... */
    int ssp;  /* Hardware SSP ID from 0,1... */
    int reserved[5];         ///< Reserved words
} gm_audio_grab_sys_info_t;

typedef struct {
    int valid;
    gm_audio_channel_type_t channel_type;
    int sample_rate;  /* 8000, 16000, 32000, 44100... */
    int sample_size;  /* bit number per sample, 8bit, 16bit... */
    int ssp;   /* Hardware SSP ID from 0,1... */
    int reserved[5];         ///< Reserved words
} gm_audio_render_sys_info_t;

#define CAPTURE_VCH_NUMBER      128
#define LCD_VCH_NUMBER          6
#define AUDIO_GRAB_VCH_NUMBER   32
#define AUDIO_RENDER_VCH_NUMBER 32

typedef struct {
    unsigned int graph_type;
    unsigned int gm_lib_version;
    gm_cap_sys_info_t cap[CAPTURE_VCH_NUMBER];
    gm_lcd_sys_info_t lcd[LCD_VCH_NUMBER];
    gm_audio_grab_sys_info_t au_grab[AUDIO_GRAB_VCH_NUMBER];
    gm_audio_render_sys_info_t au_render[AUDIO_RENDER_VCH_NUMBER];
    int reserved[5];         ///< Reserved words
} gm_system_t;


/***********************************
 *        Object Structure
 ***********************************/
typedef enum {
    GM_CAP_OBJECT = 0xFEFE0001,         ///< Capture object type
    GM_ENCODER_OBJECT = 0xFEFE0002,     ///< Encoder object type
    GM_WIN_OBJECT = 0xFEFE0003,         ///< Window object type
    GM_FILE_OBJECT = 0xFEFE0004,        ///< File object type
    GM_AUDIO_GRAB_OBJECT = 0xFEFE0005,     ///< Audio grabber object type
    GM_AUDIO_ENCODER_OBJECT = 0xFEFE0006,  ///< Audio encoder object type
    GM_AUDIO_RENDER_OBJECT = 0xFEFE0007,   ///< Audio renderer object type
} gm_obj_type_t;

typedef enum {
    GM_CBR = 1, ///< Constant Bitrate
    GM_VBR,     ///< Variable Bitrate
    GM_ECBR,    ///< Enhanced Constant Bitrate
    GM_EVBR,    ///< Enhanced Variable Bitrate
} gm_enc_ratecontrol_mode_t;


/***********************************
 *  Encode/Decode Bitstream Structure
 ***********************************/
#define GM_FLAG_NEW_FRAME_RATE  (1 << 3) ///< Indicate the bitstream of new frame rate setting
#define GM_FLAG_NEW_GOP         (1 << 4) ///< Indicate the bitstream of new GOP setting
#define GM_FLAG_NEW_DIM         (1 << 6) ///< Indicate the bitstream of new resolution setting
#define GM_FLAG_NEW_BITRATE     (1 << 7) ///< Indicate the bitstream of new bitrate setting
#define GM_MAX_ROI_QP_NUMBER   8

typedef enum  {
    GM_CHECKSUM_NONE = 0x0,           // no checksum
    GM_CHECKSUM_ALL_CRC = 0x101,      // all frames use crc method
    GM_CHECKSUM_ALL_SUM = 0x0102,     // all frames use sum method
    GM_CHECKSUM_ALL_4_BYTE = 0x103,   // all frames use 4 bytes sum method
    GM_CHECKSUM_ONLY_I_CRC = 0x201,   // only I frames use crc method
    GM_CHECKSUM_ONLY_I_SUM = 0x0202,  // only I frames use sum method
    GM_CHECKSUM_ONLY_I_4_BYTE = 0x203 // only I frames use 4 bytes sum method
} gm_checksum_type_t;

typedef enum  {
    GM_FASTFORWARD_NONE = 0,           // no fastforward
    GM_FASTFORWARD_1_FRAME = 2,        // skip 1 frame
    GM_FASTFORWARD_3_FRAMES = 4        // skip 3 frames
} gm_fast_forward_t;

typedef struct gm_enc_bitstream {
    /* provide by application */
    char *bs_buf;             ///< Bitstream buffer pointer
    unsigned int bs_buf_len;  ///< AP provide bs_buf max length
    char *mv_buf;             ///< NULL indicate no needed MV data
    unsigned int mv_buf_len;  ///< NULL indicate no needed MV data

    /* value return by gm_recv_bitstream() */
    unsigned int bs_len;      ///< Bitstream length
    unsigned int mv_len;      ///< Motion data length (移动向量长度)
    int keyframe;             ///< 1: keyframe 0: non-keyframe.
    unsigned int timestamp;   ///< Encode bitstream timestamp
    unsigned int newbs_flag;  ///< Flag notification of new seting, such as GM_FLAG_NEW_BITRATE
    unsigned int checksum;             ///< Checksum value
    int ref_frame;            ///< 1: reference frame, CANNOT skip, 0: not reference frame, can skip,
    unsigned int slice_offset[3];      ///< multi-slice offset 1~3 (first offset is 0)
    int reserved[5];          ///< Reserved words
} gm_enc_bitstream_t;

typedef struct gm_enc_multi_bitstream {
    void *bindfd;
    gm_enc_bitstream_t bs;
    int retval;  ///< less than 0: recv bistream fail.
    int reserved[6];         ///< Reserved words

#define GM_ENC_MAX_PRIVATE_DATA  28
    int enc_private[GM_ENC_MAX_PRIVATE_DATA]; ///< Library internal data, don't use it!
} gm_enc_multi_bitstream_t;

typedef enum {
    TIME_ALIGN_ENABLE  = 0xFEFE01FE,
    TIME_ALIGN_DISABLE = 0xFEFE07FE
} time_align_t;

/* NOTE: need 8bytes align for (* vpdDinBs_t)->timestamp */
typedef struct gm_dec_multi_bitstream {
    void *bindfd;
    char *bs_buf;
    unsigned int bs_buf_len;
    int retval;  ///< less than 0: send bistream fail.
    /* time_align (us): playback interval time by micro-second
        TIME_ALIGN_ENABLE(default): playback time align by LCD period (ex. 60HZ is 33333us)
        TIME_ALIGN_DISABLE: play timestamp by gm_send_multi_bitstreams called
        N(others): start to play at previous play point + N(us)
     */
    time_align_t time_align;
    int reserved[5];         ///< Reserved words

#define GM_DEC_MAX_PRIVATE_DATA         22
    int dec_private[GM_DEC_MAX_PRIVATE_DATA];   ///< Library internal data, don't use it!
} gm_dec_multi_bitstream_t;

/***************************************************
 * Capture motion dectection Structure (Multi)
 ***************************************************/
#define CAP_MOTION_SIZE     4096

typedef struct {
    /* provide md_buf by application, the max buf size is 4096*/
    char *md_buf;
    int md_buf_len;

    /* value return by gm_recv_multi_cap_md() */
    int is_valid;       ///< 1:valid, 0:invalid(It means cap_md has not been ready on this bindfd.)
    int md_len;
    gm_dim_t md_dim;    ///< MD Region dim
    gm_dim_t md_mb;     ///< MD Macro block
} gm_cap_md_info_t;

typedef struct gm_multi_cap_md {
    void *bindfd;
    gm_cap_md_info_t cap_md_info;
    int retval;   ///< less than 0: recv cap_md fail.

#define GM_MD_MAX_PRIVATE_DATA         16
    int cap_md_private[GM_MD_MAX_PRIVATE_DATA]; ///< Library internal data, don't use it!
} gm_multi_cap_md_t;


/***********************************
 *        Palette Structure
 ***********************************/
#define GM_MAX_PALETTE_IDX_NUM  16
typedef struct {
    ///< GM8210 support 16 palette numbers, GM8287/GM8283 support 8 palette numbers
    int palette_table[GM_MAX_PALETTE_IDX_NUM];
} gm_palette_table_t;


/***********************************
 *        OSD Structure
 ***********************************/
typedef enum {
    GM_OSD_ALIGN_TOP_LEFT = 0,
    GM_OSD_ALIGN_TOP_CENTER,
    GM_OSD_ALIGN_TOP_RIGHT,
    GM_OSD_ALIGN_BOTTOM_LEFT,
    GM_OSD_ALIGN_BOTTOM_CENTER,
    GM_OSD_ALIGN_BOTTOM_RIGHT,
    GM_OSD_ALIGN_CENTER,
    GM_OSD_ALIGN_NONE,                      ///< This mode is only for osd_mask with liveview_crop enabled.
} gm_osd_align_type_t;


typedef enum {
    GM_OSD_PRIORITY_MARK_ON_OSD = 0,        ///< Mark above OSD window
    GM_OSD_PRIORITY_OSD_ON_MARK             ///< Mark below OSD window
} gm_osd_priority_t;

typedef enum {
    GM_OSD_FONT_SMOOTH_LEVEL_WEAK = 0,      ///< weak smoothing effect
    GM_OSD_FONT_SMOOTH_LEVEL_STRONG         ///< strong smoothing effect
} gm_osd_font_smooth_level_t;

typedef struct {
    int enabled;    ///< OSD font smooth enable/disable 1 : enable, 0 : disable
    gm_osd_font_smooth_level_t level;      ///< OSD font smooth level
    int reserved[5];         ///< Reserved words
} gm_osd_smooth_t;

typedef enum {
    GM_OSD_MARQUEE_MODE_NONE = 0, ///< no marueee effect
    GM_OSD_MARQUEE_MODE_HLINE,    ///< one horizontal line marquee effect
    GM_OSD_MARQUEE_MODE_VLINE,    ///< one vertical line marquee effect
    GM_OSD_MARQUEE_MODE_HFLIP     ///< one horizontal line flip effect
} gm_osd_marquee_mode_t;

typedef enum {
    GM_OSD_MARQUEE_LENGTH_1024 = 0,   ///< 1024 step
    GM_OSD_MARQUEE_LENGTH_512,    ///< 512  step
    GM_OSD_MARQUEE_LENGTH_256,    ///< 256  step
    GM_OSD_MARQUEE_LENGTH_128,    ///< 128  step
    GM_OSD_MARQUEE_LENGTH_64,     ///< 64   step
    GM_OSD_MARQUEE_LENGTH_32,     ///< 32   step
    GM_OSD_MARQUEE_LENGTH_16,     ///< 16   step
    GM_OSD_MARQUEE_LENGTH_8,      ///< 8    step
    GM_OSD_MARQUEE_LENGTH_4       ///< 4    step
} gm_osd_marquee_length_t;

typedef struct {
    gm_osd_marquee_mode_t mode;
    /* Loop_time = (length(1024/512/256...) * (speed + 1)) / (30 or 60) */
    gm_osd_marquee_length_t length;  ///< OSD marquee length control
    int speed;   ///< OSD marquee speed  control, 1~3, 1:fastest, 3:slowest
    int reserved[5];         ///< Reserved words
} gm_osd_marquee_param_t;

typedef enum {
    GM_OSD_BORDER_TYPE_WIN = 0,     ///< treat border as window
    GM_OSD_BORDER_TYPE_FONT         ///< treat border as font
} gm_osd_border_type_t;

typedef struct {
    int enabled;        ///1:enable,0:disable
    int width;          ///< OSD window border width, 0~7, border width = 4x(n+1) pixels
    gm_osd_border_type_t type;  ///< OSD window border transparency type
    int palette_idx;    ///< OSD window border color, palette index 0~15
    int reserved[5];         ///< Reserved words
} gm_osd_border_param_t;

typedef enum {
    GM_OSD_FONT_ALPHA_0 = 0,  ///< alpha 0%
    GM_OSD_FONT_ALPHA_25,     ///< alpha 25%
    GM_OSD_FONT_ALPHA_37_5,   ///< alpha 37.5%
    GM_OSD_FONT_ALPHA_50,     ///< alpha 50%
    GM_OSD_FONT_ALPHA_62_5,   ///< alpha 62.5%
    GM_OSD_FONT_ALPHA_75,     ///< alpha 75%
    GM_OSD_FONT_ALPHA_87_5,   ///< alpha 87.5%
    GM_OSD_FONT_ALPHA_100     ///< alpha 100%
} gm_osd_font_alpha_t;

typedef enum {
    GM_OSD_FONT_ZOOM_NONE = 0,      ///< disable zoom
    GM_OSD_FONT_ZOOM_2X,   ///< horizontal and vertical zoom in 2x
    GM_OSD_FONT_ZOOM_3X,   ///< horizontal and vertical zoom in 3x
    GM_OSD_FONT_ZOOM_4X,   ///< horizontal and vertical zoom in 4x
    GM_OSD_FONT_ZOOM_ONE_HALF,      ///< horizontal and vertical zoom out 2x

    GM_OSD_FONT_ZOOM_H2X_V1X = 8,   ///< horizontal zoom in 2x
    GM_OSD_FONT_ZOOM_H4X_V1X,  ///< horizontal zoom in 4x
    GM_OSD_FONT_ZOOM_H4X_V2X,  ///< horizontal zoom in 4x and vertical zoom in 2x

    GM_OSD_FONT_ZOOM_H1X_V2X = 12,  ///< vertical zoom in 2x
    GM_OSD_FONT_ZOOM_H1X_V4X,  ///< vertical zoom in 4x
    GM_OSD_FONT_ZOOM_H2X_V4X,  ///< horizontal zoom in 2x and vertical zoom in 4x
} gm_osd_font_zoom_t;

#define GM_MAX_OSD_FONTS        31
#define GM_MAX_OSD_MARK_IMG_NUM 4

typedef enum {
    GM_OSD_MARK_DIM_16 = 0,  ///< 16  pixel/line
    GM_OSD_MARK_DIM_32,  ///< 32  pixel/line
    GM_OSD_MARK_DIM_64,  ///< 64  pixel/line
    GM_OSD_MARK_DIM_128, ///< 128 pixel/line
    GM_OSD_MARK_DIM_256, ///< 256 pixel/line
    GM_OSD_MARK_DIM_512, ///< 512 pixel/line
    GM_OSD_MARK_DIM_MAX,
} gm_osd_mark_dim_t;

typedef struct {
    int mark_exist;
    char *mark_yuv_buf;
    unsigned int mark_yuv_buf_len;
    gm_osd_mark_dim_t mark_width; ///< GM8210: (w*h*2)<16384 bytes   GM8287: (w*h*2)<8192 bytes, enable osg when width > 5
    gm_osd_mark_dim_t mark_height;///< GM8210: (w*h*2)<16384 bytes   GM8287: (w*h*2)<8192 bytes, enable osg when height > 5
    int osg_tp_color;            ///< reserved, do not use it. 
    unsigned short  osg_mark_idx;      ///< mark_idx for osg
    char reserved[14];         ///< Reserved words
} gm_osd_img_param_t;

typedef struct {
    gm_osd_img_param_t mark_img[GM_MAX_OSD_MARK_IMG_NUM]; //osg mode: only mark_img[0]
    int reserved[5];         ///< Reserved words
} gm_osd_mark_img_table_t;

/* gm_osd_font_t supoprts maximum GM_MAX_OSD_FONTS=31 fonts in single windows */
typedef struct {
    /* GM8210/GM828x/GM8139:
         win_idx: 0(highest priority), 1, 2...7(lowest priority)
       Note:
         gm_osd_mask_t with GM_THIS_PATH: gm_osd_mask_t->mask_idx is indexed with win_idx together
     */
    int win_idx;
    int enabled;
    gm_osd_align_type_t align_type;
    unsigned int x;     ///< The x axis in OSD window (pixel unit)
    unsigned int y;     ///< The y axis in OSD window (pixel unit)
    unsigned int h_words;   ///< The horizontal words in OSD window (word unit)
    unsigned int v_words;   ///< The vertical words in OSD window (word unit)
    unsigned int h_space;   ///< The horizontal space between charater and charater (pixel unit)
    unsigned int v_space;   ///< The vertical space between charater and charater (pixel unit)
    int font_index_len;     ///< Indicate used length of font index

    unsigned short font_index[GM_MAX_OSD_FONTS];   ///< OSD font index table in this OSD window
    gm_osd_font_alpha_t font_alpha; // GM8210:all font use the same color  GM8287:fonts can diff colors
    gm_osd_font_alpha_t win_alpha;

    int font_palette_idx;   ///< font color, GM8120 index from 0~15
    int win_palette_idx;    ///< window background color, GM8120 index from 0~15

    gm_osd_priority_t priority;  ///< OSD & Mark layer spriority
    gm_osd_smooth_t smooth;
    gm_osd_marquee_param_t marquee;
    gm_osd_border_param_t border;
    gm_osd_font_zoom_t font_zoom;
    int reserved[5];         ///< Reserved words
} gm_osd_font_t;

/* gm_osd_font2_t support dynamic font_index with user specified font number,
   compared with gm_osd_font_t */
typedef struct {
    /* GM8210/GM828x/GM8139:
         win_idx: 0(highest priority), 1, 2...7(lowest priority)
       Note:
         gm_osd_mask_t with GM_THIS_PATH: gm_osd_mask_t->mask_idx is indexed with win_idx together
     */
    int win_idx;
    int enabled;
    gm_osd_align_type_t align_type;
    unsigned int x;     ///< The x axis in OSD window (pixel unit)
    unsigned int y;     ///< The y axis in OSD window (pixel unit)
    unsigned int h_words;   ///< The horizontal words in OSD window (word unit)
    unsigned int v_words;   ///< The vertical words in OSD window (word unit)
    unsigned int h_space;   ///< The horizontal space between charater and charater (pixel unit)
    unsigned int v_space;   ///< The vertical space between charater and charater (pixel unit)
    int font_index_len;     ///< Indicate used length of font index
    unsigned short *font_index;   ///< OSD font index table in this OSD window, allocated by AP
    gm_osd_font_alpha_t font_alpha;
    gm_osd_font_alpha_t win_alpha; 
    int font_palette_idx;   ///< font color, GM8120 index from 0~15
    int win_palette_idx;    ///< window background color, GM8120 index from 0~15
    gm_osd_priority_t priority;     ///< OSD & Mark layer spriority
    gm_osd_smooth_t smooth;   
    gm_osd_marquee_param_t marquee;
    gm_osd_border_param_t border;  
    gm_osd_font_zoom_t font_zoom;  
    int reserved[5];         ///< Reserved words
} gm_osd_font2_t;

#define GM_OSD_FONT_MAX_ROW 18
typedef struct {
    int font_idx;                                 ///< font index, 0 ~ (osd_char_max - 1)
    unsigned short  bitmap[GM_OSD_FONT_MAX_ROW];    ///< GM8210 only 18 row (12 bits data + 4bits reserved)
    int reserved[5];         ///< Reserved words
} gm_osd_font_update_t;


typedef enum {
    GM_OSD_MASK_ALPHA_0 = 0, ///< alpha 0%
    GM_OSD_MASK_ALPHA_25,    ///< alpha 25%
    GM_OSD_MASK_ALPHA_37_5,  ///< alpha 37.5%
    GM_OSD_MASK_ALPHA_50,    ///< alpha 50%
    GM_OSD_MASK_ALPHA_62_5,  ///< alpha 62.5%
    GM_OSD_MASK_ALPHA_75,    ///< alpha 75%
    GM_OSD_MASK_ALPHA_87_5,  ///< alpha 87.5%
    GM_OSD_MASK_ALPHA_100    ///< alpha 100%
} gm_osd_mask_alpha_t;

typedef enum {
    GM_OSD_MASK_BORDER_TYPE_HOLLOW = 0,
    GM_OSD_MASK_BORDER_TYPE_TRUE
} gm_osd_mask_border_type_t;

typedef enum {
    GM_ALL_PATH = 0,
    GM_THIS_PATH
} gm_path_mode_t;

typedef struct {
    int width;    ///< Mask window border width when hollow on, 0~15, border width = 2x(n+1) pixels
    gm_osd_mask_border_type_t type;  ///< Mask window hollow/true
    int reserved[5];         ///< Reserved words
} gm_osd_mask_border_t;

typedef struct {
    /* GM8210/GM828x/GM8139:
        GM_ALL_PATH: apply in all capture path, implement by capture mask engine
            mask_idx= 0(lowest priority), 1, 2, ... 7 (highest priority)
        GM_THIS_PATH: apply in specified capture path, implement by capture OSD engine
                      (mask_idx & gm_osd_font_t->win_idx is indexed together)
            mask_idx= 0(highest priority), 1, 2, ... 7 (lowest priority)
    */
    int mask_idx;   ///< The mask window index
    int enabled;
    unsigned int x;    ///< Left position of mask window
    unsigned int y;    ///< Top position of mask window
    unsigned int width;    ///< The dimension width of mask window
    unsigned int height;   ///< The dimension height of mask window
    gm_osd_mask_alpha_t alpha;
    int palette_idx; ///< The mask palette_idx vlaue range:(0~3 at GM_THIS_PATH), (0~15 at GM_ALL_PATH)
    gm_osd_mask_border_t border;   ///< The feature is only available at GM_ALL_PATH.
    gm_osd_align_type_t align_type;
    int reserved[5];         ///< Reserved words
} gm_osd_mask_t;

typedef enum {
    GM_OSD_MARK_ALPHA_0 = 0,///< alpha 0%
    GM_OSD_MARK_ALPHA_25,   ///< alpha 25%
    GM_OSD_MARK_ALPHA_37_5, ///< alpha 37.5%
    GM_OSD_MARK_ALPHA_50,   ///< alpha 50%
    GM_OSD_MARK_ALPHA_62_5, ///< alpha 62.5%
    GM_OSD_MARK_ALPHA_75,   ///< alpha 75%
    GM_OSD_MARK_ALPHA_87_5, ///< alpha 87.5%
    GM_OSD_MARK_ALPHA_100   ///< alpha 100%
} gm_osd_mark_alpha_t;

typedef enum {
    GM_OSD_MARK_ZOOM_1X = 0,///< zoom in lx
    GM_OSD_MARK_ZOOM_2X,    ///< zoom in 2X
    GM_OSD_MARK_ZOOM_4X     ///< zoom in 4x
} gm_osd_mark_zoom_t;

typedef struct {
    /* GM8210/GM828x/GM8139:
        mark_idx= 0(highest priority), 1, 2, 3 (lowest priority)
    */
    int mark_idx;   ///< The mask window index, normal:0~3, osg_mode: >= 4
    int enabled;
    unsigned int x; ///< Left position of mask window
    unsigned int y; ///< Top position of mask window
    gm_osd_mark_alpha_t alpha;
    gm_osd_mark_zoom_t zoom; ///< when osg mode, only support 0
    gm_osd_align_type_t align_type;
    unsigned short osg_mark_idx; ///< when osg mode, specify the mark image
    char reserved[18];         ///< Reserved words
} gm_osd_mark_t;


/***********************************
 *       Attribute Structure
 ***********************************/
typedef struct {
    int data[8];
} gm_priv_t;

typedef struct {
    gm_enc_ratecontrol_mode_t mode;
    int gop;
    int init_quant;
    int min_quant;
    int max_quant;
    int bitrate;
    int bitrate_max;
    int reserved[5];         ///< Reserved words
} gm_enc_ratecontrol_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int cap_vch;    ///< Capture virtual channel

    //GM8210 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can scaling)
    //GM8139/GM8287 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can't scaling down)
    int path;
    int enable_mv_data;
    int I2P_threshold; ///< (percentage) interlace to progressive threshold, default is 0 (disable)
    ///< if dest_w < (src_w * threshold), transfer interlace to progressive by cap, then without 3DI action

    unsigned int dma_path;    //DMA path 0, 1, ...
    unsigned short prescale_reduce_width;  ///< It is used for capture prescale reduce width adjust
    unsigned short prescale_reduce_height; ///< It is used for capture prescale reduce height adjust
    int reserved[2]; ///< Reserved words, 
} gm_cap_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    gm_rect_t src_crop_rect;
    char cvbs_quality;         ///< 0:low quality, 1:high_quality
    char reserved[19];         ///< Reserved words
} gm_crop_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int deinterlace;  ///< temporal_deInterlace=1(enabled), 0(disabled)
    int denoise;  ///< temporal_deInterlace=1(enabled), 0(disabled)
    int stand_alone;  ///<stand_along=1(external 3di h/w), 0(embedded 3di if available by codec)
    int reserved[4];         ///< Reserved words
} gm_3di_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    int reserved[5];         ///< Reserved words
} gm_3dnr_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    int clockwise;
    int reserved[4];         ///< Reserved words
} gm_rotation_attr_t;

typedef struct  {
    int h_flip_enabled;
    int v_flip_enabled;
    int reserved[5];         ///< Reserved words
} gm_cap_flip_t;

typedef struct {
    unsigned short tamper_sensitive_b; ///< Tamper detection black sensitive, 0 ~ 100, 0: for disable
    unsigned short tamper_threshold; ///< Tamper detection luminance index, 1 ~ 255
    unsigned short tamper_sensitive_h; ///< Tamper detection homogenous sensitive, 0 ~ 100, 0: for disable
    int reserved[5];         ///< Reserved words
} gm_cap_tamper_t;

typedef struct {
    unsigned int id;    ///< specified motion parameter ID, such as VCAP_API_MD_PARAM_ALPHA...
    unsigned int value;
} gm_cap_motion_t;

typedef enum {
    GM_H264E_DEFAULT_PROFILE = 0, 
    GM_H264E_BASELINE_PROFILE = 66, 
    GM_H264E_MAIN_PROFILE = 77,
    GM_H264E_HIGH_PROFILE = 100
} gm_h264e_profile_t;

typedef enum {
    GM_H264E_DEFAULT_LEVEL = 0, 
    GM_H264E_LEVEL_1 = 10,
    GM_H264E_LEVEL_1_1 = 11,
    GM_H264E_LEVEL_1_2 = 12,
    GM_H264E_LEVEL_1_3 = 13,
    GM_H264E_LEVEL_2 = 20,
    GM_H264E_LEVEL_2_1 = 21,
    GM_H264E_LEVEL_2_2 = 22,
    GM_H264E_LEVEL_3 = 30,
    GM_H264E_LEVEL_3_1 = 31,
    GM_H264E_LEVEL_3_2 = 32,
    GM_H264E_LEVEL_4 = 40,
    GM_H264E_LEVEL_4_1 = 41,
    GM_H264E_LEVEL_4_2 = 42,
    GM_H264E_LEVEL_5 = 50,
    GM_H264E_LEVEL_5_1 = 51
} gm_h264e_level_t;

typedef enum {
    GM_H264E_DEFAULT_CONFIG = 0, 
    GM_H264E_PERFORMANCE_CONFIG = 1,
    GM_H264E_LIGHT_QUALITY_CONFIG = 2,
    GM_H264E_QUALITY_CONFIG = 3
} gm_h264e_config_t;

typedef enum {
    GM_H264E_DEFAULT_CODING = 0, 
    GM_H264E_CABAC_CODING = 1,
    GM_H264E_CAVLC_CODING = 2,
} gm_h264e_coding_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    gm_dim_t dim;
    union {
        int framerate;
        struct {
            int numerator:16; //分子
            int denominator:16; //分母
        } fps_ratio;
    } frame_info;
    gm_enc_ratecontrol_t ratectl;
    int b_frame_num;
    int enable_mv_data;
    struct {
        gm_h264e_profile_t profile:8;
        gm_h264e_level_t level:8;
        gm_h264e_config_t config:8;
        gm_h264e_coding_t coding:8;
    } profile_setting;
    struct {
        char ip_offset:8;    
        char roi_delta_qp:8;
        char reserved1:8;
        char reserved2:8;
    } qp_offset;
    gm_checksum_type_t checksum_type;
    gm_fast_forward_t fast_forward;
    int reserved[1];         ///< Reserved words
} gm_h264e_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    gm_dim_t dim;
    union {
        int framerate;
        struct {
            int numerator:16; //分子
            int denominator:16; //分母
        } fps_ratio;
    } frame_info;
    gm_enc_ratecontrol_t ratectl;
    int reserved[5];         ///< Reserved words
} gm_mpeg4e_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    gm_dim_t dim;
    union {
        int framerate;
        struct {
            int numerator:16; //分子
            int denominator:16; //分母
        } fps_ratio;
    } frame_info;
    int quality;   ///< 1(bad quality) ~ 100(best quality)
    gm_enc_ratecontrol_mode_t mode;
    int bitrate;
    int bitrate_max;
    int reserved[2];         ///< Reserved words
} gm_mjpege_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    gm_dim_t dim;
    union {
        int framerate;
        struct {
            int numerator:16; //分子
            int denominator:16; //分母
        } fps_ratio;
    } frame_info;
    int reserved[5];         ///< Reserved words
} gm_raw_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    gm_rect_t rect;
    int reserved[5];         ///< Reserved words
} gm_enc_roi_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    gm_dim_t src_dim;
    gm_rect_t src_crop_rect;
    int reserved[5];         ///< Reserved words
} gm_enc_eptz_attr_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int vch;        ///< File virtual channel (any value given by application)
    int max_width;          ///< for video data, specify its max width
    int max_height;         ///< for video data, specify its max height
    int max_fps;            ///< for video data, specify its max framerate
    unsigned short sample_rate;     ///< for audio data, specify its sample rate
    unsigned short sample_size;     ///< for audio data, specify its sample size
    gm_audio_channel_type_t channel_type;   ///< for audio data, specify its channel counts

    int reserved[3];         ///< Reserved words
} gm_file_attr_t;

#define GM_LCD0      0  ///< Indicate lcd_vch value
#define GM_LCD1      1  ///< Indicate lcd_vch value
#define GM_LCD2      2  ///< Indicate lcd_vch value
#define GM_LCD3      3  ///< Indicate lcd_vch value
#define GM_LCD4      4  ///< Indicate lcd_vch value
#define GM_LCD5      5  ///< Indicate lcd_vch value

typedef enum {
    WIN_LAYER1 = 0,         // background
    WIN_LAYER2,             // second layer
    COUNT_OF_WIN_LAYERS
} gm_win_layer_t;

typedef enum {
    DISABLE_WIN_TO_ENC = 0,
    FROM_MULTIWIN = 1,
    FROM_MAIN_DISPLAY = 2,
} gm_win_to_enc_t;

typedef struct {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int lcd_vch;   ///< Display virtual channel
    int visible;
    gm_rect_t rect;
    gm_win_to_enc_t win_to_enc;
    gm_win_layer_t  layer;
    int bg_dim_width;
    int bg_dim_height;
    unsigned int feature_bmp;   ///< bit0 : method 0 and 4, enabled 1_frame 60i display
                                ///< bit31: is reserved, please don't use it
} gm_win_attr_t;

typedef struct {
    gm_priv_t priv;
    int enabled;
    int palette_idx;  ///< Display window border color, GM8210 palette index 0~7
    int reserved[5];         ///< Reserved words
} gm_win_aspect_ratio_attr_t;

typedef struct {
    gm_priv_t priv;
    int enabled;
    int width;  ///< Display window border width, 0~7, border width = 4x(n+1) pixels
    int palette_idx;    ///< Display window border color, GM8210 palette index 0~7
    int reserved[5];         ///< Reserved words
} gm_win_border_attr_t;

#define ADD_CH_TO_BMP(bmp, ch)      bmp |= (1 << (ch))
#define DEL_CH_FROM_BMP(bmp, ch)    bmp &= (~ (1 << (ch)))
typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int vch;
    int sample_rate;
    int sample_size;
    gm_audio_channel_type_t channel_type;
    int reserved[5];         ///< Reserved words
} gm_audio_grab_attr_t;

typedef enum {
    GM_PCM = 1,
    GM_AAC,
    GM_ADPCM,
    GM_G711_ALAW,
    GM_G711_ULAW
} gm_audio_encode_type_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    gm_audio_encode_type_t encode_type;
    int bitrate;  ///< ADPCM: 16000/32000  AAC: 14500~192000
    int block_count; ///< The count of audio frames received in an receive operation (fix to 2 now)

    /* The count of samples per audio frame, suggestion:
        PCM:250~2048
        AAC: 1024*n(mono) 2048*n(stereo)
        ADPCM: 505*n(mono) 498*n(stereo)
        G711: 320*n(mono) 640*n(stereo)
         (n: 1,2,...)
     */
    int frame_samples;
    int reserved[4];         ///< Reserved words
} gm_audio_enc_attr_t;

#define SYNC_LCD_DISABLE    0xFEFEFEFE
typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int vch;
    gm_audio_encode_type_t encode_type;
    int block_size;	///< PCM:1~2048 AAC:1024 ADPCM:256 G711:320
    int sync_with_lcd_vch;   ///< value: SYNC_LCD_DISABLE, GM_LCD0, GM_LCD1,...
    int reserved[4];         ///< Reserved words
} gm_audio_render_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int multi_slice; ///< multi slice number, max value is 4
    int field_coding; ///< 1:enable 0:disable
    int gray_scale; ///< 1:enable 0:disable
    int reserved[5];         ///< Reserved words
} gm_h264_advanced_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int enabled;
    gm_rect_t rect[GM_MAX_ROI_QP_NUMBER];
    int reserved[5];         ///< Reserved words
} gm_h264_roiqp_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    int pattern;
    int reserved[5];         ///< Reserved words
} gm_h264_watermark_attr_t;

typedef struct  {
    gm_priv_t priv; ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
    union {
        int value;
        struct {
            char matrix_coefficient;
            char transfer_characteristics;
            char colour_primaries;
            char full_range : 1;
            char video_format : 3;
            char timing_info_present_flag : 1;
      } param;
    } param_info;
    union {
        int value;
        struct {
          unsigned short sar_width;
          unsigned short sar_height;
        } sar;
    } sar_info;
    int reserved[5];         ///< Reserved words
} gm_h264_vui_attr_t;

/***********************************
 *       poll Structure
 ***********************************/
#define GM_POLL_READ            1
#define GM_POLL_WRITE           2

typedef struct {
    unsigned int event;
    unsigned int bs_len;
    unsigned int mv_len;
    unsigned int keyframe;  // 1: keyframe, 0: non-keyframe
} gm_ret_event_t;

#define GM_FD_MAX_PRIVATE_DATA         4
typedef struct {
    void *bindfd;    ///< The return value of gm_bind
    unsigned int event;     ///< The poll event ID,
    gm_ret_event_t revent;  ///< The returned event value
    int fd_private[GM_FD_MAX_PRIVATE_DATA];  ///< Library internal data, don't use it! (Lib库内部数据，请不要使用)
} gm_pollfd_t;


/***********************************
 *        Snapshot Structure
 ***********************************/

typedef struct snapshot {
    void *bindfd;
    int image_quality;  ///< The value of image quality from 1(worst) ~ 100(best)
    char *bs_buf;
    unsigned int bs_buf_len; ///< User given parepred bs_buf length, gm_lib returns actual length
    int bs_width;   ///< bitstream width, support range 128 ~ 720
    int bs_height;  ///< bitstream height, support range 96 ~ 576
#define MODE_INTERFERENCE  0x2694
    unsigned int extra_mode; ///0x2694:Interference(JPEG only for cap vch0 without scaling)
    int reserved[2];
} snapshot_t;

#define SNAPSHOT_JPEG   0
#define SNAPSHOT_H264   1

typedef enum {
    GM_SNAPSHOT_P_FRAME = 0,
    GM_SNAPSHOT_I_FRAME = 2,
} slice_type_t;

typedef struct disp_snapshot {
    int lcd_vch;    ///< only support LCD0 now
    int image_quality;  ///< JPEG quality 1(worst) ~ 100(best), H264 quant 0 ~ 51
    char *bs_buf;
    unsigned int bs_buf_len; ///< User given parepred bs_buf length, gm_lib returns actual length
    int bs_width;   ///< bitstream width, support range 128 ~ 720
    int bs_height;  ///< bitstream height, support range 96 ~ 576
    int bs_type;    ///< bitstream type ==> 0: SNAPSHOT_JPEG  1: SNAPSHOT_H264
    int slice_type; ///< frame type==> GM_SNAPSHOT_P_FRAME/GM_SNAPSHOT_I_FRAME
    int reserved[4];
} disp_snapshot_t;

/***********************************
 *       Get Raw Data 
 ***********************************/
typedef enum {
    GM_PARTIAL_FROM_CAP_OBJ = 0xFBFB9933,
    GM_WHOLE_FROM_ENC_OBJ = 0xFBFC9934,
} gm_rawdata_mode_t;

typedef struct rawdata {
    gm_rect_t region;  ///< The Region based on capture output resolution (ex. h264e_attr.dim.width)
    char *yuv_buf;
    unsigned int yuv_buf_len;
    gm_rawdata_mode_t rawdata_mode;  ///< GM_PARTIAL_FROM_CAP_OBJ:0xFBFB9933, GM_WHOLE_FROM_ENC_OBJ:0xFBFC9934
    int reserved[4];
} region_rawdata_t;

/***********************************
 *        Clear Window Structure
 ***********************************/
typedef enum {
    GM_ACTIVE_BY_APPLY,
    GM_ACTIVE_IMMEDIATELY,
} gm_clear_window_mode_t;

typedef enum {
    GM_FMT_YUV422,
} gm_clear_window_yuv_t;

typedef struct {
    int in_w;
    int in_h;
    gm_clear_window_yuv_t in_fmt;
    unsigned char *buf;
    int out_x;
    int out_y;
    int out_w;
    int out_h;
    gm_clear_window_mode_t  mode;
    int reserved[5];
} gm_clear_window_t;


/***********************************
 *        Decode Keyframe Structure
 ***********************************/
typedef struct decode_keyframe {
    int  bs_width;   ///< bitstream width
    int  bs_height;  ///< bitstream height
    char *bs_buf;
    int  bs_buf_len;

    int  rgb_width;   ///< Support RGB565 Only
    int  rgb_height;
    char *rgb_buf;
    int  rgb_buf_len;
    int reserved[5];
} decode_keyframe_t;


/***********************************
 * gmlib internal use (Lib库内部数据，请不要使用)
 ***********************************/
int gm_init_private(int version);  ///< Library internal use, don't use it!
void gm_init_attr(void *name, const char *struct_str, int struct_size, int version); ///< Library internal use, don't use it!


/***********************************
 *       gmlib API
 ***********************************/

/*!
 * @fn int gm_init(void)
 * @brief Init gmlib
 * @return 0 on success(成功), negative value on error(失败)
 */
static inline int gm_init(void) { return (gm_init_private(GM_VERSION_CODE)); }

/*!
 * @fn int gm_release(void)
 * @brief Release gmlib
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_release(void);



/*!
 * @fn int gm_get_sysinfo(gm_system_t *system_info)
 * @brief Get system information
 * @param system_info Input pointer, application must prepare this memory.
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_get_sysinfo(gm_system_t *system_info);

/*!
 * @fn void *gm_new_obj(gm_obj_type_t obj_type)
 * @brief Create a new object by obj_type
 * @param obj_type Type of object
 * @return object pointer
*/
void *gm_new_obj(gm_obj_type_t obj_type);

/*!
 * @fn int gm_delete_obj(void *obj)
 * @brief Delete a exist object
 * @param obj Pointer of object
 * @return 0 on success(成功), negative value on error(失败)
*/
int gm_delete_obj(void *obj);

/*!
 * @fn void *gm_bind(void *groupfd, void *in_obj, void *out_obj)
 * @brief Bind in object to out object to groupfd
 * @param groupfd The fd of group
 * @param in_obj Input object pointer
 * @param out_obj Out object pointer
 * @return The fd of binding result
 */
void *gm_bind(void *groupfd, void *in_obj, void *out_obj);

/*!
 * @fn int gm_unbind(void *bindfd)
 * @brief Unbind the bindfd
 * @param bindfd The fd of gm_bind
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_unbind(void *bindfd);

/*!
 * @fn void *gm_new_groupfd(void)
 * @brief Create a new group
 * @return The fd of group
 */
void *gm_new_groupfd(void);

/*!
 * @fn void gm_delete_groupfd(void *groupfd)
 * @brief Delete a group by groupfd
 * @param groupfd The fd of group
 */
int gm_delete_groupfd(void *groupfd);

/*!
 * @fn int gm_apply_attr(void *bindfd, void *attr)
 * @brief dynamicly update attribute for the bindfd. 
 * @param bindfd The return fd of gm_bind()
 * @param attr Attribute pointer
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_apply_attr(void *bindfd, void *attr);

/*!
 * @fn int gm_apply(void *groupfd)
 * @brief Apply a groupfd
 * @param groupfd The fd of group
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_apply(void *groupfd);

/*!
 * @fn void DECLARE_ATTR(var, structure)
 * @brief Declare a attribyte
 * @param var AP definied variable
 * @param structure Structure of attribute
 */
#define DECLARE_ATTR(var, structure) \
    structure var = ({gm_init_attr(&var, #structure, sizeof(structure), GM_VERSION_CODE); var;})

/*!
 * @fn int gm_set_attr(void *obj, void *attr)
 * @brief Set object's attribute for object
 * @param obj Object pointer
 * @param attr Attribute pointer
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_set_attr(void *obj, void *attr);


/*!
 * @fn int gm_poll(gm_pollfd_t *poll_fds, int num_fds, int timeout_millisec)
 * @brief Blocking to poll a bitstream
 * @param poll_fds The poll fd sets
 * @param num_fds Indicate number of fd sets
 * @param timeout_millisec Timeout value (millisec)
 * @return 0 on success(成功), -4 on timeout(超时), -1 on error(失败)
 */
int gm_poll(gm_pollfd_t *poll_fds, int num_fds, int timeout_millisec);


/*!
 * @fn int gm_send_multi_bitstreams(gm_dec_multi_bitstream_t *multi_bs, int num_bs, int timeout_millisec)
 * @brief Send multiple bitstream to playback
 * @param multi_bs the Structure of decode bitstream
 * @param num_bs Indicate number of bs sets
 * @param timeout_millisec Timeout value to send bitstream (millisecond)
 * @return 0 on success(成功), -4 on timeout(超时), -1 on error(失败)
 */
int gm_send_multi_bitstreams(gm_dec_multi_bitstream_t *multi_bs, int num_bs, int timeout_millisec);

/*!
 * @fn int gm_recv_multi_bitstreams(gm_enc_multi_bitstream_t *multi_bs, int num_bs)
 * @brief Get multiple encode bitstream
 * @param multi_bs the Structure of decode bitstream
 * @param num_bs Indicate number of bs sets
 * @return 0 on success(成功), -1 on error(失败), -2 on bs_buf too small, -3 on mv_buf too small, -4 on timeout(超时)
 */
int gm_recv_multi_bitstreams(gm_enc_multi_bitstream_t *multi_bs, int num_bs);

/*!
 * @fn gm_request_keyframe(void *bindfd)
 * @brief Request a keyframe
 * @param bindfd The return fd of gm_bind()
 * @return 0 on success(成功), negative value on error(失败)
 */
int gm_request_keyframe(void *bindfd);

/*!
 * @fn int gm_request_snapshot(snapshot_t *snapshot, int timeout_millisec);
 * @brief Request a snapshot JPEG
 * @param snapshot The snapshot_t structure of snapshot
 * @param timeout_millisec Timeout value to snapshot a JPEG
 * @return positive value on success(JPEG length 成功), -1 on error(失败), -2 on buf too small, -4 on timeout(超时)
 */
int gm_request_snapshot(snapshot_t *snapshot, int timeout_millisec);

/*!
 * @fn int gm_request_snapshot(disp_snapshot_t *disp_snapshot, int timeout_millisec);
 * @brief Request a snapshot JPEG from LCD
 * @param disp_snapshot The disp_snapshot_t structure of snapshot from LCD
 * @param timeout_millisec Timeout value to snapshot a JPEG
 * @return positive value on success(bitstream length 成功), -1 on error(失败), -2 on buf too small, -4 on timeout(超时)
 */
int gm_request_disp_snapshot(disp_snapshot_t *disp_snapshot, int timeout_millisec);

/*!
 * @fn int gm_set_palette_table(gm_palette_table_t *palette)
 * @brief Set global palette table
 * @param palette Fill color with YCrYCb format to this palette table for usage.
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_palette_table(gm_palette_table_t *palette);

/*!
 * @fn int gm_set_osd_font(void *obj, gm_osd_font_t *osd_font)
 * @brief Set OSD font
 * @param obj Object pointer, currently, support only GM_CAP_OBJECT
 * @param osd_font The gm_osd_font_t structure of OSD setup
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_osd_font(void *obj, gm_osd_font_t *osd_font);

/*!
 * @fn int gm_set_osd_font2(void *obj, gm_osd_font2_t *osd_font)
 * @brief Set OSD font with string more than 31 bytes
 * @param obj Object pointer, currently, support only GM_CAP_OBJECT
 * @param osd_font The gm_osd_font_t structure of OSD setup
 * @return 0 on success(成功), -1 on error(失败) 
 */
int gm_set_osd_font2(void *obj, gm_osd_font2_t *osd_font);

/*!
 * @fn int gm_set_osd_mask(void *obj, gm_osd_mask_t *osd_mask, gm_path_mode_t path_mode)
 * @brief Set OSD mask
 * @param obj Object pointer, currently, support only GM_CAP_OBJECT
 * @param osd_mask The gm_osd_mask_t structure of OSD command
 * @param path_mode Specify the path of osd_mask
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_osd_mask(void *obj, gm_osd_mask_t *osd_mask, gm_path_mode_t path_mode);

/*!
 * @fn int gm_set_osd_mark_image(gm_osd_mark_img_table_t *osd_mark_img)
 * @brief Set OSD mark
 * @param osd_mark_img The table is for loading mark image to system.
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_osd_mark_image(gm_osd_mark_img_table_t *osd_mark_img);

/*!
 * @fn int gm_set_osd_mark(void *obj, gm_osd_mark_t *osd_mark)
 * @brief Set OSD mark
 * @param obj Object pointer, currently, support only GM_CAP_OBJECT
 * @param osd_mark The gm_osd_mark_t structure of OSD command
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_osd_mark(void *obj, gm_osd_mark_t *osd_mark);

/*!
 * @fn int gm_clear_window(int lcd_vch, gm_clear_window_t *cw_str)
 * @brief Trigger the clear window to display
 * @param lcd_vch The vch of LCD display (GM_LCD0, GM_LCD1...)
 * @param cw_str Clear window command structure
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_clear_window(int lcd_vch, gm_clear_window_t *cw_str);

/*!
 * @fn int gm_clear_bitstream(int lcd_vch, gm_clear_window_t *cw_str)
 * @brief Trigger the clear bitstream for playback
 * @param obj Object pointer of GM_FILE_OBJECT
 * @param cw_str Clear window command structure
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_clear_bitstream(void *obj, gm_clear_window_t *cw_str);

/*!
 * @fn int gm_adjust_disp(int lcd_vch, int x, int y, int width, int height)
 * @brief Set the display vision (such as CVBS)
 * @param lcd_vch The vch of LCD display (GM_LCD0, GM_LCD1...)
 * @param x The x coordinate of lcd_vch areas, 4 bytes alignment
 * @param y The y coordinate of lcd_vch areas, 2 bytes alignment
 * @param width The adjusted width of lcd_vch areas, 4 bytes alignment
 * @param height The adjusted height of lcd_vch areas, 2 bytes alignment
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_adjust_disp(int lcd_vch, int x, int y, int width, int height);

/*!
 * @fn int gm_set_cap_flip(int cap_vch, gm_cap_flip_t *cap_flip)
 * @brief Set h-flip and v-flip to capture
 * @param cap_vch The vch of capture
 * @param cap_flip Set the mode of cap flip
 * @return 0 on success(成功), -1 on error(失败)
 */
int gm_set_cap_flip(int cap_vch, gm_cap_flip_t *cap_flip);

/*!
 * @fn int gm_recv_multi_cap_md(gm_multi_cap_md_t *multi_cap_md, int num_cap_md)
 * @brief Get motion detection data (Multi)
 * @param multi_cap_md The array pointer specified the multi cap_md buffer and get it.
 * @param num_cap_md The number of multi cap_md.
 * @return 0 on success(成功), -1 on error(失败), -2 on buffer too small
 */
int gm_recv_multi_cap_md(gm_multi_cap_md_t *multi_cap_md, int num_cap_md);

/*!
 * @fn int gm_decode_keyframe(decode_keyframe_t *dec)
 * @brief Decode the keyframe into yuv buffer
 * @param dec The decode_keyframe_t structure of decoding keyframe
 * @return 0 on success(成功), -1 on error(失败), -2 on buffer too small
 */
int gm_decode_keyframe(decode_keyframe_t *dec);

/*!
 * @fn int gm_get_rawdata(void *obj, gm_rect_t region)
 * @brief Get capture raw data of region
 * @param bindfd the bind fd of gm_bind() return
 * @param region_rawdata The raw data of region
 * @param timeout_millisec Timeout value (millisec)
 * @return positive value on success(成功), -1 on error(失败), -2 on buffer too small, -4 on timeout(超时)
 */
int gm_get_rawdata(void *bindfd, region_rawdata_t *region_rawdata, int timeout_millisec);

/*!
 * @fn int gm_update_new_font(gm_osd_font_update_t new_font)
 * @brief update new font with specified index
 * @param gm_osd_font_update_t(font_idx) font index
 * @param gm_osd_font_update_t(bitmap)  font bitmap
 * @return positive value on success(成功), -1 on error(失败)
 */
int gm_update_new_font(gm_osd_font_update_t *new_font);


/***********************************
 *        Notifications
 ***********************************/
typedef enum {
    GM_NOTIFY_SIGNAL_LOSS = 0,
    GM_NOTIFY_SIGNAL_PRESENT,
    GM_NOTIFY_FRAMERATE_CHANGE,
    GM_NOTIFY_HW_CONFIG_CHANGE,
    GM_NOTIFY_TAMPER_ALARM,
    GM_NOTIFY_TAMPER_ALARM_RELEASE,
    GM_NOTIFY_AUDIO_BUFFER_UNDERRUN,
    GM_NOTIFY_AUDIO_BUFFER_OVERRUN,
    MAX_GM_NOTIFY_COUNT,
} gm_notify_t;

/*!
 * @fn int gm_notify_handler_t(gm_obj_type_t obj_type, gm_notify_t notify, int vch)
 * @brief callback function for notify
 * @param obj_type the object which sends the notification
 * @param vch the channel that occurs the notification
 * @param notify the type of this notification
 * @return return none
 */
typedef void (*gm_notify_handler_t)(gm_obj_type_t obj_type, int vch, gm_notify_t notify);

/*!
 * @fn int gm_register_notify_handler(gm_notify_t notify, gm_notify_handler_t fn_notify_handler)
 * @brief use this to register the notification that you want to receive
 * @param notify the type of this notification
 * @param fn_notify_handler callback function for notify, 0 means deregister
 * @return positive value on success, -1 on error
 */
int gm_register_notify_handler(gm_notify_t notify, gm_notify_handler_t fn_notify_handler);

/*!
 * @fn int gm_set_notify_sig(int sig)
 * @brief use this to set the notify signal number
 * @param sig: notify signal number
 * @return positive value on success, -1 on error
 */
int gm_set_notify_sig(int sig);

/*!
 * @fn int gm_set_cap_tamper(int cap_vch, gm_cap_tamper_t *cap_tamper)
 * @brief Set tamper_sensitive_b and tamper_threshold and tamper_sensitive_h to capture
 * @param cap_vch The vch of capture
 * @param cap_tamper Set parameter for capture tamper detection
 * @return 0 on success(成功), -1 on error(失败) 
 */
int gm_set_cap_tamper(int cap_vch, gm_cap_tamper_t *cap_tamper);

/*!
 * @fn int gm_set_cap_motion(int cap_vch, gm_cap_motion_t *cap_motion)
 * @brief Set motion parameter to capture
 * @param cap_vch The vch of capture
 * @param cap_motion Set parameter for capture motion detection
 * @return 0 on success(成功), -1 on error(失败) 
 */
int gm_set_cap_motion(int cap_vch, gm_cap_motion_t *cap_motion);



#ifdef __cplusplus
}
#endif

#endif
