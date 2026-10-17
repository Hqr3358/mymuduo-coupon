#include "couponserver.hpp"
#include "couponservice.hpp"
#include "TcpConnnction.hpp"
#include "log.hpp"

CouponServer::CouponServer(EventLoop* loop, uint16_t port)
    : server_(loop, port), loop_(loop)
{
    server_.SetConntionCallback(std::bind(&CouponServer::onConnection, this, std::placeholders::_1));
    server_.SetMessageCallback(std::bind(&CouponServer::onMessage, this, std::placeholders::_1,
                                         std::placeholders::_2, std::placeholders::_3));
}

void CouponServer::SetBusinessThreadNum(int n)
{
    server_.SetBusinessThreadNum(n);
}

void CouponServer::SetThreadnum(int n)
{
    server_.SetThreadnum(n);
}

void CouponServer::start()
{
    CouponService* svc = CouponService::init();
    if (!svc->ready())
    {
        Logmanage(ERR, "CouponService not ready (%s), exit",
                  CouponService::init()->error().c_str());
        exit(1);
    }
    // 优化：服务就绪后启动定时清理线程（过期券下架 + Redis 缓存清理 + 分表状态标记）
    svc->startCleaner();
    server_.start();
}

/**
 * @brief 【功能】连接建立/断开回调：维护每连接的粘包解码器（codec）
 * @details 连接建立时为该连接创建 Codec（feed 式解码），断开时清理；
 *          业务回调在业务线程池线程执行，conn 以 shared_ptr 拷贝持有，
 *          生命周期安全（对应日志 conn established/closed）。
 */
void CouponServer::onConnection(const TcpConnctionptr& conn)
{
    // 连接回调在 IO 线程执行（conntEstablish / handleClose）
    std::lock_guard<std::mutex> lock(mtx_);
    if (conn->conncted())
    {
        auto codec = std::make_shared<Codec>(
            [this, conn](const std::string& payload)
            {
                // 注意：这里在业务线程池线程执行，conn 是 shared_ptr 拷贝，生命周期安全
                try
                {
                    json js = json::parse(payload);
                    CouponService::init()->dispatch(conn, js);
                }
                catch (const std::exception& e)
                {
                    Logmanage(ERR, "json parse failed: %s", e.what());
                }
            });
        codecs_[conn->name()] = codec;
        Logmanage(NORMOL, "conn established: %s", conn->name().c_str());
    }
    else
    {
        auto it = codecs_.find(conn->name());
        if (it != codecs_.end())
        {
            it->second->reset();
            codecs_.erase(it);
        }
        Logmanage(NORMOL, "conn closed: %s", conn->name().c_str());
    }
}

/**
 * @brief 【功能】消息回调：把原始字节喂给粘包解码器，解码后分发到业务层
 * @details 该回调在业务线程池线程执行（TcpServer 分发），buf 是本次 read
 *          的独立拷贝；Codec 完成帧拆分 + JSON 解析后调用
 *          CouponService::dispatch（msgid 注册表路由）。
 */
void CouponServer::onMessage(const TcpConnctionptr& conn, Buffer* buf, TimeStamp /*ts*/)
{
    // 消息回调在业务线程池线程执行，buf 是本次 read 数据的独立拷贝
    std::string data = buf->retrieveAllAsString();
    if (data.empty()) return;

    std::shared_ptr<Codec> codec;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = codecs_.find(conn->name());
        if (it == codecs_.end()) return; // 连接刚断开，丢弃
        codec = it->second;
    }

    codec->feed(data.data(), data.size());
}
