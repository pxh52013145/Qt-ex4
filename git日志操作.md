# Git 日志拆分与提交时间修改操作指南

本文记录“把一个大提交拆成多次过程提交”以及“把提交时间从 2025-12-22 20:00 逐步改到 2026-01-02”的具体做法。两类操作都属于 **重写历史（rewrite history）**，会导致提交哈希变化，需配合强推（force push）。

---

## 重要提醒（必读）

1. **重写历史会改变提交哈希**：任何已拉取旧历史的人都需要重新同步（`reset --hard` 或重新 clone）。
2. **先做远程备份分支**：防止误操作导致历史丢失。
3. **确保工作区干净**：开始前执行 `git status`，不要有未提交修改。
4. **建议在只有你自己使用的仓库/分支上做**：多人协作分支慎用。

---

## 一、把一个大提交拆成多次“过程提交”

### 目标
将原本“一次提交所有文件”的情况，拆分为多个步骤提交（例如 `step1~step9`），让 GitHub 提交记录更符合实验过程。

### 1）备份当前 main（本地 + 远程）

```bash
# 本地备份分支（指向当前 main）
git branch backup/main-before-rewrite

# 推送备份到远程（需要能访问 GitHub）
git push origin backup/main-before-rewrite
```

> 如果你网络环境访问 GitHub 需要代理，可临时：
>
> ```powershell
> $env:HTTPS_PROXY='http://127.0.0.1:7785'
> $env:HTTP_PROXY='http://127.0.0.1:7785'
> ```

### 2）用 orphan 分支“重建一条全新的历史”

orphan 分支没有父提交，相当于从 0 开始重做提交链。

```bash
git switch --orphan rewrite-main
```

### 3）把旧 main 的内容恢复到工作区（但不保留旧提交历史）

```bash
# 把旧 main 的文件内容恢复到当前工作区/暂存区
git restore --source main --worktree --staged .
```

此时文件会处于 staged 状态（`git status` 会看到很多 `A`），我们需要取消暂存，准备分批提交：

```bash
# 取消暂存（在 orphan 分支没有 HEAD 时，常用这种方式）
git rm -r --cached .
```

### 4）按“实验步骤”分批 add + commit

下面是本项目拆分示例（你也可按自己需要调整分组）：

```bash
# step1：subdirs 工程骨架
git add .gitignore ex4.pro server/server.pro client/client.pro
git commit -m "step1: 创建 subdirs 工程骨架"

# step2：协议头文件
git add common/protocol.h
git commit -m "step2: 添加按行 JSON 协议定义"

# step3：服务器端通讯核心
git add server/chatserver.cpp server/chatserver.h server/clientworker.cpp server/clientworker.h
git commit -m "step3: 实现服务器端多线程通讯核心"

# step4：服务器端 UI
git add server/serverwindow.cpp server/serverwindow.h server/serverwindow.ui server/main.cpp
git commit -m "step4: 完成服务器端 UI 与日志显示"

# step5：客户端网络层
git add client/chatclient.cpp client/chatclient.h
git commit -m "step5: 实现客户端网络层（连接/收发/解析）"

# step6：客户端 UI
git add client/clientwindow.cpp client/clientwindow.h client/clientwindow.ui client/main.cpp
git commit -m "step6: 完成客户端 UI（聊天/在线用户/退出确认）"

# step7：一键启动脚本
git add run-demo.bat scripts/run-demo.ps1
git commit -m "step7: 添加一键启动脚本 run-demo"

# step8：VS Code 可选脚本
git add .vscode/tasks.json scripts/vscode-qmake.ps1
git commit -m "step8: 添加 VS Code 构建脚本（可选）"

# step9：README
git add README.md
git commit -m "step9: 添加 README 使用说明"
```

### 5）把新历史替换为 main 并强推到远程

```bash
# 将当前分支改名为 main
git branch -M main

# 强推（建议用 --force-with-lease 更安全）
git push --force-with-lease origin main
```

---

## 二、修改每个提交的时间（AuthorDate / CommitDate）

### 目标
把这 9 个 step 提交的时间从 `2025-12-22 20:00` 开始逐步增加，最终到 `2026-01-02`（例如晚 19:40 左右），使 GitHub 上显示的提交时间更“像真实实验过程”。

### 关键概念
- `AuthorDate`：作者写代码的时间（GitHub contributions 主要与作者时间相关）
- `CommitDate`：提交进入历史的时间

为了避免 GitHub 展示/统计不一致，通常 **两者一起改**。

### 1）再次备份当前 main

```bash
git branch backup/main-before-date-rewrite
git push origin backup/main-before-date-rewrite
```

### 2）列出需要改时间的提交列表（从旧到新）

```bash
git log --reverse --format="%H %s"
```

把输出的 9 个提交哈希复制下来，接下来要对每个哈希指定一个时间。

### 3）生成“方案 A”的时间序列（开始时间 + 逐步递增到结束）

你可以用 Python 生成 9 个时间点（示例）：

```python
import datetime as dt
from zoneinfo import ZoneInfo

start = dt.datetime(2025, 12, 22, 20, 0, tzinfo=ZoneInfo("Asia/Shanghai"))
end   = dt.datetime(2026,  1,  2, 19, 40, tzinfo=ZoneInfo("Asia/Shanghai"))
N = 9
delta = (end - start) / (N - 1)

for i in range(N):
    t = (start + delta * i).replace(second=0, microsecond=0)
    print(i + 1, t.isoformat())
```

> 你也可以不用均分：直接手动给每个 step 一个日期/时间（更自然）。

### 4）用 `git filter-branch --env-filter` 批量改时间

`--env-filter` 会对历史中的每个提交执行一段 shell 脚本，你可以通过判断 `$GIT_COMMIT`（当前被处理的旧提交哈希）来设置时间环境变量。

建议把脚本写到 `$GIT_DIR` 下面（Git 自己的目录），例如：`$GIT_DIR/commit_dates_env_filter.sh`，内容模板如下：

```sh
case "$GIT_COMMIT" in
  <old_hash_1>)
    export GIT_AUTHOR_DATE="2025-12-22 20:00:00 +0800"
    export GIT_COMMITTER_DATE="2025-12-22 20:00:00 +0800"
    ;;
  <old_hash_2>)
    export GIT_AUTHOR_DATE="2025-12-23 21:10:00 +0800"
    export GIT_COMMITTER_DATE="2025-12-23 21:10:00 +0800"
    ;;
  # ... 依次写完所有提交 ...
esac
```

然后执行重写（注意：这里的 `<old_hash_x>` 必须是 **重写前** 的提交哈希）：

```bash
git filter-branch -f --env-filter '. "$GIT_DIR/commit_dates_env_filter.sh"' -- main
```

### 5）验证时间已变化

```bash
git log --reverse --format="%h %ad %s" --date=iso-strict

# 或查看某个提交的 AuthorDate/CommitDate
git show -s --format=fuller HEAD
```

### 6）把改完时间的新历史强推到远程

```bash
git push --force-with-lease origin main
```

---

## 三、他人如何同步“被重写历史”的 main

如果别人已经 clone 过旧 main（或你自己另一台机器也拉过），推荐两种方式：

### 方式 A：重新 clone（最省事）
直接删掉旧目录，重新 `git clone`。

### 方式 B：强制对齐远端 main（会丢本地未提交改动）

```bash
git fetch --all
git switch main
git reset --hard origin/main
git clean -fd
```

---

## 四、这能“填满 GitHub contributions”吗？

**技术上可以让 contributions 图出现对应日期的提交点**，但是否显示/显示多少取决于 GitHub 的统计规则与设置：

### 会计入 contributions 的常见条件
- 提交的 **author email** 必须绑定到你的 GitHub 账号（`git config user.email` 要匹配或已添加到 GitHub）。
- 提交需要出现在仓库的 **默认分支**（通常是 `main`）或 `gh-pages`，或通过 PR 合并进入默认分支。
- 仓库不能是纯 fork 贡献（本仓库是自己创建的，通常没问题）。
- 贡献图只展示最近一段时间（通常是最近一年），更久的不会在图上显示。
- 显示的日期与 GitHub 个人资料里设置的时区有关。

### “填满”的含义与现实情况
如果你想让某一段时间每天都有绿色格子，至少需要做到：
- 那段时间里 **每天至少 1 个提交**（author date 落在那一天），并 push 到默认分支。

但不建议为了“刷图”批量造提交：会让历史失真、影响协作，也不符合贡献图的本意。更推荐把提交记录写成真实的实验过程（你现在的 step1~step9 就足够用于实验报告展示）。

