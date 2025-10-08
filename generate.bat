@echo off
setlocal

rem Step 0: Check if CMake is installed and available in PATH
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo CMake is not installed or not in PATH.
    exit /b 1
)

rem Step 1: Create a build directory
set build_dir=build
if not exist %build_dir% (
    mkdir %build_dir%
    echo Created directory: %build_dir%
) else (
    echo Directory already exists: %build_dir%
)

rem Step 2: Change current working dir into build directory
cd %build_dir% || (
    echo Failed to change directory to %build_dir%.
    exit /b 1
)

rem Step 3: Run CMake with the specified generator
cmake -G "Visual Studio 17 2022" -A x64 ..
if %errorlevel% neq 0 (
    echo CMake configuration failed.
    exit /b 1
)

rem Step 4: Open the generated project solution
set solution_file=Grinder.sln
if exist %solution_file% (
    start %solution_file%
) else (
    echo Solution file %solution_file% does not exist.
    exit /b 1
)

endlocal
