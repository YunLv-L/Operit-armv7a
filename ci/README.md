# CI checks

`script/` 存放可在本地复现的检查脚本，GitHub Actions 只负责编排检查和构建。

## Local checks

在仓库根目录运行：

```bash
python3 ci/script/check_repo_hygiene.py
python3 ci/script/check_markdown_links.py
python3 ci/script/check_localizations.py
npm --prefix web-chat run typecheck
```

CI 在 PR 中会使用目标分支和 PR head 的 commit 对比，只检查本次改动相关的文件；不传 commit 参数时，脚本检查当前工作树。

## CI layers

- `CI required`：PR 的聚合门禁
- `Repository hygiene`：差异、语法、冲突标记和本地 Markdown 链接
- `Localization`：Android 字符串资源结构、key、占位符和语言提示
- `WebChat checks`：TypeScript 类型检查和 WebChat 构建
- `Android Build`：涉及 Android 或构建输入时执行完整构建
- `Documentation advisory`：Markdown 风格和拼写提示，不阻断合并
