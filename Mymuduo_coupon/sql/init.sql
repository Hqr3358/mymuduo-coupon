-- =====================================================================
-- 优惠券系统 数据库初始化脚本（MySQL 8.x / 5.7）
--   1) 建库 coupon_db
--   2) 券模板表 coupon（单表：量级小，写入低频）
--   3) 用户券表 user_coupon_{0..15} 分表（按 user_id % 16 路由）
--      核心约束：
--        UNIQUE KEY uk_user_coupon(user_id, coupon_id)  防同一用户重复领取同一券
--        UNIQUE KEY uk_code(coupon_code)                券码全局唯一
--   4) 创建业务账号 coupon / coupon123
-- 使用：mysql -uroot < init.sql
-- =====================================================================
CREATE DATABASE IF NOT EXISTS coupon_db DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE coupon_db;

DROP TABLE IF EXISTS coupon;
CREATE TABLE coupon (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '券模板ID',
    title      VARCHAR(64)     NOT NULL                COMMENT '券名称',
    total      INT UNSIGNED    NOT NULL                COMMENT '发行总量',
    start_time DATETIME        NOT NULL                COMMENT '生效时间',
    end_time   DATETIME        NOT NULL                COMMENT '过期时间',
    status     TINYINT         NOT NULL DEFAULT 1      COMMENT '1上架 0下架',
    created_at DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_status (status),
    KEY idx_end_time (end_time)
) ENGINE = InnoDB COMMENT '券模板表';

-- 16 张分表：user_coupon_{0..15}
-- 同一用户按 user_id % 16 路由到固定一张表：
--   * 写热点分散到 16 张表，缓解单表锁竞争与 IO 压力
--   * 同一用户的所有券在同一张表，查询"我的券"只查一张表
--   * 唯一索引冲突局域化：冲突只发生在同表内部
CREATE TABLE IF NOT EXISTS user_coupon_0 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 0';

CREATE TABLE IF NOT EXISTS user_coupon_1 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 1';

CREATE TABLE IF NOT EXISTS user_coupon_2 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 2';

CREATE TABLE IF NOT EXISTS user_coupon_3 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 3';

CREATE TABLE IF NOT EXISTS user_coupon_4 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 4';

CREATE TABLE IF NOT EXISTS user_coupon_5 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 5';

CREATE TABLE IF NOT EXISTS user_coupon_6 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 6';

CREATE TABLE IF NOT EXISTS user_coupon_7 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 7';

CREATE TABLE IF NOT EXISTS user_coupon_8 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 8';

CREATE TABLE IF NOT EXISTS user_coupon_9 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 9';

CREATE TABLE IF NOT EXISTS user_coupon_10 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 10';

CREATE TABLE IF NOT EXISTS user_coupon_11 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 11';

CREATE TABLE IF NOT EXISTS user_coupon_12 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 12';

CREATE TABLE IF NOT EXISTS user_coupon_13 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 13';

CREATE TABLE IF NOT EXISTS user_coupon_14 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 14';

CREATE TABLE IF NOT EXISTS user_coupon_15 (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id     BIGINT UNSIGNED NOT NULL COMMENT '用户ID（分表键）',
    coupon_id   BIGINT UNSIGNED NOT NULL COMMENT '券模板ID',
    coupon_code VARCHAR(64)     NOT NULL COMMENT '券码（全局唯一）',
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0未核销 1已核销',
    claimed_at  DATETIME        NOT NULL COMMENT '领取时间',
    used_at     DATETIME        DEFAULT NULL COMMENT '核销时间',
    order_id    VARCHAR(64)     DEFAULT NULL COMMENT '核销单号',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_coupon (user_id, coupon_id),
    UNIQUE KEY uk_code (coupon_code),
    KEY idx_status (status),
    KEY idx_claimed_at (claimed_at)
) ENGINE = InnoDB COMMENT '用户券分表 15';
-- 业务账号（服务端 CouponService 使用：coupon / coupon123）
CREATE USER IF NOT EXISTS 'coupon'@'%' IDENTIFIED BY 'coupon123';
GRANT ALL PRIVILEGES ON coupon_db.* TO 'coupon'@'%';
FLUSH PRIVILEGES;

-- 校验：应看到 coupon + user_coupon_0 .. user_coupon_15 共 17 张表
SHOW TABLES;
