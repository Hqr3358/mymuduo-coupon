#pragma once
#include <functional>
#include <mutex>
#include <string>

/** @brief 连接级粘包解码器：4 字节长度头 + JSON 载荷，锁内切包后回调业务 */
class Codec
{
public:
    using MessageCallback = std::function<void(const std::string& payload)>;

    // 构造解码器：绑定完整包回调
    explicit Codec(MessageCallback cb);

    // 喂入数据：锁内切包，完整包回调业务
    void feed(const char* data, size_t len);

    // 重置解码器：清空缓冲
    void reset();

private:
    mutable std::mutex mtx_;
    std::string inbuf_;
    MessageCallback onMessage_;
};
