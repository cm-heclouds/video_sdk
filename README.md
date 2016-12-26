#中移物联网有限公司 OneNET C语言SDK, /video_sdk
## 目录
 * 编译SDK
 * 目录结构
 * 常见问题 Q&A
 
## 编译SDK
### 通过CMAKE编译SDK
用cmake生成调试版本的工程，使用命令：cmake -DCMAKE_BUILD_TYPE=Debug
用cmake生成发布版本的工程，使用命令：cmake -DCMAKE_BUILD_TYPE=Release
例如，编译edp相关内容：cmake -DCMAKE_BUILD_TYPE=Debug -D_BASE=1 -D_EDP=1 -D_SAMPLE=1
      编译mqtt相关内容：cmake -DCMAKE_BUILD_TYPE=Debug -D_MQTT=1 -D_SAMPLE=1 
      编译video相关的内容: cmake -DCMAKE_BUILD_TYPE=Debug -D_BASE=1 -D_MQTT=1 -D_VIDEO=1 -D_MP4V2=1 -D_ONVIF=1 -D_SAMPLE=1 -D_STATIC=1 #包含VIDEO的项目使用STATIC模式编译
### 自定义工程中编译SDK
1. 将SDK源码包含到自定义工程中
2. 设置SDK所需的头文件的路径：
   * sdk/include
   * sdk/src/comm
3. 修改include/ont/config.h 中的配置，以符合自定义工程的需求
4. 编译SDK


## 目录结构
```
sdk
 + doc               SDK相关文档
 + include           SDK API相关头文件
    + ont            SDK通用API、错误码等
    + edp            EDP协议特有API
    + mqtt           MQTT协议特有API
 + platforms         平台相关的接口（include/platform.h）实现
    + posix          支持Posix系统的平台相关接口实现
    + win            Windows系统的平台相关接口实现  
 + sample            各协议的示例
    + edp            EDP协议的示例
    + mqtt           MQTT协议的示例
 + src               SDK内部实现
   ...

## 常见问题 Q&A
1. 如何设置SDK内存使用量？
可以通过设置include/ont/config.h 中的ONT_SOCK_SEND_BUF_SIZE和ONT_SOCK_RECV_BUF_SIZE
来更改SDK在运行时的内存使用量。这两个值应根据实际需求计算，确保其值大于可能最大
的数据发送（接收）量。
2. 为什么SDK在某些芯片下运行正常，但换成另一款芯片后，程序运行异常？
可能的问题有：
*该芯片的某些操作有内存对齐要求，检查使用的数据结构是否按要求进行内存对齐
3. VIDEO smaple 支持的文件格式是什么？
SAMPLE 支持MP4格式的远程回放，示例文件的下载地址 http://pan.baidu.com/s/1kVLkxYf
