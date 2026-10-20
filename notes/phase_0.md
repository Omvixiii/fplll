# Phase 0：开发环境准备

状态：完成（2026-10-20）

## 环境核对

当前工作区运行在 Fedora Linux 44（WSL2）上。以下工具均可从项目终端调用：

| 工具 | 版本 |
| --- | --- |
| GCC / G++ | 16.2.1 |
| Clang / clang++ | 22.1.8 |
| CMake | 4.3.0 |
| Ninja | 1.13.2 |
| GDB | 17.2 |
| clangd | 22.1.8 |
| GMP | 6.3.0 |
| MPFR | 4.2.2 |

VS Code 当前通过远程 Linux 工作区连接本仓库；clangd 已安装，可用于代码导航和诊断。

## 目录约定

`build/` 和 `_install/` 已加入根目录 `.gitignore`，后续配置、编译、测试和安装均使用这两个目录。根目录仍保留 Autotools 的 `configure`、`Makefile.in` 等引导文件，因为迁移期间两套构建系统并行；它们不是某次编译生成的对象文件或可执行文件。

## 验证

已运行版本检查：

```text
gcc --version
clang --version
cmake --version
ninja --version
gdb --version
clangd --version
pkg-config --modversion gmp mpfr
```

所有命令成功，依赖版本分别为 GMP 6.3.0、MPFR 4.2.2。
