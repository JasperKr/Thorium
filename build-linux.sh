if [ -z "$1" ]; then
    CONFIG="Release"
else
    CONFIG="$1"
fi

rm -f ./build/Thorium

ENABLE_TRACY="TRACY_ENABLE"

FLAGS=""

if [ "$CONFIG" == "Debug" ]; then
  FLAGS="$FLAGS -D$ENABLE_TRACY=1 -g -O0"
else
  FLAGS="$FLAGS -O3"
fi

cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -B build \
  -DCMAKE_BUILD_TYPE=$CONFIG \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_FLAGS="-std=c++23 -Wc23-extensions $FLAGS" \
  -DCMAKE_C_FLAGS="-std=c23 -Wc23-extensions $FLAGS"

cmake --build build

# Make sure to append amdgpu.ppfeaturemask=0xffffffff to GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub (space-separated).
# Otherwise profiling may not work correctly.
# After modifying /etc/default/grub, run `sudo update-grub` or `sudo grub-mkconfig -o /boot/grub/grub.cfg` and reboot.
# You can verify the setting by running `cat /proc/cmdline` and checking for amdgpu.ppfeaturemask=0xffffffff

if [ "$2" == "profile" ]; then
  SDL_VIDEODRIVER=x11 RADV_PERFTEST=rt MESA_VK_TRACE=rgp MESA_VK_TRACE_TRIGGER=/tmp/trigger ./build/Thorium ../src/Engine/main.lua
fi

# if second argument is "run", run the built executable
if [ "$2" == "run" ]; then
  ./build/Thorium ../src/Engine/main.lua
fi