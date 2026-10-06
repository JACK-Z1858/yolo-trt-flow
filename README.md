# YoloTRTFlow v0.1.0

YoloTRTFlow is a compact YOLOv8 inference pipeline built with TensorRT 10 and OpenCV CUDA.
It loads a TensorRT `.engine` containing EfficientNMS and accepts images, image
directories, videos, cameras, and video streams.

The reusable `yolo_trt_core` library loads the engine and processes submitted
frames on one background worker. The CLI owns image/video input, rendering,
display, and video output. Runtime options are stored in YAML.

Offline mode blocks when queues fill and processes every frame. Realtime mode
keeps only the latest pending input and output (capacity 1). A `FrameBus` shares
read-only frame references with subscribers; each pipeline owns its own queue.
See [the TASK 2 architecture guide](docs/task2-architecture.md) for callbacks,
ownership, and shutdown semantics.

The repository also includes a minimal serial Python CPU demo for a simple
performance comparison.

## Project layout

```text
├─ include/                Headers, grouped by responsibility
│  ├─ common/              Frame, FrameBus, bounded queue, and logging
│  ├─ yolo/                YOLO result types, TensorRT engine, and pipeline
│  └─ app/cli/             CLI configuration, reader, rendering, and orchestration
├─ src/                    Implementations, mirroring include/
│  ├─ common/              Shared logging implementation
│  ├─ yolo/                TensorRT engine and pipeline implementations
│  └─ app/cli/             CLI entry point and frontend implementations
├─ tests/                  common/ and app/cli/ tests
├─ examples/               Standalone callback exercise
├─ docs/                   Architecture and review guide
├─ config.example.yaml     Example runtime configuration
├─ cpu_demo.py             Serial Python CPU baseline
├─ export-det.py           YOLOv8 ONNX export utility
└─ models/common.py        EfficientNMS export support
```

Namespaces follow the same boundaries: `common`, `yolo`, and `app::cli`.
The dependency direction is CLI → YOLO/common and YOLO → common.
Shared frame distribution has no dependency on YOLO result types or TensorRT.

## C++ TensorRT application

Requirements:

- CMake 3.18 or newer
- A C++17 compiler
- CUDA
- TensorRT 10
- OpenCV built with CUDA modules

Build and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTensorRT_ROOT=/path/to/TensorRT
cmake --build build --config Release
./build/yolo_trt_flow config.example.yaml
```

With a multi-config Windows generator, the executable is commonly located at:

```powershell
.\build\Release\yolo_trt_flow.exe config.example.yaml
```

Copy and edit `config.example.yaml` to select the engine, source, and
`pipeline.mode` (`offline` or `realtime`). `pipeline.queue_depth` applies only
to offline mode. Missing mode defaults to offline. This version uses one worker;
legacy `pipeline.workers` values other than 1 produce a warning and are ignored.

To simulate a camera with a local video, set `input.fake_camera: 1`.
This forces realtime mode and queue capacity 1, regardless of `pipeline.mode`
and `pipeline.queue_depth` (an explicit offline mode produces a warning).
The reader publishes at the video's recorded FPS,
with the first frame sent immediately. Invalid/missing video FPS is reported
as an error. Playback ends at EOF; it does not loop or skip source frames.
If decoding or a blocking subscriber is slow, playback slows without a catch-up
burst. This is a pacing simulator, not a hard realtime camera emulator.
`fake_camera` defaults to 0: input is unpaced and follows `pipeline.mode`.
`output.fps` only controls the saved video's FPS, not the input rate.

Natural end of input drains pending tasks. Pressing `q` cancels pending input,
allows the current inference to finish, and consumes available results before
joining the worker. A stopped pipeline must be recreated to run again.

Tests and callback exercise without GPU dependencies:

```bash
cmake -S . -B build-tests -DYOLO_BUILD_GPU=OFF -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
./build-tests/callback_demo
```

With the normal GPU build, `ctest --test-dir build --output-on-failure` also
checks configuration, FrameBus fan-out, and rendering without running inference.

## Python CPU baseline

```bash
python -m pip install -r requirements.txt
python cpu_demo.py yolov8s.pt data/passby.mp4
```

The demo runs on CPU with batch size 1 and processes frames serially. Use the
original `.pt` weights: the ONNX produced by `export-det.py` contains a
TensorRT-only EfficientNMS node and is not intended for CPU inference.

## Model export

```bash
python export-det.py --weights yolov8s.pt --sim
trtexec --onnx=yolov8s.onnx --saveEngine=yolov8s.engine
```

TensorRT engines are generally tied to the CUDA, TensorRT, and GPU environment
in which they are built. Rebuild the engine on the deployment machine when
those environments differ.

## Version history

### v0.1.0

- Provides one configurable C++ TensorRT inference application.
- Adds YAML configuration for workers, queues, input, output, and benchmarking.
- Adds a minimal serial Python CPU baseline.

### v0.0.9

- Experimental version before the project restructuring.
- Included separate OpenCV CPU, single-threaded TensorRT, and multi-worker
  TensorRT examples.
- Examples were built independently and used a mixture of hard-coded and
  command-line options.

## License and acknowledgments

Copyright (C) 2026 Jack-Z1858.

This project is licensed under the
[GNU Affero General Public License v3.0](LICENSE).

Portions are derived from
[triple-Mu/YOLOv8-TensorRT](https://github.com/triple-Mu/YOLOv8-TensorRT),
licensed under the MIT License. The Python utilities use
[Ultralytics](https://github.com/ultralytics/ultralytics), which has its own
licensing terms. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the
required notices and details.
