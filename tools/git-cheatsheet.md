# Git 速查（单人本地开发）

> 针对 `six_robot` 这种"一个人一台电脑、本地存档"的用法。
> 不用管分支、远程、协作 —— 那些等需要了再学。

---

## 一、先记住三个区

```
   你编辑的文件              git add              git commit
  ┌──────────────┐       ┌──────────────┐      ┌──────────────┐
  │    工作区     │ ────→ │    暂存区     │ ────→ │  仓库（历史） │
  │ Working Dir  │       │   Staging    │      │  Repository  │
  └──────────────┘       └──────────────┘      └──────────────┘
         ↑                                             │
         └───────────── git restore ───────────────────┘
                    （从存档点取回）
```

- **`git add`** = 挑出"这次要存档的东西"
- **`git commit`** = 打包，生成一个可以随时回来的**存档点**
- **每个 commit 就是一个存档点**，这是 git 唯一真正重要的概念

> **为什么中间要有个"暂存区"？** 因为可以只提交一部分改动。
> 单人小项目基本用不上，直接 `git add -A` 全提交就行。

---

## 二、日常 95% 就这五条

| 命令 | 作用 |
|---|---|
| `git status` | 现在有哪些改动 |
| `git diff` | 具体改了什么（逐行） |
| `git add -A` | 把所有改动放进暂存区 |
| `git commit -m "说明"` | 存档 |
| `git log --oneline` | 看存档历史 |

**典型流程：**

```powershell
cd 'D:\new stm32\six_robot'

git status                    # 1. 看看改了啥
git diff                      # 2. 确认改动是对的
git add -A                    # 3. 全部放进暂存区
git commit -m "调平环加上积分项"   # 4. 存档
```

---

## 三、场景对照表（最实用的一节）

### 我想看看现在什么情况

| 我想… | 命令 |
|---|---|
| 看有哪些文件改了 | `git status` |
| 看具体改了什么内容 | `git diff` |
| 看已经 `add` 但还没提交的内容 | `git diff --staged` |
| 看提交历史 | `git log --oneline` |
| 看提交历史（带分支线） | `git log --oneline --graph --all` |
| 看最近一次提交改了哪些文件 | `git show --stat HEAD` |
| 看最近一次提交的完整改动 | `git show HEAD` |

### 我想存档

| 我想… | 命令 |
|---|---|
| 把所有改动都存档 | `git add -A` 然后 `git commit -m "说明"` |
| 只存档某几个文件 | `git add User/leg_gait.c User/balance.c` 然后 commit |
| 存档已跟踪文件的改动（跳过新增文件） | `git commit -am "说明"` |
| 一次只存档文件的一部分改动 | `git add -p`（交互式，会逐块问你） |

### 我想"后悔"（**最常用、最值钱**）

| 我想… | 命令 | 影响范围 |
|---|---|---|
| 某个文件改坏了，丢回上次提交的样子 | `git restore User/leg_gait.c` | 一个文件 |
| 所有未提交的改动全丢，回到上次提交 | `git restore .` | 全部工作区 |
| `add` 错了，从暂存区拿出来（改动保留） | `git restore --staged User/leg_gait.c` | 暂存区 |
| 刚提交完发现不对，退掉这次提交（**改动保留**） | `git reset --soft HEAD~1` | 历史 |
| 刚提交完发现不对，退掉并**丢弃改动** | `git reset --hard HEAD~1` | 历史 + 工作区 |
| 退回更早的某个存档点 | `git reset --hard <提交号>` | 历史 + 工作区 |

> **`--soft` 和 `--hard` 的区别：**
> - `--soft`：只退"存档记录"，你的代码一个字不动 —— **安全，常用**
> - `--hard`：连代码一起丢 —— **危险，会永久丢失未提交的改动**

> **老教程里会写 `git checkout -- <文件>`，新版是 `git restore <文件>`，效果一样。**

### 我想对比

| 我想… | 命令 |
|---|---|
| 和上次提交比 | `git diff HEAD` |
| 和上上次提交比 | `git diff HEAD~1` |
| 两个提交之间比 | `git diff <提交号A> <提交号B>` |
| 只要文件名，不要内容 | `git diff --stat` |

### 我想找回丢失的东西

| 我想… | 命令 |
|---|---|
| 看所有操作记录（包括被 reset 掉的） | `git reflog` |
| 找回被 `reset --hard` 丢掉的提交 | `git reflog` 找到提交号，再 `git reset --hard <提交号>` |

> **`git reflog` 是最后一道保险 —— 只要提交过一次，几乎永远找得回来。**
> 但**没提交过的**改动，`reset --hard` 之后就真没了。

---

## 四、里程碑：打标签

调好一个能跑的版本，想标记一下：

```powershell
git tag -a v1.0-调平可用 -m "调平环 + 积分，直线能走直"
git tag                  # 列出所有标签
git show v1.0-调平可用    # 看这个标签对应的提交
```

以后想回到那个版本：

```powershell
git checkout v1.0-调平可用        # 切过去看看（只读状态）
git switch -                       # 切回来
```

或者干脆：

```powershell
git reset --hard v1.0-调平可用     # 真的退回去（丢弃之后的提交）
```

---

## 五、`.gitignore` 相关

| 我想… | 命令 |
|---|---|
| 看哪些文件被忽略了 | `git status --ignored` |
| 看某个文件为什么被忽略 | `git check-ignore -v 文件名` |
| **强制添加**一个被忽略的文件 | `git add -f MDK-ARM/six_robot/six_robot.hex` |
| 已跟踪的文件想改成忽略 | 先从 `.gitignore` 加规则，再 `git rm --cached 文件名` |

> **注意**：`.gitignore` 只对**还没被跟踪**的文件生效。
> 一个文件一旦提交过，之后再写进 `.gitignore` 是没用的 —— 必须先 `git rm --cached`。

---

## 六、三个"危险动作"

| 命令 | 风险 |
|---|---|
| `git reset --hard` | **丢弃未提交的改动，找不回来**。执行前先 `git status` 确认没有想留的东西 |
| `git rm -r --cached .` | 把暂存区清空。常用技巧但容易误伤，别乱敲 |
| 删 `.git` 目录 | **历史全没了**。这个目录就是整个仓库，删了就真没了 |

**安全习惯：**

1. 干任何"可能丢东西"的操作前，先跑一次 `git status`
2. 拿不准就先 commit 一次 —— **有存档点就永远能回来**
3. 调参之前先 commit —— 这是 git 对你最大的价值

---

## 七、给这个项目的工作流建议

### 调参之前

```powershell
git add -A
git commit -m "调参前：PID Kp=0.22 这版能跑"
```

**这样万一调崩了，`git restore .` 一秒回到能跑的状态。** 比手动备份文件可靠得多。

### 一个功能做完

```powershell
git add -A
git commit -m "调平环：加积分项，静差从 4° 降到 1°"
```

**写清楚"改了什么"和"效果如何"** —— 三个月后自己看得懂才有用。

### 调崩了

```powershell
git status                    # 先看改了哪些
git restore User/leg_gait.c   # 只还原这一个文件
# 或者
git restore .                 # 全部还原
```

### 想试试一个激进的想法

```powershell
git commit -am "存档：当前能跑的版本"   # 先存档
# ...大胆改...
git restore .                          # 不行就全丢，回到刚才的存档
```

**这就是 git 对你这个阶段最大的价值：可以放心大胆地试。**

---

## 八、命令速记卡

```
看：
  git status                      当前状态
  git diff                        改了什么
  git log --oneline               历史
  git show HEAD                   最近一次改了啥

存：
  git add -A                      全部暂存
  git commit -m "说明"             存档
  git commit -am "说明"            已跟踪文件，一步到位

退：
  git restore <文件>               单文件还原
  git restore .                   全部还原
  git restore --staged <文件>      从暂存区拿出来
  git reset --soft HEAD~1         退掉上次提交，保留改动
  git reset --hard HEAD~1         退掉上次提交，丢弃改动

找：
  git reflog                      救命日志
  git checkout <提交号>            跳到某个存档点看看
  git switch -                     跳回来
```
