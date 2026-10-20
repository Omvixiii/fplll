# Phase 2：CMake 核心构建

状态：实现与验证完成（2026-10-20）

## 实现内容

- 根目录 `CMakeLists.txt` 使用 CMake 3.20+，全项目强制 C++23，并以 imported targets 传播 GMP、MPFR 和 Threads 依赖。
- 在构建目录生成 `config.h` 和 `fplll/fplll_config.h`；当前宏配置对应无 QD、递归枚举开启、最大枚举维度 256、并行枚举维度 120。
- 构建共享库 `libfplll`，版本 `9.0.0`、SONAME `9`，包含 Autotools 默认核心源和并行枚举源。
- 构建 `fplll`、`latticegen` 和辅助程序 `llldiff`。`llldiff` 对应 Autotools 的 `check_PROGRAMS`，因此不安装。
- 增加 `CMakePresets.json` 的 `gcc-debug` Configure/Build Preset，生成文件放入 `build/gcc-debug/`，并生成 `compile_commands.json` 供 clangd 使用。
- 增加基础安装规则：库、两个安装 CLI、Autotools 公共头文件、策略文件和 `fplll.pc`。

## 验证结果

已运行：

```text
cmake --preset gcc-debug
cmake --build --preset gcc-debug --parallel 8
cmake --install build/gcc-debug --prefix _install/cmake-debug
```

配置成功，Ninja 构建的 48 个步骤全部成功；`fplll` 和 `latticegen` 报告版本 `5.5.0`。共享库 SONAME 为 `libfplll.so.9`，pkg-config 可读取版本 `5.5.0`。CMake 的实际编译命令使用 `-std=c++23`。Autotools 的 C++23 检测宏通过 Autoconf 展开检查。

## 范围说明

测试注册、QD/MPIR/外部枚举选项、静态库与调试变体、CMake package config、下游消费者和 CI 仍属于后续阶段。阶段 2 的安装结果是迁移基准，不代表 Phase 4 的 ABI 与下游兼容性验收已经完成。
