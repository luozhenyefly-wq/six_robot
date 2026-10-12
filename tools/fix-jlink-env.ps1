<#
=============================================================================
 fix-jlink-env.ps1   —— Keil 点下载后卡死/闪退 的 J-Link 环境回滚脚本
=============================================================================

 【它解决什么】
   Keil uVision 5.39 + J-Link 8.10f DLL 驱动 2012 年固件的克隆 J-Link，
   下载本身能成功，但收尾时卸载 JLinkARM.dll 会把 UV4.exe 带崩 / 卡死。

 【它做什么】
   把 C:\Keil_v5\ARM\Segger 恢复成 "Keil 5.39 官方配套" 的一致状态：
     1) JLinkARM.dll   (8.10f, 2024)  --> 备份出去
     2) JLinkARM_org.dll (7.92k, 2023) --> 复制成 JLinkARM.dll 顶上去
     3) JLinkDevices.ref (被改成指向 D:\jlink\... 的坏文件) --> 改名停用
   然后在备份目录里生成一个 restore.ps1，一条命令就能全部还原。

 【它不动什么】
   - 不碰注册表（那个 IgnoreFreeLibrary<JLinkA> 兼容性补丁只是"读出来报告"）
   - 不碰 D:\jlink（除非你显式加 -IncludeJLinkDir）
   - 不删除任何原文件（只 Copy 和 Rename，8.10f 会完整备份）

 【怎么用】
   1) 先关掉 Keil（脚本会检测，开着就直接退出）
   2) 先看计划，什么都不改：
        pwsh -File ".\tools\fix-jlink-env.ps1"
   3) 确认无误后真正执行：
        pwsh -File ".\tools\fix-jlink-env.ps1" -Apply
   4) 如果还想顺便停用 D:\jlink 那套（两个 DLL 混装）：
        pwsh -File ".\tools\fix-jlink-env.ps1" -Apply -IncludeJLinkDir
=============================================================================
#>
[CmdletBinding()]
param(
    # 不加 -Apply 就是 "只看不改" 的演练模式
    [switch]$Apply,
    # 额外把 D:\jlink 改名停用（默认不动）
    [switch]$IncludeJLinkDir
)

$ErrorActionPreference = 'Stop'

$SeggerDir   = 'C:\Keil_v5\ARM\Segger'
$DllActive   = Join-Path $SeggerDir 'JLinkARM.dll'
$DllOriginal = Join-Path $SeggerDir 'JLinkARM_org.dll'
$RefFile     = Join-Path $SeggerDir 'JLinkDevices.ref'
$JLinkDir    = 'D:\jlink'
$Stamp       = Get-Date -Format 'yyyyMMdd-HHmmss'
$BackupDir   = Join-Path $SeggerDir "_backup_$Stamp"

function Line { param([string]$C = '-') Write-Host ($C * 74) }
function Head { param([string]$T) Write-Host ''; Line '='; Write-Host "  $T"; Line '=' }

# ---------------------------------------------------------------- 0. 模式
Head 'J-Link 环境回滚脚本'
if ($Apply) {
    Write-Host '  模式: 【 实际执行 】 会修改文件' -ForegroundColor Yellow
} else {
    Write-Host '  模式: 【 演练 / DRY-RUN 】 只看不改，不改动任何文件' -ForegroundColor Green
    Write-Host '        确认没问题后，加 -Apply 再跑一次才会真正生效'
}
Write-Host "  时间戳: $Stamp"
Write-Host "  备份目录将是: $BackupDir"

# ------------------------------------------------- 1. 前置检查: Keil 是否在跑
Head '1. 前置检查：Keil 是否正在运行'
$uv4 = Get-Process -Name 'UV4' -ErrorAction SilentlyContinue
if ($uv4) {
    Write-Host '  [X] 检测到 UV4.exe 正在运行：' -ForegroundColor Red
    $uv4 | Select-Object Id, ProcessName, StartTime | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host '      DLL 被占用时改名会失败，也会让 Keil 处于半死不活的状态。' -ForegroundColor Red
    Write-Host '      请先完全关闭 Keil（任务管理器里确认 UV4.exe 消失），再重新运行本脚本。' -ForegroundColor Red
    exit 1
}
Write-Host '  [OK] Keil 没在运行' -ForegroundColor Green

$jlinkProcs = Get-Process -ErrorAction SilentlyContinue |
              Where-Object { $_.ProcessName -like 'JLink*' }
if ($jlinkProcs) {
    Write-Host '  [!] 有 J-Link 相关进程在跑，建议也关掉：' -ForegroundColor Yellow
    $jlinkProcs | Select-Object Id, ProcessName | Format-Table -AutoSize | Out-String | Write-Host
}

# ------------------------------------------------- 2. 清点当前状态
Head '2. 当前状态清点'

$state = [ordered]@{}

function Get-DllVer {
    param([string]$P)
    if (-not (Test-Path $P)) { return $null }
    return (Get-Item $P).VersionInfo.FileVersion
}

$verActive   = Get-DllVer $DllActive
$verOriginal = Get-DllVer $DllOriginal
$refExists   = Test-Path $RefFile
$refContent  = if ($refExists) { (Get-Content -Raw -Path $RefFile).Trim() } else { '' }

Write-Host ''
Write-Host '  --- 目录 ---'
Write-Host "  $SeggerDir"
if (Test-Path $SeggerDir) {
    Get-ChildItem $SeggerDir -File |
        Select-Object Name, Length, LastWriteTime |
        Format-Table -AutoSize | Out-String | Write-Host
} else {
    Write-Host '  [X] 这个目录不存在！请确认 Keil 装在 C:\Keil_v5' -ForegroundColor Red
    exit 1
}

Write-Host '  --- 关键文件判读 ---'
if ($verActive) {
    $tag = if ($verActive -eq '8.10f') { ' <- 就是它在新 DLL 配老固件' } else { '' }
    Write-Host ("  JLinkARM.dll      : {0}{1}" -f $verActive, $tag)
} else {
    Write-Host '  JLinkARM.dll      : [不存在]' -ForegroundColor Red
}
if ($verOriginal) {
    Write-Host ("  JLinkARM_org.dll  : {0}  <- Keil 原厂备份，回滚目标" -f $verOriginal)
} else {
    Write-Host '  JLinkARM_org.dll  : [不存在] 没有原厂备份，无法回滚' -ForegroundColor Red
}
if ($refExists) {
    Write-Host ("  JLinkDevices.ref  : 内容 = [{0}]  <- 指向不存在的 XML，要停用" -f $refContent)
} else {
    Write-Host '  JLinkDevices.ref  : [不存在]（可能已经处理过）'
}

Write-Host '  --- 注册表里的兼容性补丁（只读，本脚本不改）---'
$layers = Get-ItemProperty 'HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -ErrorAction SilentlyContinue
$shim = $null
if ($layers) {
    $shim = ($layers.PSObject.Properties |
             Where-Object { $_.Name -like '*UV4.exe*' } |
             Select-Object -First 1).Value
}
if ($shim) {
    Write-Host ("  $shim") -ForegroundColor Yellow
    Write-Host '  ^ 这个 IgnoreFreeLibrary 补丁说明"下载后 DLL 卸载失败"早就被修过一次。' -ForegroundColor Yellow
    Write-Host '    注意模块名写成了 JLinkA（截断），未必能匹配 JLinkARM.dll。' -ForegroundColor Yellow
    Write-Host '    建议先把 DLL 版本理顺，再考虑把它删掉；本脚本不碰它。' -ForegroundColor Yellow
} else {
    Write-Host '  （没有针对 UV4.exe 的兼容性补丁）'
}

# ------------------------------------------------- 3. 决定要做什么
Head '3. 执行计划'

$plan = New-Object System.Collections.ArrayList

if ($verActive -eq $verOriginal -and $verActive) {
    Write-Host "  [跳过] JLinkARM.dll 已经是 $verActive，无需回滚" -ForegroundColor Green
} elseif ($verActive -and $verOriginal) {
    [void]$plan.Add([pscustomobject]@{
        动作 = '备份'; 对象 = 'JLinkARM.dll'
        说明 = "$verActive  ->  $BackupDir\JLinkARM_backup.dll"
    })
    [void]$plan.Add([pscustomobject]@{
        动作 = '覆盖'; 对象 = 'JLinkARM.dll'
        说明 = "用 JLinkARM_org.dll ($verOriginal) 覆盖回去"
    })
} else {
    Write-Host '  [X] 缺少 JLinkARM.dll 或 JLinkARM_org.dll，无法回滚。' -ForegroundColor Red
    Write-Host '      这种情况下请直接考虑换 ST-Link，或手动重装 J-Link 驱动。' -ForegroundColor Red
    exit 1
}

if ($refExists) {
    [void]$plan.Add([pscustomobject]@{
        动作 = '改名'; 对象 = 'JLinkDevices.ref'
        说明 = '-> JLinkDevices.ref.bak（停用这个坏引用，改用内置设备库）'
    })
}

if ($IncludeJLinkDir) {
    if (Test-Path $JLinkDir) {
        [void]$plan.Add([pscustomobject]@{
            动作 = '改名'; 对象 = $JLinkDir
            说明 = "-> ${JLinkDir}_disabled_$Stamp（避免两套 J-Link DLL 打架）"
        })
    } else {
        Write-Host "  [跳过] $JLinkDir 不存在" -ForegroundColor Green
    }
} else {
    Write-Host "  [不动] $JLinkDir（想一起停用就加 -IncludeJLinkDir）" -ForegroundColor DarkGray
}

if ($plan.Count -eq 0) {
    Write-Host '  没有需要做的事。' -ForegroundColor Green
    exit 0
}

$plan | Format-Table -AutoSize | Out-String | Write-Host

# ------------------------------------------------- 4. 执行
Head '4. 执行'

if (-not $Apply) {
    Write-Host '  [演练模式] 上面就是全部改动，但现在【什么都没做】。' -ForegroundColor Green
    Write-Host ''
    Write-Host '  确认无误后，用下面这条命令真正执行（记得先关 Keil）：' -ForegroundColor Cyan
    Write-Host ''
    Write-Host ('    pwsh -File "{0}" -Apply' -f $PSCommandPath) -ForegroundColor Cyan
    Write-Host ''
    exit 0
}

# --- 建备份目录
if (-not (Test-Path $BackupDir)) {
    New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
    Write-Host "  [OK] 建好备份目录: $BackupDir" -ForegroundColor Green
}

# --- 动作 1: 备份 8.10f
if ($verActive -ne $verOriginal) {
    Copy-Item -Path $DllActive -Destination (Join-Path $BackupDir 'JLinkARM_backup.dll') -Force
    Write-Host "  [OK] 已备份 JLinkARM.dll ($verActive) 到备份目录" -ForegroundColor Green

    # --- 动作 2: 用 7.92k 覆盖
    Copy-Item -Path $DllOriginal -Destination $DllActive -Force
    Write-Host "  [OK] 已用 JLinkARM_org.dll ($verOriginal) 覆盖 JLinkARM.dll" -ForegroundColor Green
}

# --- 动作 3: 停用坏 ref
if ($refExists) {
    Rename-Item -Path $RefFile -NewName 'JLinkDevices.ref.bak' -Force
    Write-Host '  [OK] 已把 JLinkDevices.ref 改名为 JLinkDevices.ref.bak' -ForegroundColor Green
}

# --- 动作 4: 停用 D:\jlink
if ($IncludeJLinkDir -and (Test-Path $JLinkDir)) {
    Rename-Item -Path $JLinkDir -NewName "jlink_disabled_$Stamp" -Force
    Write-Host "  [OK] 已把 $JLinkDir 改名为 D:\jlink_disabled_$Stamp" -ForegroundColor Green
}

# ------------------------------------------------- 5. 生成还原脚本
$restorePath = Join-Path $BackupDir 'restore.ps1'
$restoreBody = @'
<#  一键还原 fix-jlink-env.ps1 的全部改动。先关掉 Keil 再跑。  #>
$ErrorActionPreference = 'Stop'
$SeggerDir = 'C:\Keil_v5\ARM\Segger'
$Here      = $PSScriptRoot

if (Get-Process -Name 'UV4' -ErrorAction SilentlyContinue) {
    Write-Host '请先关闭 Keil (UV4.exe)！' -ForegroundColor Red; exit 1
}

Write-Host '正在还原...'

# 1) 还原 JLinkARM.dll
$bak = Join-Path $Here 'JLinkARM_backup.dll'
if (Test-Path $bak) {
    Copy-Item -Path $bak -Destination (Join-Path $SeggerDir 'JLinkARM.dll') -Force
    Write-Host '  [OK] JLinkARM.dll 已还原' -ForegroundColor Green
} else {
    Write-Host '  [跳过] 没找到 JLinkARM_backup.dll' -ForegroundColor Yellow
}

# 2) 还原 JLinkDevices.ref
$refBak = Join-Path $SeggerDir 'JLinkDevices.ref.bak'
if (Test-Path $refBak) {
    Rename-Item -Path $refBak -NewName 'JLinkDevices.ref' -Force
    Write-Host '  [OK] JLinkDevices.ref 已还原' -ForegroundColor Green
}

# 3) 还原 D:\jlink
$dis = Get-ChildItem 'D:\' -Directory -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -like 'jlink_disabled_*' } | Select-Object -First 1
if ($dis) {
    Rename-Item -Path $dis.FullName -NewName 'jlink' -Force
    Write-Host "  [OK] $($dis.Name) 已还原为 D:\jlink" -ForegroundColor Green
}

Write-Host ''
Write-Host '还原完成。' -ForegroundColor Green
'@
Set-Content -Path $restorePath -Value $restoreBody -Encoding UTF8
Write-Host "  [OK] 已生成还原脚本: $restorePath" -ForegroundColor Green

# ------------------------------------------------- 6. 校验结果
Head '6. 校验结果'
$newVer = Get-DllVer $DllActive
Write-Host ("  JLinkARM.dll 当前版本 : {0}" -f $newVer)
Write-Host ("  JLinkARM_org.dll      : {0}" -f (Get-DllVer $DllOriginal))
Write-Host ("  JLinkDevices.ref      : {0}" -f $(if (Test-Path $RefFile) { '仍存在' } else { '已停用（.bak）' }))
Write-Host ''
if ($newVer -eq '7.92k') {
    Write-Host '  [OK] 回滚成功，现在是 Keil 5.39 官方配套的 7.92k' -ForegroundColor Green
} else {
    Write-Host "  [!] 版本是 $newVer，不是预期的 7.92k，请人工确认" -ForegroundColor Yellow
}
Write-Host ''
Write-Host '  接下来：' -ForegroundColor Cyan
Write-Host '    1) 打开 Keil，随便 Build 一次，然后点 Download 试一下' -ForegroundColor Cyan
Write-Host '    2) 如果还卡死：说明是"克隆硬件 + 新 DLL"的硬冲突，' -ForegroundColor Cyan
Write-Host '       要么装 J-Link V6.10~V6.44 的老 DLL，要么直接换 ST-Link' -ForegroundColor Cyan
Write-Host '    3) 想退回原状：pwsh -File "<备份目录>\restore.ps1"' -ForegroundColor Cyan
Write-Host ("       {0}" -f $restorePath) -ForegroundColor DarkGray
Write-Host ''
