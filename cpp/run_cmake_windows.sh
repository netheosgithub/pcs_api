#!/bin/bash 

hash cmake.exe 2>/dev/null
if [ $? -ne 0 ] ; then
  # If cmake.exe is not in your PATH, you may set its absolute path here:
  CMAKE="/e/programs/CMake 2.8/bin/cmake.exe"
else
  CMAKE=$(hash -t cmake.exe)
fi
echo "Using CMAKE=$CMAKE"
mkdir -p build_Win32
cd build_Win32
"$CMAKE" .. -G 'Visual Studio 12' -DCMAKE_VERBOSE_MAKEFILE=ON
cd ..

mkdir -p build_x64
cd build_x64
"$CMAKE" .. -G 'Visual Studio 12 Win64' -DCMAKE_VERBOSE_MAKEFILE=ON
cd ..

