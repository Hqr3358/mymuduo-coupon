#include "codec.hpp"
#include "protocol.hpp"
#include <vector>

/**
 * @brief 构造编解码器：绑定完整包回调（业务处理入口）
 */
Codec::Codec(MessageCallback cb)
    : onMessage_(std::move(cb))
{
}

/**
 * @brief 累积解码：锁内切包（只做内存操作），锁外逐个回调
 * @details 对应功能：连接级分包解码器，处理粘包/半包；
 *          协议破坏时清空缓冲丢弃该连接剩余数据。
 */
void Codec::feed(const char* data, size_t len)
{
    // 1) 加锁累积数据并切包（只做内存操作，不调用用户回调）
    std::vector<std::string> frames;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        inbuf_.append(data, len);

        size_t offset = 0;
        while (true)
        {
            std::string payload;
            bool bad = false;
            if (!decodeFrame(inbuf_, offset, &payload, &bad))
            {
                if (bad)
                {
                    // 协议被破坏：清空缓冲，丢弃该连接剩余数据
                    inbuf_.clear();
                    break;
                }
                break; // 半包，等下一段数据
            }
            frames.emplace_back(std::move(payload));
        }

        // 已解析部分移出累积缓冲（保留未解析的半包）
        if (offset > 0)
        {
            inbuf_.erase(0, offset);
        }
    }

    // 2) 锁外逐个回调，避免持锁执行业务逻辑
    for (auto& frame : frames)
    {
        onMessage_(frame);
    }
}

/**
 * @brief 重置解码器：清空累积缓冲（连接关闭/协议重置时调用）
 */
void Codec::reset()
{
    std::lock_guard<std::mutex> lock(mtx_);
    inbuf_.clear();
}
