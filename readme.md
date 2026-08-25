# GST_log

面向 C++17 Linux 应用的轻量日志库，提供控制台、同步文件和异步文件三种后端，支持日志级别、格式化、文件轮转、并发写入以及可超时的 `flush` / `shutdown` 生命周期控制。

GST_log 主要用于 GST 系列基础设施和服务端项目，也可以作为独立静态库接入其他 CMake 工程。

## 特性

- **三种日志后端**：彩色控制台、同步文件、单 worker 异步文件。
- **统一调用入口**：应用配置一个进程级 `GstLogger`，通过日志宏按广播、索引或名称路由。
- **并发日志写入**：多个业务线程可以同时写日志；同一个异步后端按 accepted 顺序写入文件。
- **异步批处理**：buffer 满时立即发布，低流量下最多等待约 100ms 发布 partial buffer。
- **生命周期控制**：`flush(timeout)` 建立同步边界，`shutdown(timeout)` 停止入口并排空已接受日志。
- **文件轮转保护**：支持按日期或文件大小轮转；备份失败时保留并继续追加原文件。
- **运行期故障恢复**：文件暂时不可用时不向业务线程传播异常；条件恢复后复用同一个 Logger 继续写入。
- **故障诊断**：后端进入 degraded 和 recovered 状态时通过 `stderr` 报告，避免递归进入日志系统。

## 适用场景

GST_log 适合作为以下组件的基础日志设施：

- Linux 服务端和后台进程；
- `GST_EventLoop`、`GST_net` 等基础库；
- 需要多个业务线程写入同一日志文件的应用；
- 希望业务日志调用与文件 I/O 解耦的应用。

审计、交易等要求日志在机器断电后仍然可靠保存的场景，不应只依赖当前 `flush()`。

## 架构

```text
                        application threads
                  INFO / WARN / ERROR / DEBUG
                                │
                                ▼
┌──────────────────────────────────────────────────────────┐
│ GstLogger                                                │
│  ├─ logger registry                                      │
│  ├─ index / name / broadcast routing                     │
│  └─ process-level flush and shutdown                     │
└─────────────────────────────┬────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌──────────────────────┐
│ ConsoleLogger   │ │ FileLogger      │ │ AsyncFileLogger      │
│ format + stdout │ │ format + write  │ │ format + shared      │
│ optional color  │ │ caller thread   │ │ current buffer       │
└─────────────────┘ └─────────────────┘ └──────────┬───────────┘
                                                   │ full / 100ms /
                                                   │ flush / shutdown
                                                   ▼
                                        ┌──────────────────────┐
                                        │ FIFO ready buffers   │
                                        └──────────┬───────────┘
                                                   ▼
                                        ┌──────────────────────┐
                                        │ single I/O worker    │
                                        │ write / retry /      │
                                        │ rotation / recovery  │
                                        └──────────┬───────────┘
                                                   ▼
                                                log file
```

### 组件职责

| 组件 | 职责 |
|---|---|
| `GstLogger` | 保存已配置的 backend，完成日志路由，并统一执行 `flush` 和 `shutdown`。 |
| `Logger` | 所有日志后端的抽象接口，定义格式化、写入和生命周期操作。 |
| `ConsoleLogger` | 在调用线程格式化并写入 `stdout`，可选彩色输出。 |
| `FileLogger` | 在调用线程同步完成格式化、轮转检查、文件写入和 flush。 |
| `AsyncFileLogger` | 业务线程只追加共享 buffer，单独的 I/O worker 负责批量写入、轮转和恢复。 |
| `LogFormat` | 根据 pattern 生成最终日志行。 |

## 异步写入模型

同一个 `AsyncFileLogger` 使用一个共享 current buffer 和一个 FIFO ready 队列：

```text
producer append
      │
      ├─ current buffer 满 ────────────────┐
      ├─ partial buffer 到达 100ms ───────┤
      ├─ flush() 强制发布 ────────────────┤
      └─ shutdown() 强制发布 ─────────────┤
                                           ▼
                                  FIFO ready buffers
                                           │
                                           ▼
                                  single I/O worker
```

日志完整追加到 current buffer 后算作 **accepted**。文件顺序以 accepted 顺序为准，full buffer、定时 partial buffer 以及 `flush` / `shutdown` 发布的 buffer 不会互相越过。

并发线程的时间戳在进入共享 buffer 前生成，因此文件中的时间戳不承诺严格递增。100ms 是健康后端和正常线程调度下的批处理等待目标，不是实时截止时间；积压、线程调度延迟或文件故障都可能推迟实际可见时间。

## 环境要求

- Linux；
- 支持 C++17 的编译器；
- CMake 3.16 或更高版本；
- pthread。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

构建示例程序：

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLE=ON
cmake --build build -j
```

示例程序默认不构建，需要显式设置 `BUILD_EXAMPLE=ON`。

## 安装和 CMake 接入

安装到自定义目录：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /path/to/gst-install
```

下游工程：

```cmake
find_package(GST_log CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE GST::Log)
```

如果安装在非系统目录，可以在配置下游工程时指定：

```bash
cmake -S . -B build \
    -DCMAKE_PREFIX_PATH=/path/to/gst-install
```

同一个源码树中也可以直接使用：

```cmake
add_subdirectory(path/to/GST_log)
target_link_libraries(my_app PRIVATE GST::Log)
```

## 快速开始

### 异步文件日志

```cpp
#include "GstLog.h"

#include <chrono>

int main() {
    GST::LOG::LogConfig config;
    config._logger_name = "application";
    config._logger_type = GST::LOG::LoggerType::AsyncFile;
    config._log_level = GST::LOG::LOG_LEVEL::LEVEL_INFO;
    config._log_format = "[%T][%L][%P] %S";
    config._log_target = "logs/application.log";
    config._buffer_size = 64 * 1024;
    config._trunback_type = false;
    config._log_max_size = 10 * 1024 * 1024;

    auto logger = GST::LOG::get_Instance();
    if (!logger->init(config)) {
        return 1;
    }

    INFO_NAME("application", "server started, port=%d", 9000);
    WARN_NAME("application", "queue usage=%d%%", 80);

    // 应在所有日志生产者停止后调用。
    if (!logger->shutdown(std::chrono::seconds(2))) {
        return 2;
    }
    return 0;
}
```

日志宏使用 `printf` 风格格式字符串。

### 默认控制台日志

无参数 `init()` 会创建名为 `default` 的控制台后端：

```cpp
#include "GstLog.h"

#include <chrono>

int main() {
    auto logger = GST::LOG::get_Instance();
    if (!logger->init()) {
        return 1;
    }

    INFO("hello %s", "GST_log");
    logger->shutdown(std::chrono::seconds(1));
    return 0;
}
```

## 日志路由

应用可以配置多个不同名称的 backend。名称用于标识实例，`LoggerType` 用于选择实现类型。

```cpp
GST::LOG::LogConfig console;
console._logger_name = "console";
console._logger_type = GST::LOG::LoggerType::Console;
console._log_level = GST::LOG::LOG_LEVEL::LEVEL_WARN;
console._log_format = "[%L] %S";
console._is_colorful_log = true;

GST::LOG::LogConfig file;
file._logger_name = "file";
file._logger_type = GST::LOG::LoggerType::AsyncFile;
file._log_level = GST::LOG::LOG_LEVEL::LEVEL_DEBUG;
file._log_format = "[%T][%L] %S";
file._log_target = "logs/application.log";

auto logger = GST::LOG::get_Instance();
logger->init(console);
logger->init(file);
```

三种路由方式：

```cpp
INFO("broadcast to every configured logger");
INFO_NAME("file", "write only to the named logger");
INFO_INDEX(0, "write to logger index 0");
```

- 普通宏使用内部索引 `-1`，广播到全部 backend；
- `*_NAME` 按唯一名称路由，推荐业务代码优先使用；
- `*_INDEX` 按初始化顺序路由，适合索引稳定的内部场景；
- 其他负数或越界 index 会被拒绝并报告到 `stderr`。

## 日志级别

级别从低到高：

```text
DEBUG < INFO < WARN < ERROR < FATAL
```

低于 backend 配置级别的日志会被过滤，不算作 accepted，也不进入 `flush` / `shutdown` 边界。

## 格式化 pattern

| Token | 内容 |
|---|---|
| `%P` | 当前线程 ID。 |
| `%L` | 日志级别。 |
| `%T` | 当前时间，精确到毫秒。 |
| `%F` | 函数名。 |
| `%I` | 源码行号。 |
| `%S` | 用户消息。 |
| `%R` | 源文件名。 |
| `%%` | 字面量 `%`。 |

示例：

```cpp
config._log_format = "[%T][%L][%P][%R:%I][%F] %S";
```

## 文件轮转

当前支持两种轮转方式：

### 按日期

```cpp
config._trunback_type = true;
```

日期变化后，现有日志文件会被备份，并创建新的 active file。

### 按文件大小

```cpp
config._trunback_type = false;
config._log_max_size = 10 * 1024 * 1024;
```

当 active file 达到配置大小时执行轮转。备份名包含时间戳；同一秒发生多次轮转时会增加序号避免覆盖。

如果 rename 和 copy 都失败，Logger 不会 truncate 原文件，而是重新以 append 模式打开原文件并继续写入。

## Flush 和 shutdown

普通业务模块只需要写日志。`flush` 和 `shutdown` 是应用生命周期层使用的低频控制接口。

### flush

```cpp
if (!logger->flush(std::chrono::seconds(2))) {
    // timeout 内未完成边界前日志的写入。
}
```

`flush(timeout)` 等待调用边界前已经 accepted 的日志写入，并把 C++ 流缓冲提交给操作系统。调用期间日志入口保持开放，边界之后的新日志不属于本次等待范围。

### shutdown

```cpp
if (!logger->shutdown(std::chrono::seconds(2))) {
    // 可以在文件后端恢复后重试。
}
```

`shutdown(timeout)` 的顺序是：

```text
停止接受新日志
      ↓
发布 current partial buffer
      ↓
排空 accepted 日志
      ↓
停止 worker 并关闭文件
```

成功后新日志会被拒绝，重复 shutdown 是安全的。异步 shutdown 超时时会保留 pending，并允许调用者稍后重试。

应用应先停止 EventLoop、Server、业务线程等日志生产者，再调用日志系统的 shutdown。

## 文件故障与恢复语义

同步 `FileLogger` 不保存失败日志。当前日志写入失败时，该条日志可能丢失；后续日志会尝试恢复同一个 Logger。

`AsyncFileLogger` 会保留尚未确认完成的 batch：

- 能确认没有开始写入时，恢复后直接重试；
- 写入结果无法确认时，完整重放原 batch；
- 重放前写入包含 `batch_id` 和 `attempt` 的 replay marker；
- marker 写入失败时通过 `stderr` 报告，原 batch 仍然重试。

异步故障恢复采用 **at-least-once** 语义：优先避免日志缺失，但不确定写入路径可能出现重复日志。

当前不提供公开的 backend health、pending 数量或最后错误查询接口。后端首次故障和恢复各通过 `stderr` 报告一次。

## 持久化边界

当前 `flush()` 最终调用 `std::ofstream::flush()`，只保证数据从 C++ 用户态缓冲提交给操作系统，不调用 `fsync` 或 `fdatasync`。

因此当前版本不承诺机器断电后的日志持久化。需要 durable log 的应用，应等待未来单独设计的持久化接口，或自行采用具备相应保证的存储方案。

## 目录结构

```text
GST_log/
├── CMakeLists.txt
├── GstLog.h                         # 用户入口和日志宏
├── GstLog.cpp                       # 全局 facade 访问入口
├── cmake/
│   └── GST_logConfig.cmake.in       # find_package package 模板
├── example/
│   └── Async_example.cpp
├── src/
│   ├── GstLogger.h/.cpp             # backend registry、路由和生命周期
│   ├── LogConfig.h/.cpp             # backend 配置
│   ├── LogFormat.h/.cpp             # pattern 格式化
│   ├── Marco.h                      # 日志级别和公共枚举
│   └── logger/
│       ├── Logger.h                 # backend 抽象接口
│       ├── ConsoleLogger.h/.cpp
│       ├── FileLogger.h/.cpp
│       └── AsyncFileLogger.h/.cpp
```
