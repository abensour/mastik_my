/*
 * Copyright 2016 CSIRO
 *
 * This file is part of Mastik.
 *
 * Mastik is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mastik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mastik.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

#include <util.h>

#include "l3.h"

#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>


// CPU cycles in MHz
#define CPUMHZ 3200

// Sample rate in msec
#define SLOT 2

// Step size for cache probes. That is, probe every STEP cache sets
//synchronize with l3.c
#define STEP 2

#define SLOTCYCLES (SLOT * 1000 * CPUMHZ)

#define SETS_PER_PAGE 64


#define EXPANSION 64
//#define EXPANSION 1
// comment the previous line and uncomment the following line to easily disable the index expansion
//#define EXPANDED_INDEX(index) (index)
#define EXPANDED_INDEX(index)  (((index) &0x1f ) | (((index )& ~0x1f)<<6))



int main(int ac, char **av) {
    char str[20];
    int sample_time=30000;
    int samples;
    int pid = 0;
       if(ac>2){
        pid = atoi(av[2]);
        }
    
    // Yossi: open the perf counter
    // Source: https://elixir.free-electrons.com/linux/latest/source/samples/bpf/tracex6_user.c#L138
#define SAMPLE_PERIOD  0x7fffffffffffffffULL
    struct perf_event_attr  attr_llc_miss = {
        //    .freq = 0,
        //    .sample_period = SAMPLE_PERIOD,
        //    .inherit = 0,
        .type = PERF_TYPE_RAW,
        //    .read_format = 0,
        //    .sample_type = 0,
        //        .config = (PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)};
        .config=0x412e};
    /*    PERF_COUNT_HW_CACHE_RESULT_MISS | PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8)};
     PERF_COUNT_HW_CACHE_LL |
     (PERF_COUNT_HW_CACHE_OP_READ << 8) |
     (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)}; */
    
    //    perf_llc_reads_fd = sys_perf_event_open(&perf_event_attr, 0, -1, -1, 0);
    perf_llc_reads_fd = syscall(__NR_perf_event_open, &attr_llc_miss,  pid, -1, -1, 0);
    if (perf_llc_reads_fd < 0) {
        perror("Could not open the perf event");
        exit(1);
    }
    
    // Activate the counter
    ioctl(perf_llc_reads_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_llc_reads_fd, PERF_EVENT_IOC_ENABLE, 0);
    
    
    
    if(ac>1 ){
        sample_time = atoi(av[1]);
    }
    int nsets;
    l3pp_t l3 = NULL;
    
    do {
        
        if (l3 != NULL)
            l3_release(l3);
        l3 = l3_prepare(NULL);
        nsets = l3_getSets(l3);
    } while (nsets != 8192);
    

    samples = sample_time / SLOT;
    int nmonitored = nsets/STEP;
    for (int i = 0; i < nsets; i += STEP)
        l3_monitor(l3, i);
    l3_randomise(l3);

    //lishay addition
    printf("cache slices %d, cache sets", l3_getSlices(l3), l3_getSets(l3));
    //end lishay addition 
    uint16_t *res = calloc(samples, sizeof(uint16_t));
    for (int i = 0; i < samples; i+= 1)
        res[i] = 0;
    // Squash the monitored sets into 32 pagesum sets
    l3_compress_monitored_sets(l3);
    l3_repeatedprobeperf_compressed_pp(l3, samples, res, SLOTCYCLES);
        for (int i = 0; i < samples; i++)
            printf("%u ", res[i]);
        putchar('\n');
    
    free(res);
    l3_release(l3);
}

