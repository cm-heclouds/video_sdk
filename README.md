#中移物联网有限公司 OneNET C语言SDK, /video_sdk
## 目录
 * 编译SDK
 * 常见问题 Q&A
 
## 编译SDK
### 通过CMAKE编译SDK
1. linux下编译参考tools下的命令
### 本项目引用了mbedtls
   https://github.com/ARMmbed/mbedtls

### 自定义工程中编译SDK
1. 将SDK源码包含到自定义工程中
2. 设置SDK所需的头文件的路径：
   * sdk/include
3. 修改include/ont/config.h 中的配置，以符合自定义工程的需求
4. 编译SDK


## 常见问题 Q&A
1. VIDEO smaple 支持的文件格式是？
SAMPLE 支持MP4格式的远程回放，MP4示例文件的下载地址 http://pan.baidu.com/s/1kVLkxYf  

2. ARM Linux 交叉编译 
参考tools/videosamplebuild-arm.sh
SET(TOOLCHAIN_DIR "/usr/bin/arm-2009q3/arm-2009q3/") 设置正确的交叉编译工具链目录；
如果需要链接SSL库，需要使用交叉编译工具编译对应的SSL版本，本工程使用openssl-1.0.1g，并复制到工具链对应的lib目录。

 
