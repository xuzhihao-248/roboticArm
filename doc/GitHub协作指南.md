# GitHub SSH 连接与多人协作指南
##  此文档的作用是为了快速在本地建立git工作流程方便后续开发的方便。文件中记录了我如何使用ssh协议连接github和已经解决的问题（clashverge抢占端口）文档中同时给出了一些常见的git命令。后续我会继续整理出一个更简单的txt命令文件方便直接复制粘贴。
## 本文末尾有一些常见的git指令和使用场景，不能解决的问题直接ai


## 用户清单
### xuzhihao
user.name "xuzhihao-248"

user.email "xu_zhihao@outlook.com"

## 快速配置清单

| 步骤 | 命令 |
|------|------|
| 1. 生成密钥 | `ssh-keygen -t ed25519 -C "邮箱"` |
| 2. 复制公钥 | `Get-Content ~\.ssh\id_ed25519.pub \| Set-Clipboard` |
| 3. 添加到 GitHub | 打开 github.com/settings/keys 粘贴 |
| 4. 检查 Clash 冲突 | `nslookup github.com` 看是否返回 `198.18.x.x` |
| 5. 如有冲突，配置 SSH | 创建 `~\.ssh\config` 走 443 端口（见第三章） |
| 6. 测试连接 | `ssh -T git@github.com` |
| 7. 克隆仓库 | `git clone git@github.com:用户名/仓库名.git` |

## 一、初次配置：SSH 连接 GitHub

### 1. 生成 SSH 密钥

在 PowerShell 中执行：

```powershell
ssh-keygen -t ed25519 -C "你的GitHub邮箱@example.com"
```
bash同理

一路回车，密钥保存在 `C:\Users\你的用户名\.ssh\id_ed25519`。

### 2. 复制公钥

```powershell
Get-Content ~\.ssh\id_ed25519.pub | Set-Clipboard
```

```bash
cat ~/.ssh/id_ed25519.pub
```

### 3. 添加到 GitHub（添加到自己的github里）

打开 https://github.com/settings/keys → 点击 **New SSH key** → Title 随意填 → Key 粘贴公钥 → 点击 **Add SSH key**。

### 4. 测试连接

```powershell
ssh -T git@github.com
```

看到 `Hi xxx! You've successfully authenticated` 表示配置完成。

---

## 二、判断 Clash Verge 是否与 GitHub 端口冲突（这个是我真实遇到的问题，我的pc和mac都遇到了端口被clashverge抢占的问题——已解决）

### 测试：查看 GitHub 解析的 IP

```powershell
nslookup github.com
```

| 返回 IP | 状态 |
|----------|------|
| `140.82.xxx.xxx` | 正常，无冲突 |
| `198.18.0.18` | 冲突，Clash TUN 模式劫持了 DNS |

`198.18.x.x` 是 Clash 代理内网地址，SSH 流量被拦截导致认证失败。

### 验证 SSH 连接的目标地址

```powershell
ssh -vT git@github.com 2>&1 | Select-String "Connecting to"
```

- 看到 `Connecting to github.com [198.18.x.x] port 22` → 冲突，被代理拦截
- 看到 `Connecting to ssh.github.com [140.82.xxx.xxx] port 443` → 正常，已绕过代理

---

## 三、解决方案：绕过 Clash 代理

如果确认 Clash 冲突，让 SSH 走 443 端口（代理通常不拦截 HTTPS 端口）。

### 一键配置（复制粘贴到 PowerShell 执行）

```powershell
@'
Host github.com
    Hostname ssh.github.com
    Port 443
    User git
'@ | Out-File -FilePath "$env:USERPROFILE\.ssh\config" -Encoding ASCII
```

```bash
mkdir -p ~/.ssh
cat >> ~/.ssh/config << 'EOF'
Host github.com
    Hostname ssh.github.com
    Port 443
    User git
EOF
```

执行后再验证：

```powershell
ssh -T git@github.com
```
得到结果应该同上
---

## 四、仓库管理员：邀请协作者

仓库页面 → **Settings** → **Collaborators** → **Add people**，输入对方的 GitHub 用户名或邮箱。

---

## 五、多人协作日常流程

### 工作前同步最新代码

```powershell
git checkout main
git pull origin main
```

### 创建功能分支开发

```powershell
git checkout -b feat/功能名称
```

### 提交改动

```powershell
git add .
git commit -m "描述做了什么改动"
```

### 推送到 GitHub

```powershell
git push origin feat/功能名称
```

### 发起 Pull Request

在 GitHub 仓库页面创建 PR，从 `feat/功能名称` 合并到 `main`。

队友可 review 代码、讨论修改，通过后点击 **Merge**。

### PR 合并后同步

```powershell
git checkout main
git pull origin main
```

---

## 六、常用 Git 命令

| 场景 | 命令 | 说明 |
|------|------|------|
| 克隆仓库 | `git clone <url>` | 第一次下载项目 |
| 查看状态 | `git status` | 查看哪些文件被修改/暂存 |
| 添加文件 | `git add <file>` | 将文件加入暂存区 |
| 添加所有 | `git add .` | 添加当前目录所有改动 |
| 提交 | `git commit -m "消息"` | 生成一个本地版本 |
| 推送 | `git push origin <分支>` | 上传到远程仓库 |
| 拉取 | `git pull origin <分支>` | 拉取并合并远程更新 |
| 获取远程更新 | `git fetch origin` | 只拉取不合并，查看后再手动合并 |
| 查看分支 | `git branch` | `-r` 看远程分支，`-a` 看所有分支 |
| 切换分支 | `git checkout <分支>` | 切换到已有分支 |
| 创建并切换 | `git checkout -b <新分支>` | 从当前分支创建新分支 |
| 合并分支 | `git merge <分支>` | 将指定分支合并到当前分支 |
| 查看提交历史 | `git log --oneline --graph` | 一行显示，带分支图 |
| 查看文件修改历史 | `git log -p <文件>` | 显示每次改动的具体内容 |
| 查看谁改了什么 | `git blame <文件>` | 显示每一行是谁最后一次修改 |
| 比较差异（工作区） | `git diff` | 查看工作区和暂存区的差异 |
| 比较差异（暂存区） | `git diff --staged` | 查看暂存区和上次提交的差异 |
| 撤销工作区修改 | `git checkout -- <文件>` | 放弃修改（危险） |
| 撤销暂存 | `git reset HEAD <文件>` | 移出暂存区，保留修改 |
| 撤销提交（保留修改） | `git reset --soft HEAD~1` | 撤销 commit，修改保留 |
| 撤销提交（完全删除） | `git reset --hard HEAD~1` | 完全删除上次提交（危险） |
| 临时保存修改 | `git stash` | 暂存当前未提交的修改 |
| 恢复临时修改 | `git stash pop` | 恢复最近一次 stash 的修改 |
| 查看远程仓库 | `git remote -v` | 显示 origin 的地址 |
| 修改远程地址 | `git remote set-url origin <新url>` | 更换仓库地址 |
| 解决冲突 | 手动编辑冲突文件 → `git add .` → `git commit` | 合并冲突后的标准流程 |


