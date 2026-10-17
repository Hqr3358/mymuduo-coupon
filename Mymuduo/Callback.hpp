/** @brief 回调类型定义：连接/关闭/消息回调 + TcpConnctionptr（网络库各组件间解耦） */
#pragma once
//#include "TcpConnnction.hpp"
#include<functional>
#include <memory>
class TimeStamp;
class Buffer;
class TcpConnction;
using TcpConnctionptr=std::shared_ptr<TcpConnction>;
using ConntionCallback=std::function<void(const TcpConnctionptr&)>;
using CloseCallback=std::function<void(const TcpConnctionptr&)>;
using MessageCallback=std::function<void(const TcpConnctionptr&,Buffer*,TimeStamp)>;

