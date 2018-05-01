//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*
  
  This file describes a Markov style prefetcher with a prefetch and access history table.  For each input address addr,
 the most recent pair of delta is calculated and that is used as the access key. The chain is then traversed and prefetches issued. In this prefetcher, the prefetch degree is set as 2.
 The chain is followed according to certain conditions.The prefetches are then filled into L2 or LLC depending on L2's MSHR status.

 */

#include <stdio.h>
#include "../inc/prefetcher.h"

#define Page_Count 64
#define Prefetch_Degree 2
#define delta_table_Size 100
#define access_table_Size 64

typedef struct PF_data_page
{
  // page address
  unsigned long long int page;

  //Access key
  unsigned long long int access_key1[access_table_Size];
  unsigned long long int access_key2[access_table_Size];
  int delta_pointer[access_table_Size];
  int top_access;

  //delta-Table
  int delta_delta[delta_table_Size];
  int delta_point[delta_table_Size];
  int top_delta;
  

  
  //PF_Table
  // This map represents cache lines in this page that have already been prefetched.
  // We will only prefetch lines that haven't already been either demand accessed or prefetched.
  int pf_map[64];

  

  //delta-Table Updation resources

  unsigned long long int last_address1;
  unsigned long long int last_address2;
  int prev_delta_point;  

  // used for page replacement
  unsigned long long int lru;
} PF_data_page_t;

PF_data_page_t PF_pages[Page_Count];


void l2_prefetcher_initialize(int cpu_num)
{
  printf("Markov_PF_history Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're running in
  //printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

   int i;
 
  for(i=0; i<Page_Count; i++)
    { 
      PF_pages[i].page = 0;
      PF_pages[i].lru = 0;

      int j;
      for(j=0; j<access_table_Size; j++)
	{
	  PF_pages[i].access_key1[j] = 0;
          PF_pages[i].access_key2[j] = 0;
          PF_pages[i].delta_pointer[j] = -1;
	  PF_pages[i].pf_map[j] = 0;
	}
     
     int k;
     for(k=0; k<delta_table_Size; k++)
         {PF_pages[i].delta_delta[k]=0;
          PF_pages[i].delta_point[k]=-1;
         } 
     
 
    PF_pages[i].last_address1=0; 
    PF_pages[i].last_address2=0;
    PF_pages[i].top_access=-1;
    PF_pages[i].top_delta=-1;
    PF_pages[i].prev_delta_point=-1;
   
    }

}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  //entering operate
   
  unsigned long long int cl_address = addr>>6;
  unsigned long long int page = cl_address>>6;
  int new_pageoffset, now_pageoffset;
  now_pageoffset =   (addr>>6) & 63;  
  
  // check to see if we have a page hit
  int page_index = -1;
  int i;
  for(i=0; i<Page_Count; i++)
    {
      if(PF_pages[i].page == page)
	{ 
	  page_index = i;
	  break;
	}
    }

  if(page_index == -1)
    { 
      // the page was not found, so we must replace an old page with this new page

      // find the oldest page
      int lru_index = 0;
      unsigned long long int lru_cycle = PF_pages[lru_index].lru;
      int i;
      for(i=0; i<Page_Count; i++)
	{
	  if(PF_pages[i].lru < lru_cycle)
	    {
	      lru_index = i;
	      lru_cycle = PF_pages[lru_index].lru;
	    }
	}
      page_index = lru_index;

      // reset the oldest page
      PF_pages[page_index].page = page;
     
       int j;
      for(j=0; j<access_table_Size; j++)
	{
	  PF_pages[page_index].access_key1[j] = 0;
          PF_pages[page_index].access_key2[j] = 0;
          PF_pages[page_index].delta_pointer[j] = -1;
	  PF_pages[page_index].pf_map[j] = 0;
	}
     int k;
     for(k=0; k<delta_table_Size; k++)
         {PF_pages[page_index].delta_delta[k] = 0;
          PF_pages[page_index].delta_point[k] = -1;
         } 
     
     
    PF_pages[page_index].last_address1 = 0; 
    PF_pages[page_index].last_address2 = 0;
    PF_pages[page_index].top_access=-1;
    PF_pages[page_index].top_delta=-1;
    PF_pages[page_index].prev_delta_point=-1;
  
    }

  // update LRU
  PF_pages[page_index].lru = get_current_cycle(0);


  //calculate delta1 and delta2 
  int delta1;
  int delta2;
 
    //these are offsets of the 3 addresses with the granularity of bytes
    unsigned long long int page_offset = addr & 4095;
    unsigned long long int page_offset_lastaddr1 = (PF_pages[page_index].last_address1) & 4095;
    unsigned long long int page_offset_lastaddr2 = (PF_pages[page_index].last_address2) & 4095;

  delta1 = page_offset_lastaddr1 - page_offset_lastaddr2;
  delta2 = page_offset           - page_offset_lastaddr1; 
 
  //update delta_Table using previous address
  if(PF_pages[page_index].top_delta >-1)
    {PF_pages[page_index].delta_delta[PF_pages[page_index].top_delta] = delta2;
     PF_pages[page_index].delta_point[PF_pages[page_index].top_delta] = PF_pages[page_index].prev_delta_point;
for ( int count = 0; count< delta_table_Size; count++)
         {
          if( PF_pages[page_index].delta_point[count] == PF_pages[page_index].top_delta )
             {
              PF_pages[page_index].delta_point[count] = -1;
             }
         }
    }
  
  
  //checking the Access_key_table for a hit
    int hit = 0;
    int access_index;

    for(i=0; i< access_table_Size; i++)
       {
        if (delta1 == PF_pages[page_index].access_key1[i] && delta2 == PF_pages[page_index].access_key2[i])
           { hit = 1;
             access_index =  i;
             break;
           }
        }
  //if the pair does not hit in the access_key_table, then assign a new place and do not prefetch anything
    if (!hit) 
       { 
         PF_pages[page_index].top_access++;
        if(PF_pages[page_index].top_access <access_table_Size)
        access_index = PF_pages[page_index].top_access;
        else
         { 
         access_index = 0;
         PF_pages[page_index].top_access = 0;
         }
        PF_pages[page_index].access_key1[access_index] = delta1;
        PF_pages[page_index].access_key2[access_index] = delta2;
 
       }


  //if the pair hits, then access the delta_Table and prefetch lines according to the deltas, the PF_table and the Degree of Prefetching
  //Do not prefetch what is in the prefetch table
  //Update the prefetch table
  
    else
       {
        int count_prefetch;
        int trace_delta;
        int current_delta_pointer ,next_delta_pointer;
        next_delta_pointer = PF_pages[page_index].delta_pointer[access_index];
        current_delta_pointer = next_delta_pointer;
        unsigned long long int new_addr = addr;
        
        for(count_prefetch=1; count_prefetch <= Prefetch_Degree; count_prefetch++)
           {
              if (count_prefetch!=1)
                 {  if( trace_delta == PF_pages[page_index].delta_delta[next_delta_pointer] )
 
                      {if (current_delta_pointer >= delta_table_Size -1)
                          {
                           current_delta_pointer = 0;
                          }
                       else
                          {
                           current_delta_pointer++;
                          }      
                        trace_delta = PF_pages[page_index].delta_delta[current_delta_pointer];
                        new_addr = new_addr + trace_delta;
                      }
                    else
                      {
                       current_delta_pointer = next_delta_pointer;
                       trace_delta = PF_pages[page_index].delta_delta[current_delta_pointer];
                       new_addr = addr + trace_delta;
                      }
                 } 
              else
                 {
                  trace_delta = PF_pages[page_index].delta_delta[current_delta_pointer];
                  new_addr = addr + trace_delta;
                 }

            next_delta_pointer = PF_pages[page_index].delta_point[current_delta_pointer];
            //check the PF_Table 
            
            new_pageoffset =   (new_addr>>6) & 63;             // This page-offset has a granularity of cache line 
            
            unsigned long long int new_addr_page = new_addr>>12;
            
            //DO NOT Prefetch if Already Prefetched (not taking into account evictions)  
            if( (new_addr_page != PF_pages[page_index].page) || (PF_pages[page_index].pf_map[new_pageoffset] == 1) || (new_pageoffset == now_pageoffset))
              { //do not prefetch

              } 
            // Prefetch 
            else
              {
               // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
	       if(get_l2_mshr_occupancy(0) < 8)
	        {
	         l2_prefetch_line(0, addr, new_addr, FILL_L2);
                }
	       else
	        {
	         l2_prefetch_line(0, addr, new_addr, FILL_LLC); 
	        }
               // update the PF_map
               PF_pages[page_index].pf_map[new_pageoffset] = 1;
              
              }
             if (next_delta_pointer==-1)
               {
                break;
               }

           }  
       }
     
     // Update delta_table_pointer
     if(PF_pages[page_index].top_delta < delta_table_Size-1)
        PF_pages[page_index].top_delta++;
     else
        { 
        PF_pages[page_index].top_delta = 0;
        
        }
        PF_pages[page_index].prev_delta_point = PF_pages[page_index].delta_pointer[access_index];
        PF_pages[page_index].delta_pointer[access_index] = PF_pages[page_index].top_delta; 

    // Update last addresses
    PF_pages[page_index].last_address2 = PF_pages[page_index].last_address1;
    PF_pages[page_index].last_address1 = addr;
 
    //Update the PF_map with the present access
    PF_pages[page_index].pf_map[now_pageoffset] = 1;
   
   
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

