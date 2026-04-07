#pragma once
#include <memory>
#include <type_traits>
#include <utility>

#if __cplusplus < 201402L
namespace std {
    template <typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template <typename T>
    std::unique_ptr<T> make_unique(std::size_t size) {
        static_assert(std::is_array<T>::value, "T must be an array type");
        using U = typename std::remove_extent<T>::type;
        return std::unique_ptr<T>(new U[size]());
    }
}

#endif
    
namespace GST {
namespace LOG {

enum class LOG_LEVEL {
    LEVEL_DEBUG = 0,
    LEVEL_INFO = 1,
    LEVEL_WARN = 2,
    LEVEL_ERROR = 3,
    LEVEL_FATAL = 4
};

// 文件分割方式
enum LOG_TRUNC_TYPE {
    // 系统时间
    TRUNC_TYPE_SYS_TIME,
    TRUNC_TYPE_FILE_SZIE,
    TRUNC_TYPE_NONE
};

}
}
