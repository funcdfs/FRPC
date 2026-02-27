/**
 * @file error_codes.cpp
 * @brief 错误码描述信息实现
 */

#include "frpc/error_codes.h"

namespace frpc {

std::string_view get_error_message(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success:
            return "成功";
        
        // 网络错误
        case ErrorCode::NetworkError:
            return "通用网络错误";
        case ErrorCode::ConnectionFailed:
            return "连接失败";
        case ErrorCode::ConnectionClosed:
            return "连接关闭";
        case ErrorCode::SendFailed:
            return "发送失败";
        case ErrorCode::RecvFailed:
            return "接收失败";
        case ErrorCode::Timeout:
            return "超时";
        
        // 序列化错误
        case ErrorCode::SerializationError:
            return "序列化失败";
        case ErrorCode::DeserializationError:
            return "反序列化失败";
        case ErrorCode::InvalidFormat:
            return "格式无效";
        
        // 服务错误
        case ErrorCode::ServiceNotFound:
            return "服务未找到";
        case ErrorCode::ServiceUnavailable:
            return "服务不可用";
        case ErrorCode::NoInstanceAvailable:
            return "没有可用的服务实例";
        case ErrorCode::ServiceException:
            return "服务执行异常";
        
        // 参数错误
        case ErrorCode::InvalidArgument:
            return "参数无效";
        case ErrorCode::MissingArgument:
            return "缺少参数";
        
        // 系统错误
        case ErrorCode::InternalError:
            return "内部错误";
        case ErrorCode::OutOfMemory:
            return "内存不足";
        case ErrorCode::ResourceExhausted:
            return "资源耗尽";
        
        default:
            return "未知错误";
    }
}

} // namespace frpc
