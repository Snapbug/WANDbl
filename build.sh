#!/bin/bash
path_to_executable=$(which cmake28)
if [ -x "$path_to_executable" ] ; then
  echo "Using cmake28 ..."
  CMAKE=cmake28
else 
  echo "No cmake28 found. Using cmake."
  echo "Assuming it is recent enough."
  echo "This might fail."
  CMAKE=cmake
fi

echo "Pull all external modules .."
git submodule init
git submodule update
echo "Configure and build dependencies .."
cd build
$CMAKE ..
if [ $? -ne 0 ];
then
  echo "ERROR: cmake failed!"
  exit -1
fi
make -j 5
if [ $? -ne 0 ];
then
  echo "ERROR: Make failed!"
  exit -1
fi
echo "Configure and build ATIRE ..."
cd ../external/atire
make
if [ $? -ne 0 ];
then
  echo "ERROR: Build ATIRE failed!"
  exit -1
fi
echo "Compile the ATIRE extractor ..."
cd ../../src
make
if [ $? -ne 0 ];
then
  echo "ERROR: ATIRE dependent build failed!"
  exit -1
fi
cd ..
cp src/mk_wand_idx bin/mk_wand_idx
cp build/wand_search bin/wand_search
echo "Binaries are now in the bin directory"
