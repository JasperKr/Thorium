valgrind --gen-suppressions=all ./build/Thorium ../src/Engine/main.lua

# Delete vgcore file if it exists
rm -f ./vgcore.*