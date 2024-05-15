if [ -d "bin/" ]; then
	rm -r bin/
fi

echo "Building Flash Layer..."
mkdir bin/
mkdir bin/tests/
gcc -c -Wall -o bin/flash.o layers/flash/flash.c
echo "Building mklfs..."
g++ -g -Wall -std=c++1z -o bin/mklfs ./utilities/mklfs.cpp bin/flash.o
echo "Building lfsck..."
g++ -g -Wall -std=c++1z -o bin/lfsck ./utilities/lfsck.cpp bin/flash.o
echo "Building Tests..."
g++ -g -Wall -std=c++1z -o bin/tests/log_test tests/log_test.cpp bin/flash.o
g++ -g -Wall -std=c++1z -o bin/tests/checkpoint_test tests/checkpoint_test.cpp bin/flash.o
g++ -g -Wall -std=c++1z -o bin/tests/file_test tests/file_test.cpp bin/flash.o
g++ -g -Wall -std=c++1z -o bin/tests/directory_test tests/directory_test.cpp bin/flash.o
g++ -g -Wall -std=c++1z -o bin/tests/cleaner_test tests/cleaner_test.cpp bin/flash.o
g++ -g -Wall -std=c++1z -o bin/tests/segment_cache_test tests/segment_cache_test.cpp bin/flash.o

echo "Building lfs with fuse..."
g++ -g -Og `pkg-config fuse --cflags --libs` -Wall -std=c++1z -o bin/lfs lfs_main.cpp bin/flash.o

rm -r bin/tests/*.dSYM
rm -r bin/*.dSYM
