#pragma once
#include <functional>
#include <mutex>
#include <string>

/** @brief 连接级粘包解码器：4 字节长度头 + JSON 载荷，锁内切包后回调业务 */
class Codec
{
public:
    using MessageCallback = std::function<void(const std::string& payload)>;

    explicit Codec(MessageCallback cb);

    void feed(const char* data, size_t len);

    void reset();

private:
    mutable std::mutex mtx_;
    std::string inbuf_;
    MessageCallback onMessage_;
};
