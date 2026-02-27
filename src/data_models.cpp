/**
 * @file data_models.cpp
 * @brief FRPC 框架数据模型实现
 * 
 * 实现数据模型的相关方法，主要是请求 ID 生成逻辑。
 */

#include "frpc/data_models.h"

namespace frpc {

/**
 * @brief 全局请求 ID 计数器
 * 
 * 使用原子变量确保线程安全的 ID 生成。
 * 初始值为 0，每次调用 generate_id() 时递增。
 * 
 * @note 使用 std::memory_order_relaxed 因为我们只需要原子性，
 *       不需要严格的内存顺序保证
 */
static std::atomic<uint32_t> g_request_id_counter{0};

uint32_t Request::generate_id() {
    // 使用 fetch_add 原子递增并返回递增前的值
    // 加 1 确保 ID 从 1 开始（0 保留为无效 ID）
    return g_request_id_counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

} // namespace frpc
