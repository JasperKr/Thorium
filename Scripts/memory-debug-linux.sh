valgrind --gen-suppressions=all ./build/snap ../src/Engine/main.lua

# Delete vgcore file if it exists
rm -f ./vgcore.*