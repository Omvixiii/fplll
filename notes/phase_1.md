# Phase 1：Autotools 基线

状态：完成（2026-10-20）

基线工作树起点为 `a48096bb`（`upstream/master`）。现有的 out-of-tree Autotools 参考构建位于 `build/autotools-reference/`，独立安装前缀位于 `_install/autotools-reference/`。

## 配置与功能基线

`config.status --config` 记录的配置为：

```text
--prefix=/home/fedora/fplll/_install/autotools-reference --without-qd
```

为支持该 out-of-tree 基线，`fplll/Makefile.am` 将构建目录中的 `fplll/` 加入头文件搜索路径，使库源码能找到生成的 `fplll_config.h`。

`config.log` 记录该历史基线使用 G++ `-std=c++11`，依赖 GMP、MPFR 和 pthread；配置头启用递归枚举，最大枚举维度为 256，并行枚举维度为 120。未启用 QD、MPIR 或外部枚举库。用户随后明确要求最低标准从 C++23 开始，因此 C++11 仅作为本次原 Autotools 基线的历史记录；迁移后的构建配置统一要求 C++23。

## 测试与安装结果

`build/autotools-reference/tests/test-suite.log` 汇总为 15 个测试全部通过：15 PASS、0 FAIL、0 ERROR、0 SKIP。

参考安装前缀包含：

- `libfplll.so.9.0.0`、SONAME 链接 `.so.9`、开发链接 `.so`，以及静态库 `libfplll.a`；
- 公共头文件和生成的 `fplll_config.h`；
- `fplll.pc` 与 `share/fplll/strategies/default.json`；
- `fplll` 和 `latticegen` 两个安装 CLI。

`llldiff` 在 Autotools 中属于 `check_PROGRAMS`，不是安装 CLI；CMake 会构建该辅助目标，但不将其安装到 `bin/`。

## 验收结论

Autotools 基线配置、15 项测试和独立前缀安装均有现存日志与安装文件作为证据，符合阶段验收标准。CMake 的功能映射以这些结果为对照；性能测量和跨编译器基准留到 Phase 5。
