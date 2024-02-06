#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iomanip>
#include "sim.h"
#include "cache.cc"

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main(int argc, char *argv[])
{
   FILE *fp;              // File pointer.
   char *trace_file;      // This variable holds the trace file name.
   cache_params_t params; // Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;               // This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;         // This variable holds the request's address obtained from the trace.
                          // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.
   bool L1_or_L2;
   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9)
   {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }

   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t)atoi(argv[1]);
   params.L1_SIZE = (uint32_t)atoi(argv[2]);
   params.L1_ASSOC = (uint32_t)atoi(argv[3]);
   params.L2_SIZE = (uint32_t)atoi(argv[4]);
   params.L2_ASSOC = (uint32_t)atoi(argv[5]);
   params.PREF_N = (uint32_t)atoi(argv[6]);
   params.PREF_M = (uint32_t)atoi(argv[7]);
   trace_file = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *)NULL)
   {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   // printf("===================================\n");

   Cache L1_cache(params, 0);
   // std::cout << "0000000000000000000000000000000000000000000" << std::endl;

   // To see have L2 or not
   L1_or_L2 = (params.L2_ASSOC > 0) && (params.L2_SIZE > 0);
   Cache L2_cache(params, L1_or_L2);

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2)
   { // Stay in the loop if fscanf() successfully parsed two tokens as specified.

      // if (rw == 'r')
      //    printf("r %x\n", addr);
      // else if (rw == 'w')
      //    printf("w %x\n", addr);
      // else
      // {
      //    printf("Error: Unknown request type %c.\n", rw);
      //    exit(EXIT_FAILURE);
      // }

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////

      // std::cout << "0000000000000000000000000000000000000000000" << std::endl;
      uint32_t old_evicted_addr;
      cache_status_msg L1_status; // To check hit、miss、evict
      std::tie(L1_status, old_evicted_addr) = L1_cache.request_no_pref(rw, addr);

      if (0 < params.L2_SIZE && L1_status == EVICT)
      { // if L1 evicted do request('w', evicted_old_addr)

         L2_cache.request('w', old_evicted_addr);
         // std::cout << "////////////////////////////////////////" << std::endl;
         L2_cache.request('r', addr);
         //{msg, evict_addr2} = L1_cache.request(rw, addr);
      }

      if (0 < params.L2_SIZE && L1_status == MISS)
      {
         L2_cache.request('r', addr);
      }
   }
   // L1.print();
   // L2.print();
   printf("===== L1 contents =====\n");
   L1_cache.print_content();
   if (0 < params.L2_SIZE)
   {
      printf("\n");
      printf("===== L2 contents =====\n");
      L2_cache.print_content();
   }

   std::cout << "" << std::endl;
   std::cout << "===== Measurements =====" << std::endl;
   std::cout << "a. L1 reads:" << L1_cache.cnt_Read << std::endl;
   std::cout << "b. L1 read misses:" << L1_cache.cnt_Read_miss << std::endl;
   std::cout << "c. L1 writes: " << L1_cache.cnt_Write << std::endl;
   std::cout << "d. L1 write misses:" << L1_cache.cnt_Write_miss << std::endl;
   std::cout << "e. L1 miss rate:" << std::fixed << std::setprecision(4) << static_cast<float>((L1_cache.cnt_Read_miss + L1_cache.cnt_Write_miss) / static_cast<float>(L1_cache.cnt_Read + L1_cache.cnt_Write)) << std::endl;
   std::cout << "f. L1 writebacks:" << L1_cache.cnt_writebacks << std::endl;
   std::cout << "g. L1 prefetches:"
             << "0" << std::endl;

   std::cout << "h. L2 reads (demand):" << L2_cache.cnt_Read << std::endl;
   std::cout << "i. L2 read misses (demand):" << L2_cache.cnt_Read_miss << std::endl;
   std::cout << "j. L2 reads (prefetch):"
             << "0" << std::endl;
   std::cout << "k. L2 read misses (prefetch):"
             << "0" << std::endl;

   std::cout << "l. L2 writes:" << L2_cache.cnt_Write << std::endl;
   std::cout << "m. L2 write misses:" << L2_cache.cnt_Write_miss << std::endl;
   std::cout << "n. L2 miss rate: " << std::fixed << std::setprecision(4) << static_cast<float>((L2_cache.cnt_Read_miss) / static_cast<float>(L1_cache.cnt_Read_miss + L1_cache.cnt_Write_miss)) << std::endl;
   std::cout << "o. L2 writebacks:" << L2_cache.cnt_writebacks << std::endl;
   std::cout << "p. L2 prefetches:"
             << "0" << std::endl;
   if (0 < params.L2_SIZE)
      std::cout << "q. memory traffic: " << L2_cache.cnt_Read_miss + L2_cache.cnt_Write_miss + L2_cache.cnt_writebacks << std::endl;
   else
      std::cout << "q. memory traffic: " << L1_cache.cnt_Read_miss + L1_cache.cnt_Write_miss + L1_cache.cnt_writebacks << std::endl;

   return (0);
}
