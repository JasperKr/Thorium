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
git clone --depth 1 https://github.com/google/shaderc.git temp_shaderc
cp -r temp_shaderc/include/* include/shaderc/
rm -rf temp_shaderc

echo "All dependencies fetched into include/"
