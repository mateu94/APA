//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*

  This file does NOT implement any prefetcher, and is just an outline

 */

#include <stdio.h>
#include "../inc/prefetcher.h"

#define PAGE_COUNT 4

// Global History Buffer
typedef struct GHB_page
{
	// Page owner of the GHB
	unsigned long long int page;

	// Address that caused the miss
	unsigned long long int miss_addr[64];
	
	// The index in the GHB where the delta last happened
	unsigned int previous_delta_pointer[64];
} GHB_page;

// Address Stream Correlations
typedef struct ASC_History_
{
	// The delta correlation is a unique pair (x,y) of deltas
	// First delta
	long long int firstDelta[64];

	// Second delta
	long long int secondDelta[64];

	// Prefetch prediction (4 subsequent deltas)
	long long int prediction[64][4];

	// Length of the prediction (4 means prediction has been calculated)
	char length[64];

	// Pointer to the head of the ASC
	unsigned int head;
} ASC_History_;

// Address Stream Correlations being calculated
typedef struct ASC_Current_
{
	// The delta correlation is a unique pair (x,y) of deltas
	// First delta
	long long int firstDelta[4];

	// Second delta
	long long int secondDelta[4];

	// Prefetch prediction (4 subsequent deltas)
	long long int prediction[4][4];

	// Length of the prediction (4 means prediction has been calculated)
	char length[4];

	// Pointer to the head of the ASC
	char head;
} ASC_Current_;

GHB_page GHB[PAGE_COUNT];

ASC_History_ ASC_History;
ASC_Current_ ASC_Current;

// used for page replacement
unsigned long long int lru;

unsigned long long int lastAddr;
long long int lastDelta;

void l2_prefetcher_initialize(int cpu_num)
{
  printf("No Prefetching\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
	
	// GHB
	#if 0
	int i, j;
	for(i=0; i<PAGE_COUNT; i++)
		for(j=0; j<64; j++)
		{
			GHB[i].miss_addr[j] = 0;
			GHB[i].previous_delta_pointer[j] = 0;
		}
	lru = 0;
	#endif

	// ASC
	ASC_History.head = 0;
	ASC_Current.head = 0;

	lastAddr = 0;
	lastDelta = 0;
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  //printf("(0x%llx 0x%llx %d %d %d) ", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

	unsigned long long int cl_address = addr>>6;
	unsigned long long int page = cl_address>>6;
	unsigned long long int page_offset = cl_address&63;

	// GHB
	#if 0
	// check to see if we have a page hit
	int page_index = -1;
	int i;
	for(i=0; i<PAGE_COUNT; i++)
	{
		if(GHB[i].page == page)
		{
			page_index = i;
			break;
		}
	}

	// if the page was not found, the lru page has to be replaced
	if (page_index == -1)
	{
		GHB[lru].page = page;
		page_index = lru;
		for(i=0; i<64; i++)
		{
			GHB[lru].miss_addr[i] = 0;
			GHB[lru].previous_delta_pointer[i] = 0;
		}
		lru++;
		lru = lru%4;
	}
	#endif

	// ASC
	// Only prefetch if it is a miss
	if (!cache_hit)
	{
		// calculate the stride between the current address and the last address
		long long int delta;
		delta = addr - lastAddr;

		int i;
		// Look if the current correlation is being processed
		for(i=0; i<4; i++)
		{
			if (ASC_Current.firstDelta[i] == lastDelta && ASC_Current.secondDelta[i] == delta)
			{
				char length = ASC_Current.length[i];
				if (length == 4)
				{
					// The ASC_History is updated with the new prediction
				}
				else
				{
					// Append delta to the prediction
					ASC_Current.prediction[i][length] = delta;
					ASC_Current.length[i]++;
				}
			}
		}
	}

	//l2_prefetch_line(0, addr, ((addr>>6)+1)<<6, FILL_L2);
}

void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
  // uncomment this line to see the information available to you when there is a cache fill event
  //printf("0x%llx %d %d %d 0x%llx\n", addr, set, way, prefetch, evicted_addr);
}

void l2_prefetcher_heartbeat_stats(int cpu_num)
{
  printf("Prefetcher heartbeat stats\n");
}

void l2_prefetcher_warmup_stats(int cpu_num)
{
  printf("Prefetcher warmup complete stats\n\n");
}

void l2_prefetcher_final_stats(int cpu_num)
{
  printf("Prefetcher final stats\n");
}
