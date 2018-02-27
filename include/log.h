#ifndef ONT_INCLUDE_ONT_LOG_H_
#define ONT_INCLUDE_ONT_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

/**
 * 日志等级
 */
typedef enum ont_log_level_t {
    ONTLL_DEBUG,        /**< 调试信息， 最低 */
    ONTLL_INFO,         /**< 普通信息 */
    ONTLL_WARNING,      /**< 告警信息 */
    ONTLL_ERROR,        /**< 错误信息 */
    ONTLL_FATAL         /**< 致命错误信息，最高 */
} ont_log_level_t;

typedef void (*ont_log_func)(void *ctx, ont_log_level_t ll , const char *format, ...);

typedef struct ont_logger_t {
    ont_log_level_t ll;
    void *context;
    ont_log_func log;
} ont_logger_t;


extern ont_logger_t ont_logger;

/**
 * 设置日志打印函数
 */
#define ont_log_set_logger(ctx, log_func) \
    do {ont_logger.context = ctx; ont_logger.log = log_func; } while(0)

/**
 * 设置日志过滤等级，低于\a ll 的日志不被打印
 */
#define ont_log_set_log_level(level) \
    do {ont_logger.ll = level; } while(0)

#define ONT_LOG_BEGIN(level)                                            \
    do {                                                                \
        const ont_log_level_t ll = level;                               \
        if (ont_logger.ll <= ll) {                                      \
            void *ctx = ont_logger.context

#define ONT_LOG_END() }} while(0)

#define ONT_LOG0(level, format)                 \
    ONT_LOG_BEGIN(level);                       \
    ont_logger.log(ctx, ll, format);            \
    ONT_LOG_END()

#define ONT_LOG1(level, format, arg1)           \
    ONT_LOG_BEGIN(level);                       \
    ont_logger.log(ctx, ll, format, arg1);      \
    ONT_LOG_END()


#define ONT_LOG2(level, format, arg1, arg2)          \
    ONT_LOG_BEGIN(level);                            \
    ont_logger.log(ctx, ll, format, arg1, arg2);     \
    ONT_LOG_END()

#define ONT_LOG3(level, format, arg1, arg2, arg3)         \
    ONT_LOG_BEGIN(level);                                 \
    ont_logger.log(ctx, ll, format, arg1, arg2, arg3);    \
    ONT_LOG_END()

#define ONT_LOG4(level, format, arg1, arg2, arg3, arg4)       \
    ONT_LOG_BEGIN(level);                                     \
    ont_logger.log(ctx, ll, format, arg1, arg2, arg3, arg4);  \
    ONT_LOG_END()

#define ONT_LOG5(level, format, arg1, arg2, arg3, arg4, arg5)        \
    ONT_LOG_BEGIN(level);                                            \
    ont_logger.log(ctx, ll, format, arg1, arg2, arg3, arg4, arg5);   \
    ONT_LOG_END()

#define ONT_LOG6(level, format, arg1, arg2, arg3, arg4, arg5, arg6)      \
    ONT_LOG_BEGIN(level);                                                \
    ont_logger.log(ctx, ll, format, arg1, arg2, arg3, arg4, arg5, arg6); \
    ONT_LOG_END()



#ifdef ONT_DEBUG

#define ONT_LOG_DEBUG0(format) ONT_LOG2(ONTLL_DEBUG, "%s:%d: "format, __FILE__, __LINE__)
#define ONT_LOG_DEBUG1(format, arg1) ONT_LOG3(ONTLL_DEBUG, "%s:%d: "format, __FILE__, __LINE__, arg1)
#define ONT_LOG_DEBUG2(format, arg1, arg2) ONT_LOG4(ONTLL_DEBUG, "%s:%d: "format, __FILE__, __LINE__, arg1, arg2)
#define ONT_LOG_DEBUG3(format, arg1, arg2, arg3) ONT_LOG5(ONTLL_DEBUG, "%s:%d: "format, __FILE__, __LINE__, arg1, arg2, arg3)
#define ONT_LOG_DEBUG4(format, arg1, arg2, arg3, arg4) ONT_LOG6(ONTLL_DEBUG, "%s:%d: "format, __FILE__, __LINE__, arg1, arg2, arg3, arg4)


#else /* !defined(ONT_DEBUG) */

#define ONT_LOG_DEBUG0(format)
#define ONT_LOG_DEBUG1(format, arg1)
#define ONT_LOG_DEBUG2(format, arg1, arg2)
#define ONT_LOG_DEBUG3(format, arg1, arg2, arg3)
#define ONT_LOG_DEBUG4(format, arg1, arg2, arg3, arg4)

#endif /* ONT_DEBUG */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ONT_INCLUDE_ONT_LOG_H_ */
