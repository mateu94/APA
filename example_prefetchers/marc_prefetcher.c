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

	// State of the delta correlation
	// 1: Valid
	// 0: Not valid, the prediction is being calculated
	char state[64];

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

	// Index to the History ASC
	unsigned int indexHistory[4];

	// Pointer to the head of the ASC
	unsigned int head;
} ASC_Current_;

GHB_page GHB[PAGE_COUNT];

ASC_History_ ASC_History;
ASC_Current_ ASC_Current;

// used for page replacement
unsigned long long int lru;

unsigned long long int lastAddr;
long long int lastlastDelta;
long long int lastDelta;

void printInformation()
{
	printf("ASC Current\n");
	int i;
	for(i=0; i<4; i++)
	{
		printf("%lld, %lld, %d, %u\n", ASC_Current.firstDelta[i], ASC_Current.secondDelta[i], ASC_Current.length[i], ASC_Current.indexHistory[i]);
	}
}

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
	lastlastDelta = 0;
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
		delta = cl_address - lastAddr;

		int i;
		char found = 0;
		char found2 = 0;

		// Look if there are pending correlations to be predicted
		for(i=0; i<4; i++)
		{
			if (ASC_Current.length[i] != 4)
			{
				char index = ASC_Current.length[i];
				unsigned int indexHistory = ASC_Current.indexHistory[i];
				ASC_History.prediction[indexHistory][index] = delta;

				// Increase the prediction length and set the state to 1 if the prediction is full
				ASC_Current.length[i]++;
				ASC_History.length[indexHistory]++;
				if (ASC_Current.length[i] == 4)
				{
					ASC_History.state[indexHistory] = 1;
				}
			}
		}

		// Look if the current correlation is already in the ASC
		for (i=0; i<64 && (!found || !found2); i++)
		{
			// Check if the current correlation is ready and valid to prefetch
			if (ASC_History.firstDelta[i] == lastDelta && ASC_History.secondDelta[i] == delta)
			{
				found = 1;
				if (ASC_History.state[i] == 1)
				{
					l2_prefetch_line(0, addr, ((addr>>6)+ASC_History.prediction[i][0])<<6, FILL_L2);
					l2_prefetch_line(0, addr, ((addr>>6)+ASC_History.prediction[i][1])<<6, FILL_L2);
					l2_prefetch_line(0, addr, ((addr>>6)+ASC_History.prediction[i][2])<<6, FILL_L2);
					l2_prefetch_line(0, addr, ((addr>>6)+ASC_History.prediction[i][3])<<6, FILL_L2);
				}
			}
			// Check if the first predicted delta of the previous correlation is correct
			else if (ASC_History.firstDelta[i] == lastlastDelta && ASC_History.secondDelta[i] == lastDelta)
			{	
				found2 = 1;
				// If it differs, set the state to 0 and start the new prediction
				if (ASC_History.state[i] == 1 && ASC_History.prediction[i][0] != delta)
				{
					ASC_History.state[i] = 0;
					ASC_History.prediction[i][0] = delta;
					ASC_History.length[i] = 1;

					unsigned int head = ASC_Current.head;
					ASC_Current.firstDelta[head] = lastlastDelta;
					ASC_Current.secondDelta[head] = lastDelta;
					ASC_Current.length[head] = 1;
					ASC_Current.indexHistory[head] = i;
					ASC_Current.head = (ASC_Current.head + 1)%4;
				}
			}
		}

		// If not found add a new entry for that correlation and start the new prediction
		if (!found && lastlastDelta != 0 && lastDelta != 0)
		{
			unsigned int head = ASC_History.head;
			ASC_History.firstDelta[head] = lastDelta;
			ASC_History.secondDelta[head] = delta;
			ASC_History.state[head] = 0;
			ASC_History.length[head] = 0;
			ASC_History.head = (ASC_History.head +1)%64;

			unsigned int headCurrent = ASC_Current.head;
			ASC_Current.firstDelta[headCurrent] = lastDelta;
			ASC_Current.secondDelta[headCurrent] = delta;
			ASC_Current.length[headCurrent] = 0;
			ASC_Current.indexHistory[headCurrent] = ASC_History.head;
			ASC_Current.head = (ASC_Current.head + 1)%4;
		}

		lastAddr = cl_address;
		lastlastDelta = lastDelta;
		lastDelta = delta;
	}

	//printf("lastlastDelta = %lld, lastDelta = %lld\n", lastlastDelta, lastDelta);
	//printInformation();
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
