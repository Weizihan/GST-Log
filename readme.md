# GST_log

## 编译

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build -j
```

仅编译静态库 `libTlog.a`，输出在 `build/` 下。

若要一并编译示例程序：

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON
cmake --build build -j
```

可执行文件输出目录为 `output/`（例如 `output/example_Async`）。

## 调用

1. 链接 `Tlog` 静态库，并链接系统线程库（如 `-pthread` 或 CMake 的 `Threads::Threads`）。
2. 包含头文件 `GstLog.h`（需能解析到同目录下的 `src/` 等路径，与当前工程 `include_directories` 一致）。
3. 初始化后使用宏写日志，例如：

```cpp
#include "GstLog.h"

int main() {
    if (!GST::LOG::GstLogger::get_Instance()->init()) {
        return 1;
    }
    INFO("hello %d", 42);
    return 0;
}
```

也可先构造 `GST::LOG::LogConfig` 再调用 `init(config)` 配置文件/异步等后端。
