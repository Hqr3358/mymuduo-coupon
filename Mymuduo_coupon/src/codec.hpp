#pragma once
#include <functional>
#include <mutex>
#include <string>

 */
class Codec
{
public:
    using MessageCallback = std::function<void(const std::string& payload)>;

    explicit Codec(MessageCallback cb);

     */
    void feed(const char* data, size_t len);

     */
    void reset();

private:
    mutable std::mutex mtx_;
    std::string inbuf_;
    MessageCallback onMessage_;
};
