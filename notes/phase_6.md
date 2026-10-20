# Phase 6：文档与审核材料

状态：本地交付完成（2026-10-21）

## 交付内容

- `README.md` 增加 CMake + Ninja Preset 使用说明：C++23、工具依赖、固定构建/安装位置、Debug/Release、Clang、QD、枚举选项和 sanitizer presets。Autotools 使用说明保留为迁移期的旧入口。
- [`cmake-migration.md`](cmake-migration.md) 汇总 Autotools 与 CMake 的依赖检测、配置头、目标、选项、测试、安装和 CI 对应关系，并记录本地未具备的可选依赖。
- 文档提交为 `63874a9c`；本报告随 Phase 6 一并提交。

## 验证

- `git diff --check` 通过。
- `cmake --list-presets` 能读取全部十个构建配置。
- CMake、Codecov、Conda 工作流 YAML 及 Conda recipe 的 Jinja 展开结果均通过 YAML 解析。
- 本阶段只改文档，没有重复编译；Phase 5 已完成 GCC/Clang 完整 CTest、QD 选项测试、安装 consumer 和无 m4 源码副本构建验证。
- 将原 Autotools 基线固定到独立 worktree 后，删除了 `git clean -ndX` 预览确认的根目录 Autotools bootstrap 产物。构建缓存、测试安装和 uv 环境仍保留在 `build/` 与 `_install/`；当前源码根目录不再有这些 Autotools 生成文件。

本阶段只交付本地文档与 Git 提交，没有推送分支或创建远端 PR。
