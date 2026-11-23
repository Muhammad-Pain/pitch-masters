@echo off
chcp 65001 >nul
rem ============================================================
rem  Pitch Masters: сборка C++ части под Android (Windows)
rem  Нужно: Python 3, Git, Android NDK (через Android Studio)
rem  Перед запуском укажите путь к NDK, например:
rem    set ANDROID_HOME=C:\Users\ИМЯ\AppData\Local\Android\Sdk
rem ============================================================
setlocal
if "%ANDROID_HOME%"=="" (
  echo [!] Укажите ANDROID_HOME — папку Android SDK. Пример:
  echo     set ANDROID_HOME=C:\Users\%USERNAME%\AppData\Local\Android\Sdk
  exit /b 1
)
if "%NDK_VERSION%"=="" set NDK_VERSION=28.1.13356709
echo Использую NDK %NDK_VERSION% (поменять: set NDK_VERSION=ваша_версия)
python -m pip install scons
if not exist godot-cpp (
  git clone --depth 1 -b 4.5 https://github.com/godotengine/godot-cpp.git godot-cpp || exit /b 1
)
scons platform=android arch=arm64 target=template_release ndk_version=%NDK_VERSION% -j4 || exit /b 1
scons platform=android arch=arm32 target=template_release ndk_version=%NDK_VERSION% -j4 || exit /b 1
scons platform=windows target=template_debug -j4 || exit /b 1
echo.
echo Готово. Библиотеки лежат в game\bin. Откройте папку game в Godot 4.6
echo и сделайте Project ^> Export ^> Android ^> Export Project.
endlocal
