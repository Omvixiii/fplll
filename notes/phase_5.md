# Phase 5：CI、性能评估与隔离

状态：完成（2026-10-21）

## CMake 构建边界

- `CMakeLists.txt` 在 `project()` 前拒绝 in-source build；`cmake -S . -B .` 已验证会失败。CMake CLI 在执行项目脚本前仍会创建初始 `CMakeCache.txt`/`CMakeFiles/`，本次拒绝测试留下的两个产物已移除；日常配置使用固定 Binary Directory 的 Preset。
- [CMakePresets.json](../CMakePresets.json) 固定 Ninja Binary Directory 为 `build/<preset>`，安装前缀为 `_install/<preset>`。Preset 使用 C++23、`ccache` launcher 和 8 个 build jobs。
- 配置头 `fplll/fplll_config.h`、`config.h` 和生成的顶层 `fplll.h` 都写入 Binary Directory。库统一输出到 `lib/<CONFIG>/`，CLI 和测试程序输出到 `bin/<CONFIG>/`。
- 每个 CTest 在 `test-work/<test-name>/` 下运行。BKZ 测试使用相对文件名写入其隔离工作目录，不再用 `tmpnam()` 创建 `/tmp` 文件。
- 使用公共 OBJECT library 编译核心源文件，再链接 shared/static library；可选 debug 和 variant library 也复用相应对象，避免 shared/static 重复编译。
- 已从不包含 `m4/`、`configure.ac`、`Makefile.am` 和 `autogen.sh` 的源码副本完成 CMake 配置及 42 步 Ninja 构建，证明 CMake 不调用 Autotools/m4。

## 原有构建功能映射

CMake 现在覆盖默认 shared/static `libfplll`、`fplll`、`latticegen`，测试用 `llldiff`，以及可显式构建的 `fpllld`、`fplllv`、debug CLI 和对应静态库；`dbg` 聚合目标对应 Autotools 的 debug 构建入口。选项覆盖 GMP/MPIR、MPFR、QD、Threads、atomic、递归枚举、枚举维度和外部枚举；15 项现有测试接入 CTest。安装规则、SONAME、pkg-config、CMake package 和 shared/static consumer 已在 Phase 4 建立，本阶段再次验证安装消费路径。

## CI 与验证

- [CMake CI](../.github/workflows/cmake.yml) 包含 GCC/Clang Debug/Release、no-parallel/QD/iterative feature matrix、ASan/UBSan 和安装 consumer。
- [Codecov workflow](../.github/workflows/codecov.yml) 和 [Conda workflow](../.github/workflows/conda.yml) 均使用 CMake；[Conda recipe](../conda.recipe/meta.yaml) 使用 CMake + Ninja 构建。
- GCC Debug：构建通过，完整 CTest 15/15。
- Clang Debug：构建通过，完整 CTest 15/15。
- QD-enabled：目标构建通过，`test_gso`、`test_pruner` 2/2。
- `fpllld`、`fplllv`、debug CLI、debug/static variant 目标均构建通过。
- GCC Release：构建及独立前缀安装通过；安装后的 shared/static consumer 均编译、链接并运行。
- 原 Autotools 基线已固定在 detached worktree `build/autotools-reference-worktree`，提交 `360cfc6b`。在该 worktree 内 `make check` 15/15，`make install` 成功；生成文件和安装前缀都留在 `build/` 下。旧提交的干净 out-of-source 编译曾因历史头文件相对路径失败，因此参考构建保留在隔离 worktree 内执行。
- `git diff --check` 通过。执行了 `git status --short` 和 `git clean -ndX` 预览；预览列出已有的忽略型 Autotools 生成文件以及 `build/`、`_install/`，没有执行删除。

## 构建耗时对照

在相同 GCC 16.2、C++23、Debug、`-Wall`、QD 关闭、测试目标关闭、4 并行设置下，记录一次全新配置和完整默认库/CLI 构建：

| 构建系统 | 配置耗时 | 完整构建耗时 |
| --- | ---: | ---: |
| CMake + Ninja | 0.42 秒 | 66.26 秒 |
| Autotools | 5.21 秒 | 131.81 秒 |

Autotools 默认源列表还编译并行枚举维度 130–160；本次配置的最大并行枚举维度是 120，CMake 因此只编译 20–120。该差异符合各自配置，但计时不是完全相同的编译源集合。此次计时按 4 并行完成；随后将常用 Preset 和 CI 编译 jobs 调整为 8，没有为此再执行全量重编。

Conda 没有安装在当前环境，Conda recipe 通过了 Jinja 展开后的 YAML 解析，实际打包交由新增 CI job 验证。MPIR 和外部枚举库在本地环境也不可用；相应配置入口已实现，见 Phase 4 的限制记录。
