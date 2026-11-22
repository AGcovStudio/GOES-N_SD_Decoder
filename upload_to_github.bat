@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
rem ===========================================================================================================
rem  一键上传到 GitHub。双击运行即可（放在项目根目录，和 README.md 同一层）。
rem  需要：Git for Windows（https://git-scm.com/download/win）。
rem  可选：GitHub CLI（https://cli.github.com/），装了的话仓库不存在时会自动创建。
rem  登录：第一次推送时会弹出 GitHub 登录窗口，由你自己登录；本脚本不保存任何密码或令牌。
rem ===========================================================================================================
set "GH_OWNER=AGcovStudio"
set "GH_REPO=GOES-N_SD_Decoder"
set "BRANCH=main"
set "VISIBILITY=public"

cd /d "%~dp0"
if not exist "README.md" (
  echo 请把本脚本放在项目根目录（和 README.md 同一层）再运行。
  goto :fail
)

where git >nul 2>&1 || (
  echo 没有找到 git。请先安装 Git for Windows：https://git-scm.com/download/win
  goto :fail
)

rem ---- 提交者信息（只在没设置过时询问）
for /f "delims=" %%A in ('git config --global user.name 2^>nul') do set "GN=%%A"
if "!GN!"=="" (
  set /p "GN=Git 用户名（显示在提交记录里）: "
  git config --global user.name "!GN!"
)
for /f "delims=" %%A in ('git config --global user.email 2^>nul') do set "GE=%%A"
if "!GE!"=="" (
  echo 提交需要一个邮箱。可以用 GitHub 提供的隐私邮箱：GitHub 设置 - Emails 里的 xxxx+用户名@users.noreply.github.com
  set /p "GE=Git 邮箱: "
  git config --global user.email "!GE!"
)

rem ---- 仓库与提交
if not exist ".git" (
  echo [1/4] 初始化本地仓库
  git init -b %BRANCH% || goto :fail
) else (
  echo [1/4] 本地仓库已存在
)
git checkout -B %BRANCH% >nul 2>&1

echo [2/4] 添加文件（build、dist、录音文件已在 .gitignore 中排除）
git add -A || goto :fail
git diff --cached --quiet
if errorlevel 1 (
  set "MSG=GOES-N SD Decoder 1.0.0"
  git rev-parse --verify HEAD >nul 2>&1
  if not errorlevel 1 (
    set "MSG="
    set /p "MSG=提交说明（直接回车用默认 Update）: "
    if "!MSG!"=="" set "MSG=Update"
  )
  git commit -m "!MSG!" || goto :fail
) else (
  echo     没有新的改动需要提交。
)

rem ---- 远程仓库
set "URL=https://github.com/%GH_OWNER%/%GH_REPO%.git"
git remote get-url origin >nul 2>&1 || git remote add origin "%URL%"
git remote set-url origin "%URL%"

echo [3/4] 检查 GitHub 上的仓库 %GH_OWNER%/%GH_REPO%
git ls-remote origin >nul 2>&1
if errorlevel 1 (
  where gh >nul 2>&1
  if errorlevel 1 (
    echo     GitHub 上还没有这个仓库（或还没登录）。
    echo     请先在浏览器打开 https://github.com/new ，Owner 选 %GH_OWNER%，仓库名填 %GH_REPO%，
    echo     不要勾选 README / .gitignore / License，创建后再运行本脚本。
    echo     也可以安装 GitHub CLI 后重新运行，脚本会自动创建仓库。
    goto :fail
  )
  gh auth status >nul 2>&1 || gh auth login || goto :fail
  echo     用 GitHub CLI 创建仓库（%VISIBILITY%）
  gh repo create "%GH_OWNER%/%GH_REPO%" --%VISIBILITY% --description "GOES-N/O/P 1676 MHz Sensor Data baseband playback decoder" || goto :fail
)

echo [4/4] 推送到 %URL%
git push -u origin %BRANCH% || goto :fail

echo.
echo 完成：https://github.com/%GH_OWNER%/%GH_REPO%
goto :end

:fail
echo.
echo *** 上传没有完成，请看上面的提示。***
pause
exit /b 1

:end
pause
endlocal
