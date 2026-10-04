# TASK 2：推理库与应用边界

```text
Reader → FrameBus → InferencePipeline [输入队列 → 单个 TrtWorker]
                                             │ 结果回调（worker 线程）
                                             ▼
                         CLI 结果队列 → 主线程渲染 / 显示 / 写视频
```

`yolo_trt_core` 包含 `TrtEngine`、`TrtWorker` 和 `InferencePipeline`。
它不读视频或 YAML，不画框，也不负责 GUI。
暂时沿用 `src/` 和 `include/yolo/`，通过 CMake target 划分库和应用。
未来 Qt、服务器、ROS 2 适配器可以直接链接该库。

## 接口与所有权

- 构造 pipeline：接收已解析的 `PipelineConfig`，同步加载 engine。
- `start()`：创建唯一 worker；context 和 CUDA stream 在 worker 内创建。
- `submit(FramePtr)`：接收任务。返回 true 表示接收成功，不表示推理完成。
- 结果回调：在 worker 线程运行，带检测数据及原图引用。异常会进入错误处理。
- `request_stop(mode)`：关闭输入队列，不等待线程，可从回调调用。
- `stop(mode)`：关闭输入并等待 worker，包括回调结束；只能由拥有者线程调用。
- 析构：以 CancelPending 兜底。禁止从回调销毁 pipeline。
- 实例只能启动一次。再次运行需要创建新实例。

`FrameBus::publish()` 同步依次调用订阅者，只分发引用，没有额外线程或缓冲。
订阅关系在启动前设置；所有订阅者都拒绝时返回 false。
订阅者抛异常会终止本次发布并传播给发布者。
当前只有 YOLO 订阅者。其他模型可订阅同一个来源并管理各自任务队列。

Reader 对输入 `clone()` 后发布 `shared_ptr<const Frame>`。
原图及其别名在发布后必须只读；const 不会阻止通过其他 cv::Mat 别名修改像素。
结果持有原图，渲染器复制原图再画框。当前没有内存池。

## 队列与停止

| 场景 | 输入任务队列 | CLI 结果队列 |
|---|---|---|
| offline | 配置容量；满时等待 | 配置容量；满时等待 |
| realtime | 容量 1；替换旧帧 | 容量 1；替换旧结果 |

容量只计算等待任务，不包含正在处理或显示的帧。
离线回调可以阻塞在 CLI 结果队列；主线程一直消费它。
CLI 输出失败时先关闭结果队列，解除回调阻塞，再等待 worker。
实时回调不等待队列空位。将来 Qt 适配器需要把结果交给 GUI 线程。

自然结束使用 Drain：拒绝新任务，处理待处理任务后退出。
用户停止使用 CancelPending：拒绝新任务、释放待处理引用，当前任务仍交付结果。
CancelPending 可升级先前的 Drain。停止不会重新打开队列。
单 worker 保持接收顺序；丢帧导致 ID 间隔是正常的，CLI 不等待连续 ID。

`stop()` 的线程终止保证要求结果消费者能继续接收或主动解除回调阻塞。
OpenCV 的设备/网络读取可能阻塞；停止标志不会中断正在进行的 `VideoCapture::read()`。
当前读线程会在读取返回后退出，不提供摄像头/网络源的硬性停止时限。

## 日志与指标

统一输出 INFO/WARN/ERROR、时间、模块和线程 ID，写入 stderr，不逐帧刷屏。
TensorRT 日志也进入该入口。初始化失败抛异常；后台异常取消输入并通知错误回调，
随后触发完成回调。CLI 在 join 后重新抛出记录的异常。

Submitted 统计已接受任务；processed 统计成功完成推理的任务；input dropped
只统计输入队列替换掉的任务，不含停止取消。Frames 是 CLI 实际消费结果数；
实时显示可能再次丢弃结果，因此这些计数不一定相等。
实时视频写出为固定 FPS，不保留原始时间戳。

推理计时仍为 TensorRT + D2H；端到端计时从提交前到主线程取到结果，
不包含主线程画框、显示和编码。旧版本画框发生在 worker 内，因此端到端指标
与旧版本不完全同口径。无显示/输出时新版本跳过画框；比较性能应保持配置一致。
实时模式 FPS 为实际消费结果的速率。

## 验收与学习入口

1. 在原有 GPU 环境构建，运行同一 engine、视频与 offline 配置，核对帧数。
2. 按 q 停止；填错输入路径、输出路径；确认线程退出且错误可见。
3. 切 realtime，确认队列容量 1、允许 ID 跳跃且能完成退出。
4. 运行 CTest 检查阻塞/唤醒、排空/取消、丢旧释放引用、共享分发及原图不变。

Review 顺序：`types.hpp` → `pipeline.hpp` → `blocking_queue.hpp` →
`pipeline.cpp` → `cli.cpp`。先关注数据所有权、回调线程和停止时谁解除阻塞。
`examples/callback_demo.cpp` 可单独练习普通回调与线程的区别。
