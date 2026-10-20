# Phase 4：安装、ABI 与下游兼容性

状态：完成（2026-10-20）

## 安装与 ABI

- `gcc-release` Preset 把安装前缀设为 `_install/cmake-release`，安装库目录设为 `lib`，与 Autotools 参考安装布局一致。
- CMake 同时安装 shared `libfplll.so.9.0.0`、SONAME 链接 `libfplll.so.9` 和 static `libfplll.a`。
- `readelf -d` 确认 Autotools 与 CMake shared library 的 SONAME 都为 `libfplll.so.9`。
- 对两份库使用 `nm -D --defined-only` 比较强导出（`T`/`D`/`B`）：97 个符号名称完全一致；`B`、`D`、`T`、`V`、`u` 类型计数也一致。只有弱符号 `W` 集不同（Autotools 17,690，CMake 17,343），主要是模板/标准库实例化；未发现强 ABI 符号缺失或新增。
- 去除安装前缀后，两边的 CLI、头文件、静态/共享库、pkg-config 和策略文件路径一致。Autotools 额外提供 Libtool 专属 `libfplll.la`；CMake 额外提供 package config、version 和 exported targets 文件。

## CMake package 与 pkg-config

- 安装 `fplllConfig.cmake`、版本文件和 `fplllTargets.cmake`，导出 `fplll::fplll` 与 `fplll::fplll_static`。
- exported targets 传播 C++23 编译特性、头文件路径和 GMP/MPFR/Threads 依赖；QD、MPIR、外部枚举依赖由 package config 按构建选项重新发现。
- 修正安装头文件对 `fplll_config.h` 的搜索路径，并让 CMake/Autotools 的 `fplll.pc` 提供 `include/fplll` 和 `-std=c++23`。
- CMake consumer 项目仅从安装前缀 `find_package(fplll 5.5 CONFIG)`，shared 与 static consumer 均编译、链接并运行成功；对 QD-enabled 安装前缀也完成 shared/static consumer 验证。
- pkg-config 读取版本 `5.5.0`，并返回安装前缀下的头文件、库目录及 C++23 编译参数。安装 package metadata 中没有源码树或构建树路径。

## fpylll 下游验证

使用 uv 在 `build/fpylll-venv-uv/` 创建 CPython 3.14.8 隔离环境，并把 CMake 安装内容装入该环境前缀。使用该安装库源码构建 fpylll 0.6.4，安装运行依赖 cysignals 1.13.1；`import fpylll` 和一个 `IntegerMatrix` + `LLL.reduction` 调用成功。系统 Python 没有安装或升级任何包。

第一次尝试使用 Fedora 系统 Python 时发现缺少 `Python.h`；随后改用 uv 管理、位于 `build/uv-python/` 的 Python 发行包完成构建。fpylll 上游 [setup.py](https://github.com/fplll/fpylll/blob/master/setup.py) 固定加入 `-std=c++11`，再追加环境中的 `CXXFLAGS`；验证时通过该参数覆盖为项目要求的 `-std=c++23`，并提供安装前缀的头文件与库搜索路径。

## 限制

当前环境没有 MPIR 或外部枚举库，因此这些可选依赖的安装 consumer 路径尚未实测；GMP、MPFR、Threads、QD、shared/static、pkg-config 和 fpylll 路径均已验证。
