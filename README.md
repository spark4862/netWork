如何实现tcp
+ message报文 segment报文段 packet分组 ip datagram数据报 frame帧 bit
+ 为什么要做这个项目
  + 加深对TCP网络协议的理解
  + 训练编程能力

+ tcp组成：
  + tcpReceiver
    + 发送确认报文，设置windowsize来进行流量控制
    + 存放失序segment的部分，难点为不同范围数据的合并
    + 存放可以被应用程序直接读取的segment的部分，提供应用层读取接口，重点为用归属的移动替代拷贝
    + 对于收到的报文，需要做seqno到stream index的转换
  + Sender
    + 计时器逻辑，重传逻辑的实现
+ 网络层到链路层接口的实现
+ 路由器转发功能实现

+ 遇到的困难
  + 如何设计更好的数据结构来提升效率
  + 在streambyte中，需要消除拷贝，实现延迟更改
  + 在查找时，使用set来代替queue，使用emplace_hint实现高效率插入
  + 使用桶和哈希来实现最长前缀匹配

+ 收获
  + tcp详细了解，书本上只有tcp相关的概念，只有实现tcp才能对tcp的细节有一个深入了解
  + 了解了linux发送TCP报文段的几种方法，如使用UDP发送，使用虚拟TUN设备发送。
  + clang-format
  + clang-tidy
  + addressSan


+ 如何运行
  + To set up the build system: `cmake -S . -B build`
  + To compile: `cmake --build build`
  + To run tests: `cmake --build build --target test`
  + To run speed benchmarks: `cmake --build build --target speed`
  + To format code: `cmake --build build --target format`
