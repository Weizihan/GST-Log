# GST-Log 验收记录

本次修改起点：detached HEAD `ab788640cd4ec970084a26508f25fb124753d713`，工作区原先干净。
参考提交 `096fd30e78dc921f1dfd34518f14a7da14039cf8` 存在；没有切换、重置或覆盖为该版本。
全部构建、安装和运行产物位于 `/tmp`，没有进入 Loop/Net 集成。

## 可重复执行的命令

在 GST-Log 根目录执行：

```bash
cmake -S . -B /tmp/gst-log-debug -DCMAKE_BUILD_TYPE=Debug \
  -DGST_LOG_BUILD_TESTS=ON -DBUILD_EXAMPLE=ON \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'
cmake --build /tmp/gst-log-debug --parallel 2
ctest --test-dir /tmp/gst-log-debug --output-on-failure

cmake -S . -B /tmp/gst-log-release -DCMAKE_BUILD_TYPE=Release \
  -DGST_LOG_BUILD_TESTS=ON -DBUILD_EXAMPLE=ON \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'
cmake --build /tmp/gst-log-release --parallel 2
ctest --test-dir /tmp/gst-log-release --output-on-failure

cmake -S . -B /tmp/gst-log-asan -DCMAKE_BUILD_TYPE=Debug \
  -DGST_LOG_BUILD_TESTS=ON -DBUILD_EXAMPLE=ON \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g'
cmake --build /tmp/gst-log-asan --parallel 1
ctest --test-dir /tmp/gst-log-asan --output-on-failure

cmake -S . -B /tmp/gst-log-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DGST_LOG_BUILD_TESTS=ON \
  '-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer -g'
cmake --build /tmp/gst-log-tsan --parallel 1
ctest --test-dir /tmp/gst-log-tsan --output-on-failure
```

## 实际结果

编译器为 GCC 15.2.0，CMake 4.2.3，Linux。

| 验证 | 最终结果 |
|---|---|
| Debug，库、示例和测试 | 构建成功，11/11 CTest 通过 |
| Release，库、示例和测试 | 构建成功，11/11 CTest 通过 |
| ASan + UBSan + LeakSanitizer | 11/11 通过，未报告相应检测问题 |
| TSan | 11/11 通过，未报告数据竞争 |
| 测试关闭的 Release 构建 | 成功，不依赖测试替换或动态链接辅助代码 |
| 安装后的外部工程 | 四种配置均成功编译、链接、运行；仅包含公开 GstLog.h，链接导出的 GST::Log |
| 严格标准 C++17 外部工程 | 设置 CXX_STANDARD=17、CXX_EXTENSIONS=OFF 后，四种配置重新验证通过 |
| 小规模示例 file / async | 各接受 4,000 条、拒绝 0 条；各实际输出 260,000 字节，与预期一致 |
| git diff --check | 通过 |

受跟踪的执行环境中，LeakSanitizer 初次运行因不支持 ptrace 而失败；这不是测试通过。
随后在获准的非跟踪环境中，使用默认启用的泄漏检测完成了全套 ASan/UBSan/LSan 验收，没有关闭泄漏检测。
TSan 不需要改变测试内容即可完成。测试结果不等于穷尽全部线程交错或证明所有负载无缺陷。

完整 CTest 输出保存在对应构建目录的 `validation.log`；严格 C++17 外部项目复核保存在 `external-validation.log`。

## 覆盖范围

- `format`：pattern、未知 token、空消息、已有换行、过滤、控制台颜色及 NUL。
- `registry`：单/多后端、同类型多实例、广播隔离、名称/索引、未知路由、重复名称/路径、字符串格式、长消息和生命周期。
- `compatibility`：默认初始化、实例名称与后端类型独立（包括 Async/Console 名称）、原有宏及 std::string 格式调用；`registry` 同时检查空类型被拒绝。
- `paths`：绝对/相对目录、自动创建、续写、父路径错误、无效参数、失败后重试；本次非 root 环境实际验证了权限不足。
- `rotation_sync/rotation_async`：04:59 到 05:00、每日零点、长时间无日志、同段续写/跨段重启、大小阈值重启、同秒归档冲突、组合条件、单调时钟间隔、归档失败和归档后新文件创建失败。
- `async_lifecycle`：在途数据计入记录/字节上限，满载和超大消息拒绝，flush、关闭排空、重复关闭及关闭后拒绝。
- `errors`：EINTR、短写、磁盘写入失败、错误结果保持、后续继续输出、删除后重建、/dev/full 和控制台 flush 失败。
- `concurrent`：同步/异步各四个生产者、各 2,000 条，检查数量、完整性和每个生产者的顺序。
- `early_stop`：通过线程入口和 join 的握手，把 worker 固定延迟到停止请求之后，验证显式关闭和析构都完整排空 50 条。
- `external`：真实安装、导出依赖、独立 CMake 工程、公开头文件、宏、NUL 和异步关闭。

时间、写入和轮转失败由测试程序的链接替换控制，生产库没有运行期测试回调。
挂起保护使用 CTest TIMEOUT，不用它证明生产接口有硬截止时间。没有随机 sleep 或专门吞吐量优化。

## 已知边界

失败的异步批次报告后释放，不自动重放；后续处理继续，flush/shutdown 持续返回 false。
不提供断电持久化、硬超时、多进程轮转协调、自动清理归档或编码转换。
重启的自然时间恢复依赖文件修改时间，固定间隔在重启/重开后重新计时。
这些边界和路径解释规则详见主 README，未被测试结果扩大为更强保证。
