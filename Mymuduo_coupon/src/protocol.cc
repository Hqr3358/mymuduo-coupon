#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>

 */
void encodeFrame(std::string& out, const std::string& payload)
{
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    out.append(reinterpret_cast<const char*>(&len), sizeof(len));
    out.append(payload);
}

 */
bool decodeFrame(const std::string& in, size_t& offset, std::string* payload, bool* bad)
{
    *bad = false;
    if (in.size() - offset < sizeof(uint32_t))
    {
        return false; // 长度头还没收齐
    }

    uint32_t len = 0;
    std::memcpy(&len, in.data() + offset, sizeof(len));
    len = ntohl(len);

    if (len == 0 || len > kMaxFrameSize)
    {
        *bad = true; // 非法长度，协议被破坏
        return false;
    }

    if (in.size() - offset - sizeof(uint32_t) < len)
    {
        return false; // 载荷还没收齐
    }

    payload->assign(in.data() + offset + sizeof(uint32_t), len);
    offset += sizeof(uint32_t) + len;
    return true;
}
