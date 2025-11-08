if "%1"=="" (
    set "CONFIG=Release"
) else (
    set "CONFIG=%1"
)

del .\build\Thorium.exe

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build

.\build\Thorium.exe
