/**
 * @file client.cc
 * @brief 优惠券系统 压测/演示客户端（纯 POSIX socket，不依赖网络库）
 *
 * 用法：
 *   couponclient ping
 *   couponclient issue <title> <total>              # 发券
 *   couponclient claim <coupon_id> <user_id>        # 领券
 *   couponclient use <user_id> <coupon_code>        # 核销
 *   couponclient mine <user_id>                     # 查询我的券
 *   couponclient info <coupon_id>                   # 查询券模板+实时库存
 *   couponclient stress <coupon_id> <users> <threads>   # 并发领券压测（防超发验证）
 *   couponclient dup <coupon_id> <user_id> <times>  # 同一用户并发重复领取（去重验证）
 *   couponclient stress-mix <coupon_id> <users> <threads>  # 并发领券+随机核销
 * 通用参数 [host] [port]，默认 127.0.0.1:9081
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

enum MsgId
{
    MSG_PING = 0,
    MSG_ISSUE = 1,
    MSG_CLAIM = 2,
    MSG_USE = 3,
    MSG_QUERY_MINE = 4,
    MSG_QUERY = 5,
};

// ---- 封包/解包（与服务端协议一致：4字节大端长度头 + JSON）----
static void encodeFrame(std::string& out, const std::string& payload)
{
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    out.append(reinterpret_cast<const char*>(&len), sizeof(len));
    out.append(payload);
}

static int connectTcp(const std::string& host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        close(fd);
        return -1;
    }
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

static void sendAll(int fd, const std::string& data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
        {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            throw std::runtime_error("send failed");
        }
        sent += static_cast<size_t>(n);
    }
}

static json readResponse(int fd)
{
    uint32_t hdr = 0;
    size_t got = 0;
    while (got < sizeof(hdr))
    {
        ssize_t n = ::recv(fd, reinterpret_cast<char*>(&hdr) + got, sizeof(hdr) - got, 0);
        if (n <= 0)
        {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            throw std::runtime_error("recv header failed");
        }
        got += static_cast<size_t>(n);
    }
    uint32_t len = ntohl(hdr);
    if (len == 0 || len > (1u << 20))
    {
        throw std::runtime_error("bad frame length");
    }
    std::string body(len, '\0');
    got = 0;
    while (got < len)
    {
        ssize_t n = ::recv(fd, &body[got], len - got, 0);
        if (n <= 0)
        {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            throw std::runtime_error("recv body failed");
        }
        got += static_cast<size_t>(n);
    }
    return json::parse(body);
}

static json request(const std::string& host, int port, const json& req)
{
    int fd = connectTcp(host, port);
    if (fd < 0) throw std::runtime_error("connect failed");
    std::string out;
    encodeFrame(out, req.dump());
    try
    {
        sendAll(fd, out);
        json resp = readResponse(fd);
        close(fd);
        return resp;
    }
    catch (...)
    {
        close(fd);
        throw;
    }
}

// =====================================================================
// 压测统计结构 + 各压测 worker（对应 README 的 stress / dup / stress-mix）
// =====================================================================
/**
 * @brief 【功能】压测结果统计：按 errno 分类计数
 * @details ok=成功(0)，noStock=无库存(2)，dup=重复领取(3)，
 *          other=其他错误，connFail=连接失败——用于防超发/去重断言。
 */
struct StressStat
{
    std::atomic<long> ok{0};
    std::atomic<long> noStock{0};
    std::atomic<long> dup{0};
    std::atomic<long> other{0};
    std::atomic<long> connFail{0};
};

/**
 * @brief 【功能】单次领券请求 + 统计（claimWorker/dupWorker 共用，消除重复代码）
 * @details 每个用户独立 TCP 连接发起 MSG_CLAIM，按 errno 归类到 StressStat。
 */
static void claimOnce(const std::string& host, int port, int64_t couponId,
                      int64_t userId, StressStat* st)
{
    int fd = connectTcp(host, port);
    if (fd < 0)
    {
        st->connFail++;
        return;
    }
    json req = {{"msgid", MSG_CLAIM}, {"coupon_id", couponId}, {"user_id", userId}};
    std::string out;
    encodeFrame(out, req.dump());
    try
    {
        sendAll(fd, out);
        json resp = readResponse(fd);
        int err = resp.value("errno", -1);
        if (err == 0) st->ok++;
        else if (err == 2) st->noStock++;
        else if (err == 3) st->dup++;
        else st->other++;
    }
    catch (...)
    {
        st->other++;
    }
    close(fd);
}

/**
 * @brief 【功能】并发领券压测 worker（couponclient stress）
 * @details 对应功能测试第 2 项「防超发」：beginUid..endUid 每个用户领一次，
 *          多线程并发；期望结果 = 成功数等于库存、其余全部 noStock、dup=0。
 */
static void claimWorker(const std::string& host, int port, int64_t couponId,
                        int beginUid, int endUid, StressStat* st)
{
    for (int uid = beginUid; uid < endUid; ++uid)
    {
        claimOnce(host, port, couponId, uid, st);
    }
}

/**
 * @brief 【功能】同一用户并发重复领取压测 worker（couponclient dup）
 * @details 对应功能测试第 3 项「领取去重」：同一 userId 并发领 times 次，
 *          期望结果 = 成功 1 / 重复领取 times-1（Redis Lua SADD 去重）。
 */
static void dupWorker(const std::string& host, int port, int64_t couponId, int64_t userId,
                      int times, StressStat* st)
{
    for (int i = 0; i < times; ++i)
    {
        claimOnce(host, port, couponId, userId, st);
    }
}

/**
 * @brief 【功能】并发领券 + 成功后随机双核销压测 worker（couponclient stress-mix）
 * @details 对应功能测试第 10 项「全链路验证」：领券成功后同一连接连续核销两次，
 *          第一次应成功(errno=0)、第二次应幂等返回已核销(errno=6)；
 *          other>0 即视为核销幂等失效。
 */
static void mixWorker(const std::string& host, int port, int64_t couponId,
                      int beginUid, int endUid, StressStat* st)
{
    for (int uid = beginUid; uid < endUid; ++uid)
    {
        int fd = connectTcp(host, port);
        if (fd < 0)
        {
            st->connFail++;
            continue;
        }
        try
        {
            json req = {{"msgid", MSG_CLAIM}, {"coupon_id", couponId}, {"user_id", uid}};
            std::string out;
            encodeFrame(out, req.dump());
            sendAll(fd, out);
            json resp = readResponse(fd);
            int err = resp.value("errno", -1);
            if (err != 0)
            {
                if (err == 2) st->noStock++;
                else if (err == 3) st->dup++;
                else st->other++;
                close(fd);
                continue;
            }
            st->ok++;
            std::string code = resp["data"]["coupon_code"].get<std::string>();

            // 领到券后核销两次：第一次应成功，第二次应"已核销"（幂等）
            for (int round = 0; round < 2; ++round)
            {
                json useReq = {{"msgid", MSG_USE}, {"user_id", uid}, {"coupon_code", code}};
                std::string out2;
                encodeFrame(out2, useReq.dump());
                sendAll(fd, out2);
                json useResp = readResponse(fd);
                int uerr = useResp.value("errno", -1);
                if (round == 0 && uerr != 0) st->other++; // 首次核销失败记 other
                if (round == 1 && uerr != 6) st->other++; // 二次核销必须幂等返回已核销
            }
        }
        catch (...)
        {
            st->other++;
        }
        close(fd);
    }
}

static void printStat(const StressStat& st, const std::string& tag)
{
    long total = st.ok + st.noStock + st.dup + st.other + st.connFail;
    printf("\n===== %s =====\n", tag.c_str());
    printf("total requests : %ld\n", total);
    printf("success        : %ld\n", st.ok.load());
    printf("no stock       : %ld\n", st.noStock.load());
    printf("duplicate      : %ld\n", st.dup.load());
    printf("other error    : %ld\n", st.other.load());
    printf("conn fail      : %ld\n", st.connFail.load());
}

// =====================================================================
// main
// =====================================================================
static void usage()
{
    std::cout
        << "usage:\n"
        << "  couponclient ping\n"
        << "  couponclient issue <title> <total>\n"
        << "  couponclient claim <coupon_id> <user_id>\n"
        << "  couponclient use <user_id> <coupon_code>\n"
        << "  couponclient mine <user_id>\n"
        << "  couponclient info <coupon_id>\n"
        << "  couponclient stress <coupon_id> <users> <threads>\n"
        << "  couponclient dup <coupon_id> <user_id> <times>\n"
        << "  couponclient stress-mix <coupon_id> <users> <threads>\n"
        << "  [host] [port] optional, default 127.0.0.1 8081\n";
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage();
        return 1;
    }

    std::string host = "127.0.0.1";
    int port = 9081;
    // 通用尾部参数 [host] [port]
    if (argc >= 3 && std::string(argv[argc - 2]).find('.') != std::string::npos)
    {
        host = argv[argc - 2];
        port = atoi(argv[argc - 1]);
    }

    std::string cmd = argv[1];
    try
    {
        if (cmd == "ping")
        {
            json resp = request(host, port, {{"msgid", MSG_PING}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "issue" && argc >= 4)
        {
            json resp = request(host, port, {{"msgid", MSG_ISSUE},
                                             {"title", argv[2]}, {"total", atoi(argv[3])}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "claim" && argc >= 4)
        {
            json resp = request(host, port, {{"msgid", MSG_CLAIM},
                                             {"coupon_id", atoll(argv[2])},
                                             {"user_id", atoll(argv[3])}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "use" && argc >= 4)
        {
            json resp = request(host, port, {{"msgid", MSG_USE},
                                             {"user_id", atoll(argv[2])},
                                             {"coupon_code", argv[3]}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "mine" && argc >= 3)
        {
            json resp = request(host, port, {{"msgid", MSG_QUERY_MINE},
                                             {"user_id", atoll(argv[2])}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "info" && argc >= 3)
        {
            json resp = request(host, port, {{"msgid", MSG_QUERY},
                                             {"coupon_id", atoll(argv[2])}});
            std::cout << resp.dump(2) << std::endl;
        }
        else if (cmd == "stress" && argc >= 5)
        {
            int64_t couponId = atoll(argv[2]);
            int users = atoi(argv[3]);
            int threads = atoi(argv[4]);
            if (threads < 1) threads = 1;
            if (users < 1) users = 1;

            StressStat st;
            std::vector<std::thread> tvec;
            auto t0 = std::chrono::steady_clock::now();
            int chunk = (users + threads - 1) / threads;
            for (int t = 0; t < threads; ++t)
            {
                int begin = t * chunk + 1;
                int end = std::min(begin + chunk, users + 1);
                if (begin >= end) break;
                tvec.emplace_back(claimWorker, host, port, couponId, begin, end, &st);
            }
            for (auto& th : tvec) th.join();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();

            printStat(st, "stress claim (stock=" + std::to_string(users) + " users)");
            printf("elapsed        : %ld ms\n", ms);
            // 防超发断言：成功数 == min(库存, 用户数)；由于每用户只领一次，成功数应等于
            // 收到的"库存不足"前的成功数。这里给出期望：success + noStock == users 且
            // success <= users。若成功数 == users，说明库存 >= 用户数，未超发。
            printf("note: success should <= users(%d), and each user at most 1 success\n", users);
        }
        else if (cmd == "dup" && argc >= 5)
        {
            int64_t couponId = atoll(argv[2]);
            int64_t userId = atoll(argv[3]);
            int times = atoi(argv[4]);

            StressStat st;
            std::vector<std::thread> tvec;
            auto t0 = std::chrono::steady_clock::now();
            for (int t = 0; t < times; ++t)
            {
                tvec.emplace_back(dupWorker, host, port, couponId, userId, 1, &st);
            }
            for (auto& th : tvec) th.join();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();

            printStat(st, "dup claim (same user x" + std::to_string(times) + ")");
            printf("elapsed        : %ld ms\n", ms);
            printf("expect: success == 1, duplicate == %d\n", times - 1);
        }
        else if (cmd == "stress-mix" && argc >= 5)
        {
            int64_t couponId = atoll(argv[2]);
            int users = atoi(argv[3]);
            int threads = atoi(argv[4]);
            if (threads < 1) threads = 1;
            if (users < 1) users = 1;

            StressStat st;
            std::vector<std::thread> tvec;
            auto t0 = std::chrono::steady_clock::now();
            int chunk = (users + threads - 1) / threads;
            for (int t = 0; t < threads; ++t)
            {
                int begin = t * chunk + 1;
                int end = std::min(begin + chunk, users + 1);
                if (begin >= end) break;
                tvec.emplace_back(mixWorker, host, port, couponId, begin, end, &st);
            }
            for (auto& th : tvec) th.join();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();

            printStat(st, "stress-mix (claim + double-use)");
            printf("elapsed        : %ld ms\n", ms);
            printf("expect: success == issued stock, and no 'other error' means use idempotent\n");
        }
        else
        {
            usage();
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
