#include <iostream>
#include <assert.h>
#include "../data_structures/flash_data.hpp"
#include "../data_structures/segment.hpp"
#include "../data_structures/segment_factory.hpp"
#include "../data_structures/segment_cache.hpp"
#include "../layers/flash/flash.h"

unsigned int cacheSize = 4;

FlashData flashData = 
{
	.blockSize = 2,
	.segmentSize = 32,
	.flashSize = 100,
	.wearLimit = 1000,
	.numBlocks = 100 * 32,
	.checkpointSegment = 2
};

SegmentFactory segmentFactory(flashData);
SegmentCache cache(&segmentFactory, cacheSize);

void TestSegmentCache()
{
	auto s3 = segmentFactory.Build(3);
	auto s4 = segmentFactory.Build(4);
	auto s5 = segmentFactory.Build(5);
	auto s6 = segmentFactory.Build(6);
	auto s7 = segmentFactory.Build(7);
	auto s8 = segmentFactory.Build(8);

	assert(cache.containsEntry(3) == false);
	assert(cache.containsEntry(4) == false);
	assert(cache.containsEntry(5) == false);
	assert(cache.containsEntry(6) == false);
	assert(cache.containsEntry(7) == false);
	assert(cache.containsEntry(8) == false);

	cache.putEntry(s3);
	assert(cache.containsEntry(3) == true);

	cache.putEntry(s4);
	assert(cache.containsEntry(4) == true);

	cache.putEntry(s5);
	assert(cache.containsEntry(5) == true);

	cache.putEntry(s6);
	assert(cache.containsEntry(6) == true);

	cache.putEntry(s7);
	assert(cache.containsEntry(7) == true);
	assert(cache.containsEntry(6) == true);
	assert(cache.containsEntry(5) == true);
	assert(cache.containsEntry(4) == true);
	assert(cache.containsEntry(3) == false);

	cache.putEntry(s8);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == true);
	assert(cache.containsEntry(6) == true);
	assert(cache.containsEntry(5) == true);
	assert(cache.containsEntry(4) == false);
	assert(cache.containsEntry(3) == false);

	s3 = segmentFactory.Build(3);
	cache.putEntry(s3);
	assert(cache.containsEntry(3) == true);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == true);
	assert(cache.containsEntry(6) == true);
	assert(cache.containsEntry(5) == false);
	assert(cache.containsEntry(4) == false);

	s4 = segmentFactory.Build(4);
	cache.putEntry(s4);
	assert(cache.containsEntry(4) == true);
	assert(cache.containsEntry(3) == true);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == true);
	assert(cache.containsEntry(6) == false);
	assert(cache.containsEntry(5) == false);

	assert(cache.getEntry(4)->summary.segmentNumber == 4);
	assert(cache.getEntry(3)->summary.segmentNumber == 3);
	assert(cache.getEntry(8)->summary.segmentNumber == 8);
	assert(cache.getEntry(7)->summary.segmentNumber == 7);

	cache.invalidateEntry(1);
	assert(cache.containsEntry(4) == true);
	assert(cache.containsEntry(3) == true);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == true);

	cache.invalidateEntry(3);
	assert(cache.containsEntry(4) == true);
	assert(cache.containsEntry(3) == false);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == true);

	cache.invalidateEntry(7);
	assert(cache.containsEntry(4) == true);
	assert(cache.containsEntry(3) == false);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == false);

	cache.invalidateEntry(4);
	assert(cache.containsEntry(4) == false);
	assert(cache.containsEntry(3) == false);
	assert(cache.containsEntry(8) == true);
	assert(cache.containsEntry(7) == false);

	cache.invalidateEntry(8);
	assert(cache.containsEntry(4) == false);
	assert(cache.containsEntry(3) == false);
	assert(cache.containsEntry(8) == false);
	assert(cache.containsEntry(7) == false);
}

void RunTests()
{
	TestSegmentCache();
}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}