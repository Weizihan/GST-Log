# GST_log

面向 Linux / C++17 的独立静态日志库。保留 `GstLogger` 门面和 Console、File、AsyncFile 后端。
文件后端采用追加写入；异步后端保持 FIFO 和单 worker，不依赖 EventLoop。

## 构建、测试和安装

```bash
cmake -S . -B /tmp/gst-log-build -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_EXAMPLE=ON -DGST_LOG_BUILD_TESTS=ON
cmake --build /tmp/gst-log-build --parallel 2
ctest --test-dir /tmp/gst-log-build --output-on-failure
cmake --install /tmp/gst-log-build --prefix /path/to/gst-log
```

测试默认关闭。构建产物留在构建目录；CTest 包含安装后独立外部项目的编译、链接和运行。
测试使用可控时间和写入屏障，不等待实际整点或随机 sleep。工具链要求 Linux、C++17、pthread；测试另外使用 GNU/LLVM 链接器的 `--wrap` 和系统动态链接接口，无第三方测试框架。

安装后的外部 CMake 工程：

```cmake
find_package(GSTLog CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE GST::Log)
```

配置时传入 `-DCMAKE_PREFIX_PATH=/path/to/gst-log`。源码嵌入方式：

```cmake
add_subdirectory(path/to/GST-Log)
target_link_libraries(my_app PRIVATE GST::Log)
```

实体构建目标为 `GST_Log`，静态库名为 `libGST_Log.a`；`GST::Log` 是统一的 CMake 使用入口。

## 使用

```cpp
#include "GstLog.h"

int main() {
    GST::LOG::LogConfig config;
    config._logger_name = "application";
    config._log_type = "async";
    config._log_target = "./logs/";
    config._log_file_name = "application.log";
    config._log_level = GST::LOG::LOG_LEVEL::LEVEL_INFO;
    config._log_format = "[%T][%L][%R:%I] %S";
    config._natural_rotation = GST::LOG::NaturalRotation::Hourly;
    config._log_max_size = 10 * 1024 * 1024;

    auto logger = GST::LOG::get_Instance();
    if (!logger->init(config)) return 1;
    INFO_NAME("application", "started, port=%d", 9000);
    // 先停止所有日志生产者，再关闭日志系统。
    return logger->shutdown() ? 0 : 2;
}
```

无参数 `init()` 创建名为 `default`、最低级别 DEBUG 的彩色控制台后端。
初始化属于启动阶段操作。后端对象成功初始化一次后不能再次初始化；门面 shutdown 后不能重新开启。
配置和销毁不得与直接后端对象的初始化并发。生产者可并发写入，flush 可与生产者并发，重复 shutdown 安全。

## 配置和路由

- `_logger_name` 是唯一实例名称，支持多个同类型实例。
- `_log_type` 接受 `console`、`file`、`async`，不区分大小写，默认值为 `file`；显式传入空值或未知类型时初始化失败。`_logger_name` 是用户自定义的实例名称，仅用于注册、查询和路由，不参与后端类型选择。
- 同名配置和同一规范化文件路径上的第二个后端均被拒绝，在打开新文件之前检查。
- `INFO/WARN/ERROR/DEBUG/FATAL` 广播；`*_NAME` 按名称路由；`*_INDEX` 按初始化顺序路由。
- 仅索引 `-1` 表示广播，其他负数、越界索引和未知名称均被拒绝，不会意外广播。
- 级别为 `DEBUG < INFO < WARN < ERROR < FATAL`，输出不低于配置级别的日志。
- `_log_encoding` 目前只接受 `UTF-8` / `utf-8`，按原始字节输出，不做转码或 UTF-8 有效性检查。其他编码明确初始化失败。
- 无效级别、负文件阈值/间隔、非正异步缓冲区或队列限制均被拒绝，不创建目标目录或文件。

原来只有声明、没有定义的无参数 `add_logger()` / `delete_logger()` 已移除；仓库没有调用方。
新增后端仍使用 `init(config)`。不支持运行时删除注册项，因而没有额外的对象回收机制。

## 路径规则

相对路径在初始化时以进程当前工作目录解析，之后 `chdir` 不改变该后端的目标。父目录自动创建，失败会报告路径和原因并返回 false。

`_log_target` 是已有目录、以 `/` 结尾，或尚不存在且没有扩展名时，按目录处理，与 `_log_file_name` 拼接：

- `/data` + `application.log` → `/data/application.log`（`/data` 不存在或本身是目录时）；
- `./log/` + `application.log` → 当前工作目录下的 `log/application.log`；
- `./log/application.log` → 直接作为完整文件路径。

已有的无扩展名普通文件仍作为文件使用。要新建无扩展名文件，请将目录写入 `_log_target`、文件名写入 `_log_file_name`。
目录模式下文件名必须是单一路径组件，不能包含父路径。路径不能含 NUL，符号链接在初始化时解析。
一个活动文件应由一个后端独占；不承诺跨进程、硬链接别名或外部工具同时轮转的协调能力。

## 格式与消息

日志宏使用 printf 风格参数；格式与实参类型必须匹配。普通字符串格式参数仍可使用 `std::string`。
所有后端共享基类的级别过滤、pattern 展开和换行流程，Console 只额外装饰颜色。

| token | 内容 |
|---|---|
| `%P` | 线程 ID 的哈希表示 |
| `%L` | 级别 |
| `%T` | 本地时间，含毫秒 |
| `%F` | 函数名 |
| `%I` | 行号 |
| `%S` | 用户消息 |
| `%R` | 源文件名 |
| `%%` | 字面量 `%` |

未知 token 保留原文。token 不附加空格，分隔符完全由 pattern 控制；最终输出保证以换行结束，已有末尾换行不重复追加。
消息内嵌 NUL 按长度保留，包括名称路由和控制台输出。广播各后端独立格式化，不会把前一个后端的结果作为下一个后端的消息。

## 文件轮转和重启

`_natural_rotation` 支持：

- `NaturalRotation::None`：关闭自然时间条件；
- `NaturalRotation::Hourly`：本地自然整点；
- `NaturalRotation::Daily`：本地每日零点；
- `NaturalRotation::Legacy`（默认）：兼容 `_trunback_type`，true 为 Daily，false 为 None。

04:59 启动的小时轮转会在 05:00 后第一次实际写入前轮转，不从启动时间累计一小时。
没有日志时不创建空归档，空活动文件直接复用。同步、异步使用同一个文件轮转实现；异步以 worker 实际写入时的时间段决定归属，不保证按消息生成时间拆分批次。

`_log_max_size > 0` 启用大小条件。大小与时间条件可同时启用，任一满足就轮转；同时满足只归档一次。
大小在下一条同步记录或下一批异步数据写入前检查，因此是轮转阈值，不是硬文件大小上限。

`_rotation_interval > 0` 额外启用固定间隔条件，使用单调时钟，自文件初始化、重开或上一次成功轮转开始计时。
它可以与其他条件组合；进程重启后重新开始间隔计时，不恢复旧的间隔起点。

初始化只追加，不主动归档。非空已有文件的最后修改时间用于恢复自然时间段：仍在当前段则续写，跨段则在下一次写入前归档；已经达到大小阈值也在下一次写入前轮转。
归档名包含时间戳，冲突时追加序号。先安全 rename，再创建新活动文件；归档失败保留原文件、报告错误并拒绝本次文件写入，后续写入可重试。新活动文件创建失败时旧日志仍在归档中。
文件删除或替换后，后续写入尝试重新打开、创建目标。

恢复不引入持久化状态。修改时间被外部工具改动、恢复旧备份、系统时钟/时区变化会影响判断；本地夏令时重复小时视为同一个小时标签。
请在启动前设置时区。无需多进程协调的普通服务适用，需要严格审计时间分桶时应另行设计。
归档没有自动清理策略，应由部署侧管理磁盘保留。

## 异步接受、满载和生命周期

`_buffer_size` 是初始缓冲预留和批次目标大小，默认 64 KiB；不是总队列容量。

- `_max_pending_logs`：最大未完成记录数，默认 8192；
- `_max_pending_bytes`：最大未完成格式化字节数，默认 8 MiB；
- 两个限制都包含 current、ready 和正在 I/O 的数据；超出任一条件即拒绝新消息，不等待队列空位。
- 单条消息超过批次目标仍可接受，但不能超过总字节限制。初始缓冲大小不能大于总字节限制。
- 限制约束待处理有效载荷，内存另有容器、分配容量和已完成批次释放前的短暂开销，并非精确 RSS 上限。

已有 `void log()` 和日志宏保留；拒绝的有效日志会向 stderr 报告。
需要检查满载或关闭结果时使用兼容新增的 `try_log()`：

```cpp
bool accepted = GST::LOG::get_Instance()->try_log(
    "application", GST::LOG::LOG_LEVEL::LEVEL_INFO,
    __FILE__, __LINE__, __func__, "connection=%d", 42);
```

单个异步后端返回 true 仅表示已接受，不表示已经写入磁盘。
过滤、关闭、无效路由或队列满时返回 false。广播返回 true 表示至少一个符合级别的后端接受，且所有符合级别的后端均接受；false 时可能已有部分后端接受，不能盲目重试广播。
`try_log` 的队列拒绝不写 stderr，适合调用者自行决定降级策略；原宏保留错误诊断，但 stderr 本身可能阻塞。

`flush()` 和 `shutdown()` 不带超时：

- flush 等待后端调用边界之前已接受数据的处理，生产者可以继续写；门面依次建立各后端边界，不承诺全局同一时刻快照。
- shutdown 停止入口，尝试写出所有已接受记录，再 join worker、关闭文件。重复调用安全。
- worker 即使在停止请求后才开始执行，也会排空已接受的 current 和 ready 数据。
- I/O 失败的批次报告后释放，不自动重放；worker 继续处理后续数据并尝试重开文件。部分写入失败可能留下不完整记录。
- 一旦发生输出/轮转失败，flush/shutdown 对该实例持续返回 false，避免后来的成功掩盖之前的数据损失；初始化失败后重试成功，不继承之前初始化阶段的错误。
- 同步后端 try_log 的失败直接返回 false；异步已接受日志的后续 I/O 错误由 flush/shutdown 返回值和 stderr 报告。
- 析构只做尽力收尾；需要确认结果时必须显式 shutdown。应先停止生产者，保证对象生命周期覆盖所有调用。

文件后端直接使用 Linux write(2)，处理 EINTR 和短写；flush 表示数据已提交给操作系统，不调用 fsync/fdatasync，不保证断电持久化。
没有 I/O 取消能力，阻塞设备或文件系统可能延长 flush/shutdown；本版不提供硬实时截止时间。
无用户回调、运行期删除后端或外部资源托管，内部锁不执行应用回调。

## 兼容性说明

保留原日志宏、门面、`GST_Log` 目标和三个后端的职责划分。新增 try_log / flush / shutdown。
日志格式去掉隐式空格，过滤方向修正为最低级别语义，非法路由不再广播，因此输出行为会按上述规则变化。
可变参数主入口改用 `const char*` 格式，避免把引用参数用于 va_start；std::string 调用通过模板适配保留，旧函数指针类型需调整。
自定义后端需实现 flush/shutdown，不能覆盖公共 log 流程，输出放在 write_log；这些是统一处理和生命周期所需的接口调整。
