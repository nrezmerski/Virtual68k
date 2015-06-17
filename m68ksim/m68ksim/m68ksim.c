//
//  m68ksim.c
//  m68ksim
//
//  Created by Daniele Cattaneo on 28/12/14.
//  Copyright (c) 2014 Daniele Cattaneo. 
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include "m68ksim.h"
#include "addrspace.h"
#include "ram.h"
#include "musashi/m68k.h"
#include "elf.h"
#include "debugger.h"
#include "breakpoints.h"
#include "tty.h"
#include "error.h"


volatile int sim_on, debug_on;
int servermode_on;

struct timeval cyc_t0;
long long cyc_dcycles;


void signal_enterDebugger(int signo) {
  debug_on = 1;
}


int main(int argc, char *argv[]) {
  uint32_t availRam, stackTop, stackSize;
  int c, special;
  error_t *tmpe;
  
  sim_on = 0;
  debug_on = 0;
  servermode_on = 0;
  signal(SIGINT, signal_enterDebugger);
  mem_init();
  
  stackTop = ADDRSPACE_SIZE/2;
  stackSize = SEGM_GRANULARITY * 4;
  
  optind = 1;
  while (optind < argc) {
    special = 0;
    c = getopt(argc, argv, "Bdm:l:i:I:");
    if (c != -1) {
      switch (c) {
        case 'm':
          availRam = (unsigned int)strtoul(optarg, NULL, 0);
          if (availRam == 0)
            availRam = 0x800000;
          ram_install(0, availRam, &tmpe);
          iferror_die(tmpe);
          break;
          
        case 'I':
          special = 1;
        case 'i':
          if (strcasecmp(optarg, "tty") == 0)
            iferror_die(tty_installCommand(special, argc, argv));
          else
            iferror_die(error_new(101, "Unknown device type %s.\n", optarg));
          break;
          
        case 'l':
          iferror_die(elf_load(optarg));
          break;
        
        case 'd':
          debug_on = 1;
          break;
          
        case 'B':
          servermode_on = 1;
          break;
      }
    } else
      iferror_die(error_new(102, "Ignored unknown option %s.\n", argv[optind++]));
  }
  
  ram_install(stackTop - stackSize, stackSize, &tmpe);
  iferror_die(tmpe);
  ram_install(0, SEGM_GRANULARITY, NULL);
  
  m68k_write_memory_32(0, stackTop);
  error_drainPool();
  
  m68k_init();
  sim_on = 1;
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);
  m68k_set_instr_hook_callback(cpu_instrCallback);
  m68k_pulse_reset();
  
  gettimeofday(&cyc_t0, NULL);
  cyc_dcycles = 0;
  for (;;) {
    m68k_execute(CYCLES_PER_LOOP);
    
    if (cyc_dcycles > LONG_LONG_MAX - (CYCLES_PER_LOOP+1)) {
      cyc_dcycles = 0;
      gettimeofday(&cyc_t0, NULL);
    } else {
      cyc_dcycles += m68k_cycles_run();
    }
  }
  return 1;
}

