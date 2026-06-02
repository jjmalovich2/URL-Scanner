# 1. Wipe out any old, broken build artifacts to prevent cache errors
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue build
New-Item -ItemType Directory -Force build
cd build

# 2. Re-run configuration and compile
cmake -G "MinGW Makefiles" ..
mingw32-make

# 3. Execute your program (it will now be successfully named scanner.exe)
& ".\scanner.exe"
