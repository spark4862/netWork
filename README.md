# TCP, 网络层接口，路由器转发功能实现

+ 为什么要做这个项目
  + 加深对TCP网络协议的理解
  + 训练编程能力

+ tcp组成：
  + tcpReceiver
    + 发送确认报文，设置windowsize来进行流量控制
    + 存放失序segment的部分，即字节重组结构，测试速度达到9.61 Gbit/s
    + 存放可以被应用程序直接读取的segment的部分，即可靠字节流，提供应用层读取接口，测试速度达到13.75 Gbit/s
    + 对于收到的报文，需要做seqno到stream index的转换
  + Sender
    + 计时器逻辑，重传逻辑的实现
+ 网络层到链路层接口的实现
+ 路由器转发功能实现
  + 由转发表，带缓存异步接口组成

+ 如何运行
  + To set up the build system: `cmake -S . -B build`
  + To compile: `cmake --build build`
  + To run tests: `cmake --build build --target test`
  + To run speed benchmarks: `cmake --build build --target speed`
  + To format code: `cmake --build build --target format`
