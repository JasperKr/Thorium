if [ -z "$1" ]; then
    CONFIG="Release"
else
    CONFIG="$1"
fi

rm -f ./build/Thorium

cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -B build \
  -DCMAKE_BUILD_TYPE=$CONFIG \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -DCMAKE_C_COMPILER=clang

cmake --build build

./build/Thorium