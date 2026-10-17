-- =====================================================================
-- 领券原子脚本（Redis Lua）
--   发券/领券/核销的核心一致性：防超发 + 防重复领取
--
-- 为什么用 Lua：
--   Redis 单线程执行脚本，脚本内的 GET/比较/DECR/SADD 全程无并发竞态，
--   天然原子。比"DECR 后检查 <0 再 INCR 回滚"的非原子方案更严谨，
--   也比 WATCH/MULTI/EXEC 乐观锁方案在争用下更高效。
--
-- KEYS[1] = coupon:stock:{couponId}     剩余库存
-- KEYS[2] = coupon:claimed:{couponId}   已领取用户集合
-- ARGV[1] = userId
--
-- 返回：
--   >=0  剩余库存（领取成功：已 DECR 库存、已 SADD 用户）
--   -1   库存不足
--   -2   该用户已领取（重复）
--   -3   券不存在（库存 key 未初始化，可能已下架/Redis 数据丢失）
-- =====================================================================
local stock = redis.call('GET', KEYS[1])
if not stock then
    return -3
end
stock = tonumber(stock)
if stock <= 0 then
    return -1
end
if redis.call('SISMEMBER', KEYS[2], ARGV[1]) == 1 then
    return -2
end
redis.call('DECR', KEYS[1])
redis.call('SADD', KEYS[2], ARGV[1])
return stock - 1
