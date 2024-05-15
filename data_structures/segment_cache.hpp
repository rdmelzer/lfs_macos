#pragma once

#include <vector>
#include "segment.hpp"
#include "segment_factory.hpp"

class SegmentCache
{
private:
	int cache_size;
	std::vector<InMemorySegment *> queue;		
	SegmentFactory * segmentFactory;

public:
	SegmentCache(SegmentFactory * segFac, int size) : 			
		cache_size(size),
		segmentFactory(segFac)
	{
	}

	~SegmentCache()
	{
		for (auto seg : queue)
		{
			segmentFactory->Destroy(seg);
		}
	}

	bool containsEntry(unsigned int segmentNumber)
	{
		for (auto seg : queue)
		{
			if (seg->summary.segmentNumber == segmentNumber)
			{
				return true;
			}
		} 

		return false;
	}

	InMemorySegment * getEntry(unsigned int segmentNumber)
	{
		std::cout << "[SegmentCache] getting segment in cache. Segment: " << segmentNumber << std::endl;

		if (!containsEntry(segmentNumber))
		{
			std::cerr << "[SegmentCache] ERROR: Attempting to accesss non existing entry in segment cache" << std::endl;
			throw;
		}

		if (queue[0]->summary.segmentNumber == segmentNumber)
		{
			return queue[0];
		}

		for(auto it = queue.begin(); it != queue.end(); ++it)
		{
			auto x = *it;
			if (x->summary.segmentNumber == segmentNumber)
			{
				queue.erase(it);
    			queue.insert(queue.begin(), x);
    			break;
			}
		}
		
		return queue[0];
	}

	void putEntry(InMemorySegment * segment)
	{
		// cant add duplicated entries
		unsigned int segmentNumber = segment->summary.segmentNumber;
		std::cout << "[SegmentCache] Putting segment in cache. Segment: " << segmentNumber << std::endl;

		if (containsEntry(segmentNumber))
		{
			std::cerr << "[SegmentCache] ERROR: Attempting to add duplicate entry to segment cache: " << segmentNumber << std::endl;
			for(auto it = queue.begin(); it != queue.end(); ++it)
			{
				auto x = *it;
				x->summary.PrintSegmentSummaryBlock();
			}

			throw;
		}

		if (queue.size() == cache_size)
		{
			std::cout << "[SegmentCache] Segment cache full. Removing segment " << queue.back()->summary.segmentNumber << std::endl;
			InMemorySegment * toDelete = queue.back();
			segmentFactory->Destroy(toDelete);
			queue.pop_back();
		}

		queue.insert(queue.begin(), segment);
	}

	void invalidateEntry(unsigned int segmentNumber)
	{
		std::cout << "[SegmentCache] invalidating segment in cache. Segment: " << segmentNumber << std::endl;

		if (!containsEntry(segmentNumber))
		{
			return;
		}

		for(auto it = queue.begin(); it != queue.end(); ++it)
		{
			auto x = *it;
			if (x->summary.segmentNumber == segmentNumber)
			{
				segmentFactory->Destroy(x);
				queue.erase(it);
				break;
			}
		}
	}
};