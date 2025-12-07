@REM Fetch https://github.com/shader-slang/slang/releases/download/v2025.22.1/slang-2025.22.1-windows-x86_64.zip

set URL="https://github.com/shader-slang/slang/releases/download/v2025.22.1/slang-2025.22.1-windows-x86_64.zip"
set TEMP_DIR=.\slang_download

if not exist %TEMP_DIR% (
    mkdir %TEMP_DIR%
)

echo "Fetching slang..."
curl -L %URL% -o %TEMP_DIR%\slang.zip

echo "Extracting slang..."
mkdir %TEMP_DIR%\contents
tar -xf %TEMP_DIR%\slang.zip -C %TEMP_DIR%\contents

if not exist .\include\slang\ (
    mkdir .\include\slang\
)

copy %TEMP_DIR%\contents\include\* .\include\slang\ /Y

if not exist .\bin\slang\ (
    mkdir .\bin\slang\
)

copy %TEMP_DIR%\contents\bin\* .\bin\slang\

if not exist .\lib\slang\ (
    mkdir .\lib\slang\
)

copy %TEMP_DIR%\contents\lib\* .\lib\slang\ /Y

echo "Fetching float-16..."
set FLOAT16_URL="https://github.com/fengwang/float16_t/blob/master/float16_t.hpp?raw=true"

if not exist .\include\float16_t\ (
    mkdir .\include\float16_t\
)

curl -L %FLOAT16_URL% -o .\include\float16_t\float16_t.hpp

echo "Cleaning up..."
rmdir /s /q %TEMP_DIR%