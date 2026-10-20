# Phase 3：配置选项与 CTest

状态：完成（2026-10-20）

## 实现

- 将 `tests/Makefile.am` 中的 15 个测试注册为同名 CMake/CTest targets。
- 每个测试通过 `TESTDATADIR` 指向源码树根目录，确保 out-of-tree 测试能定位 `tests/lattices` 和策略数据。
- 配置头现在根据 CMake 选项生成 QD、GMP/MPIR、递归枚举、并行枚举维度和外部枚举宏。
- 新增 `FPLLL_WITH_QD`、`FPLLL_USE_MPIR`、`FPLLL_WITH_RECURSIVE_ENUM`、`FPLLL_MAX_ENUM_DIM`、`FPLLL_MAX_PARALLEL_ENUM_DIM` 和外部枚举库/函数配置项。
- QD 支持通过 pkg-config 查找，缺少 pkg-config 元数据时回退到头文件和库查找；并行枚举关闭时不编译并行枚举源。
- 增加 `std::atomic<int64_t>` 链接检测；仅在默认链接失败时增加 `libatomic`。
- 增加 `gcc-debug-no-parallel`、`gcc-debug-qd`、`gcc-debug-iterative` 配置/构建 Presets。

## 验证

默认 `gcc-debug` 配置下，15 个测试目标全部构建成功。完整 CTest 有 14 项当次通过；`test_bkz` 在受限沙箱内尝试写 `/tmp/file...` 时被系统拒绝。获准在沙箱外仅重跑该用例后，`test_bkz` 通过，因此 15 个测试均已验证通过。所有使用 lattice fixtures 的测试均通过，证明 `TESTDATADIR` 指向正确。

特性配置验证：

| Preset / 配置 | 构建 | 选定 CTest |
| --- | --- | --- |
| `gcc-debug-qd`（QD 2.3.23） | 74/74 步骤通过 | `test_nr`、`test_pruner`、`test_gso`：3/3 通过 |
| `gcc-debug-no-parallel`（最大并行维度 0） | 62/62 步骤通过 | `test_enum`、`test_svp`：2/2 通过 |
| `gcc-debug-iterative`（递归枚举关闭） | 74/74 步骤通过 | `test_enum`：1/1 通过 |

GMP/MPFR/pthread/atomic 默认配置和 QD 开关已在 Fedora 环境验证。当前环境没有 MPIR 头文件/库，也没有外部枚举库，因此这两种可选依赖的启用链接路径尚未实测，相关 CMake 配置项已实现。

## 已知提示

`test_bkz.cpp` 链接时会出现既有 `tmpnam` 安全警告；这与 Autotools 基线中的测试代码相同，不影响测试结果。完整 CTest 在沙箱内运行时，`test_bkz` 需要允许写入 `/tmp`。
