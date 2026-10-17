#pragma once
#include <cstdint>
#include <string>


// 消息类型（客户端 -> 服务端请求）
enum MsgId
{
    MSG_PING       = 0, // 心跳
    MSG_ISSUE      = 1, // 发券（管理端）：{msgid, title, total, start_time?, end_time?}
    MSG_CLAIM      = 2, // 领券（用户端）：{msgid, coupon_id, user_id}
    MSG_USE        = 3, // 核销：{msgid, user_id, coupon_code}
    MSG_QUERY_MINE = 4, // 查询我的券：{msgid, user_id}
    MSG_QUERY      = 5, // 查询券模板（含实时库存）：{msgid, coupon_id}
    MSG_ERROR      = 99
};

// 业务错误码
enum ErrCode
{
    ERR_OK            = 0,  // 成功
    ERR_ARGS          = 1,  // 参数错误
    ERR_NO_STOCK      = 2,  // 库存不足
    ERR_DUP_CLAIM     = 3,  // 重复领取（同一用户已领过该券）
    ERR_NOT_EXIST     = 4,  // 券不存在 / 已下架 / 已过期
    ERR_INVALID       = 5,  // 券码无效（不属于该用户）
    ERR_ALREADY_USED  = 6,  // 券已核销（幂等返回）
    ERR_DB            = 7,  // 数据库错误
    ERR_SYSTEM        = 8,  // 系统繁忙（业务线程池限流 / 内部错误）
    ERR_REDIS         = 9   // Redis 不可用
};

static constexpr uint32_t kMaxFrameSize = 1 << 20; // 单包最大 1MB

void encodeFrame(std::string& out, const std::string& payload);

bool decodeFrame(const std::string& in, size_t& offset, std::string* payload, bool* bad);
