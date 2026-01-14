#!/usr/bin/env bash
set -e

git submodule update --init --recursive

# Create include folders
mkdir -p include/tl include/vma include/stb include/volk include/shaderc

# Fetch TL expected.hpp
curl -L https://raw.githubusercontent.com/TartanLlama/expected/master/include/tl/expected.hpp -o include/tl/expected.hpp

# Fetch VMA
curl -L https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h -o include/vma/vk_mem_alloc.h

# Fetch Volk
curl -L https://raw.githubusercontent.com/zeux/volk/master/volk.h -o include/volk/volk.h
curl -L https://raw.githubusercontent.com/zeux/volk/master/volk.c -o include/volk/volk.c

# Fetch stb_image
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o include/stb/stb_image.h

# Shaderc: clone repository and copy necessary include

# check if the folder is empty, otherwise skip
if [ -z "$(ls -A include/shaderc)" ]; then
  echo "Fetching shaderc..."
  git clone --depth 1 https://github.com/google/shaderc.git temp_shaderc
  cp -r temp_shaderc/include/* include/shaderc/
else
  echo "shaderc include folder is not empty, skipping fetch."
fi
rm -rf temp_shaderc

# If "./include/slang/" is missing or empty:
if [ ! -d "./include/slang/" ] || [ -z "$(ls -A ./include/slang/)" ]; then
    URL="https://github.com/shader-slang/slang/releases/download/v2025.23.1/slang-2025.23.1-linux-x86_64.zip"
    TEMP_DIR="./slang_download"

    if [ ! -d "$TEMP_DIR" ]; then
        mkdir -p "$TEMP_DIR"
    fi

    echo "Fetching slang..."
    curl -L "$URL" -o "$TEMP_DIR/slang.zip"
    echo "Extracting slang..."
    mkdir -p "$TEMP_DIR/contents"
    unzip -q "$TEMP_DIR/slang.zip" -d "$TEMP_DIR/contents"
    if [ ! -d "./include/slang/" ]; then
        mkdir -p "./include/slang/"
    fi
    cp -r "$TEMP_DIR/contents/include/"* "./include/slang/"
    if [ ! -d "./bin/slang/" ]; then
        mkdir -p "./bin/slang/"
    fi
    # cp -rL "$TEMP_DIR/contents/bin/"* "./bin/slang/"
    find "$TEMP_DIR/contents/bin" -maxdepth 1 -type f -exec cp {} ./bin/slang/ \;
    if [ ! -d "./lib/slang/" ]; then
        mkdir -p "./lib/slang/"
    fi
    # cp -rL "$TEMP_DIR/contents/lib/"* "./lib/slang/"
    find "$TEMP_DIR/contents/lib" -maxdepth 1 -type f -exec cp {} ./lib/slang/ \;
    echo "Cleaning up..."
    rm -rf "$TEMP_DIR"
fi

echo "Fetching float-16"
URL="https://github.com/fengwang/float16_t.git"
if [ ! -d "float16_t_temp" ]; then
    git clone --depth 1 "$URL" float16_t_temp
fi
mkdir -p include/float16_t
cp float16_t_temp/float16_t.hpp include/float16_t/float16_t.hpp
rm -rf float16_t_temp

# Download https://github.com/spnda/fastgltf.git to temp/fastgltf and copy temp/fastgltf/include/fastgltf/* to include/fastgltf/
URL="https://github.com/spnda/fastgltf.git"
if [ ! -d "include/fastgltf" ]; then
    git clone --depth 1 "$URL" include/fastgltf
fi
URL="https://github.com/simdjson/simdjson/releases/download/v4.2.4/singleheader.zip"
if [ ! -d "include/simdjson" ]; then
    mkdir -p include/simdjson
    curl -L "$URL" -o "include/simdjson/singleheader.zip"
    unzip -q "include/simdjson/singleheader.zip" -d "include/simdjson/"
    rm "include/simdjson/singleheader.zip"
fi

echo "All dependencies fetched into include/"
