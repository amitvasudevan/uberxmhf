/*
	scheduler hypapp

	author: amit vasudevan (amitvasudevan@acm.org)
*/

#include <types.h>
#include <arm8-32.h>
#include <bcm2837.h>
#include <miniuart.h>
#include <debug.h>

#include <hypmtscheduler.h>


//////
// externs
//////

extern void uapp_sched_fiq_handler(void);
extern u32 uapp_sched_fiqhandler_stack[];
extern u32 uapp_sched_fiqhandler_stack_top[];
extern __attribute__(( section(".data") )) u32 priority_queue_lock=1;
extern __attribute__(( section(".data") )) u32 hypmtscheduler_execution_lock=1;

extern void uapp_hypmtsched_schedentry(void);

//////
// forward function prototypes
//////

u64 uapp_sched_read_cpucounter(void);
void_uapp_sched_logic(void);


//////
// global variables
//////

volatile u32 fiq_sp = 0;
volatile u32 fiq_cpsr = 0;
volatile u32 fiq_pemode= 0;
volatile u32 normal_sp = 0;

__attribute__((section(".data"))) volatile u32 fiq_timer_handler_timerevent_triggered = 0;
__attribute__((section(".data"))) volatile u32 fiq_timer_handler_guestmode_pc = 0;
__attribute__((section(".data"))) volatile u32 fiq_timer_handler_guestmode_spsr = 0;

__attribute__((section(".data"))) struct sched_timer sched_timers[MAX_TIMERS];   // set of timers
__attribute__((section(".data"))) struct sched_timer *timer_next = NULL; // timer we expect to run down next
__attribute__((section(".data"))) TIME time_timer_set;    // time when physical timer was set

__attribute__((section(".data"))) struct sched_timer timer_last = {
  FALSE,
  FALSE,
  0,
  VERY_LONG_TIME,
  VERY_LONG_TIME
};

__attribute__((section(".data"))) volatile u8 thread1_event = FALSE;
__attribute__((section(".data"))) volatile u8 thread2_event = FALSE;





//////
// initialize timer data structures
//////
void uapp_sched_timers_init(void){
  u32 i;

  for(i=0; i < MAX_TIMERS; i++)
	  sched_timers[i].inuse = FALSE;
}


//////
// enable FIQs
//////
void enable_fiq(void){
	u32 cpsr;
	cpsr = sysreg_read_cpsr();
	cpsr &= ~(1UL << 6);	//clear CPSR.F to allow FIQs
	sysreg_write_cpsr(cpsr);
}

//////
// disable FIQs
//////
void disable_fiq(void){
	u32 cpsr;
	cpsr = sysreg_read_cpsr();
	cpsr |= (1UL << 6);	//set CPSR.F to prevent FIQs
	sysreg_write_cpsr(cpsr);
}



//////
// undeclare (and disable) a timer
//////
void uapp_sched_timer_undeclare(struct sched_timer *t){
	//disable_fiq();

	if (!t->inuse) {
		enable_fiq();
		return;
	}

	t->inuse = FALSE;

	// check if we were waiting on this one
	if (t == timer_next) {
		uapp_sched_timers_update(uapp_sched_read_cpucounter() - time_timer_set);
		if (timer_next) {
			uapp_sched_start_physical_timer(timer_next->time_to_wait);
			time_timer_set = uapp_sched_read_cpucounter();
		}
	}

	//enable_fiq();
}


//////
// declare a timer
// time = time to wait in clock ticks
// returns NULL if something went wrong
//////
//struct sched_timer *uapp_sched_timer_declare(u32 time, char *event, int priority){
//struct sched_timer *uapp_sched_timer_declare(u32 time, HYPTHREADFUNC func, int priority){
struct sched_timer *uapp_sched_timer_declare(u32 first_time_period,
		u32 regular_time_period, int priority, HYPTHREADFUNC func){
	struct sched_timer *t;

  //disable_fiq();

  for (t=sched_timers;t<&sched_timers[MAX_TIMERS];t++) {
    if (!t->inuse) break;
  }

  // out of timers?
  if (t == &sched_timers[MAX_TIMERS]) {
    //enable_fiq();
    return(0);
  }

  // install new timer
  //t->event = event;
  t->event = FALSE;
  t->disable_tfunc = FALSE;
  t->regular_time_period = regular_time_period;
  t->first_time_period = first_time_period;
  t->priority = priority;
  t->tfunc = func;

  t->first_time_period_expired = 0;
  t->time_to_wait = first_time_period;
  t->sticky_time_to_wait = regular_time_period;

  //_XDPRINTF_("%s,%u: event=%u, time_to_wait=%016llx, sticky_time_to_wait=%016llx, priority=%u\n",
	//	  __func__, __LINE__,
	//		t->event, t->time_to_wait, t->sticky_time_to_wait, t->priority);


  if (!timer_next) {
    // no timers set at all, so this is shortest
    time_timer_set = uapp_sched_read_cpucounter();
    uapp_sched_start_physical_timer((timer_next = t)->time_to_wait);
	//_XDPRINTF_("%s,%u: ENTER, time_to_wait=%016llx\n", __func__, __LINE__,
	//		t->time_to_wait);
	//bcm2837_miniuart_puts("[HYPSCHED]: shortest timer set val=0x\n");
    //debug_hexdumpu32(t->time_to_wait);
	//bcm2837_miniuart_puts("\n");



  } else if ((first_time_period + uapp_sched_read_cpucounter()) < (timer_next->time_to_wait + time_timer_set)) {
    // new timer is shorter than current one, so
    uapp_sched_timers_update(uapp_sched_read_cpucounter() - time_timer_set);
    time_timer_set = uapp_sched_read_cpucounter();
    uapp_sched_start_physical_timer((timer_next = t)->time_to_wait);
	//_XDPRINTF_("%s,%u: ENTER\n", __func__, __LINE__);
  } else {
    // new timer is longer, than current one
	//_XDPRINTF_("%s,%u: ENTER, time_to_wait=%016llx\n", __func__, __LINE__,
	//		t->time_to_wait);
  }

  t->inuse = TRUE;

  //enable_fiq();

  return(t);
}


//////
// redeclare an expired timer
//////
struct sched_timer *uapp_sched_timer_redeclare(struct sched_timer *t, u32 first_time_period,
		u32 regular_time_period, int priority, HYPTHREADFUNC func){

	t->event = FALSE;
	t->disable_tfunc = FALSE;
	t->first_time_period = first_time_period;
	t->regular_time_period = regular_time_period;
	t->priority = priority;
	t->tfunc = func;

	t->first_time_period_expired = 0;
	t->time_to_wait = first_time_period;
	t->sticky_time_to_wait = regular_time_period;

	if (!timer_next) {
		// no timers set at all, so this is shortest
		time_timer_set = uapp_sched_read_cpucounter();
		uapp_sched_start_physical_timer((timer_next = t)->time_to_wait);

	} else if ((first_time_period + uapp_sched_read_cpucounter()) < (timer_next->time_to_wait + time_timer_set)) {
	    // new timer is shorter than current one, so update all timers
		// and set this one as the shortest timer
	    uapp_sched_timers_update(uapp_sched_read_cpucounter() - time_timer_set);
	    time_timer_set = uapp_sched_read_cpucounter();
	    uapp_sched_start_physical_timer((timer_next = t)->time_to_wait);

	} else {
	    // new timer is longer, than current one, so we dont do anything
	}

	t->inuse = TRUE;

	return t;
}





//////
// subtract time from all timers, enabling those that run out
//////
void uapp_sched_timers_update(TIME time){
  struct sched_timer *t;

  timer_next = &timer_last;

  //_XDPRINTF_("%s,%u: ENTER: time=%016llx\n", __func__, __LINE__, time);

  for (t=sched_timers;t<&sched_timers[MAX_TIMERS];t++) {
    if (t->inuse) {
      if (time < t->time_to_wait) { // unexpired
  		//_XDPRINTF_("%s,%u: ENTER: time_to_wait=%016llx\n", __func__, __LINE__,
  		//		t->time_to_wait);
  		//_XDPRINTF_("%s,%u: timer_next->time_to_wait=%016llx\n", __func__, __LINE__,
  		//		timer_next->time_to_wait);
  		t->time_to_wait -= time;
        if (t->time_to_wait < timer_next->time_to_wait){
          timer_next = t;
    		//_XDPRINTF_("%s,%u: ENTER\n", __func__, __LINE__);
        }
      } else { // expired
        /* tell scheduler */
		//_XDPRINTF_("%s,%u: ENTER\n", __func__, __LINE__);
    	t->event = TRUE;
        t->inuse = FALSE; 	// remove timer
        fiq_timer_handler_timerevent_triggered=1; //set timerevent_triggered to true
        //spin_lock(&priority_queue_lock);
		//priority_queue_insert((void *)t, t->priority);
		//spin_unlock(&priority_queue_lock);
		//_XDPRINTFSMP_("%s,%u: inserted 0x%08x with priority=%d\n", __func__, __LINE__,
		//		t, t->priority);
		//_XDPRINTFSMP_("\n%s: task timer priority=%d expired!\n", __func__, t->priority);
    	//bcm2837_miniuart_puts("\n[HYPSCHED]: Task timer expired. Priority=0x");
    	//debug_hexdumpu32(t->priority);
    	//bcm2837_miniuart_puts(" recorded.\n");

        //uapp_sched_timer_declare(t->sticky_time_to_wait, NULL, t->priority);
      }
    }
  }

  /* reset timer_next if no timers found */
  if (!timer_next->inuse) {
	  timer_next = 0;
		//_XDPRINTF_("%s,%u: ENTER\n", __func__, __LINE__);
  }
}


//////
// read current physical counter for the CPU; we use this as current time
//////
u64 uapp_sched_read_cpucounter(void){
	return sysreg_read_cntpct();
}


//////
// start physical timer to fire off after specified clock ticks
//////
void uapp_sched_start_physical_timer(TIME time){
	//_XDPRINTFSMP_("%s: time=%u\n", __func__, (u32)time);
	//bcm2837_miniuart_puts("\n[HYPSCHED:start_physical_timer: period=0x");
	//debug_hexdumpu32((u32)time);
	//bcm2837_miniuart_puts("\n");

	sysreg_write_cnthp_tval(time);
	sysreg_write_cnthp_ctl(0x1);
}

//////
// stop physical timer
//////
void uapp_sched_stop_physical_timer(void){
	sysreg_write_cnthp_ctl(0x0);
}



//////
// scheduler timer event processing
//////
void uapp_sched_process_timers(u32 cpuid){
	u32 i;
	u32 time_to_wait;
	int priority;

	for(i=0; i < MAX_TIMERS; i++){
		if(sched_timers[i].event){
			sched_timers[i].event = FALSE;
			if(sched_timers[i].tfunc && !sched_timers[i].disable_tfunc){
				priority_queue_insert((void *)&sched_timers[i], sched_timers[i].priority);
			}

			time_to_wait = sched_timers[i].regular_time_period; //reload
			priority = sched_timers[i].priority;
			//uapp_sched_timer_declare(time_to_wait, time_to_wait, priority, sched_timers[i].tfunc);
			uapp_sched_timer_redeclare(&sched_timers[i], time_to_wait, time_to_wait, priority, sched_timers[i].tfunc);
		}
	}
}






//////
// uapp sched timer_initialize
// initialize hypervisor timer functionality
//////
void uapp_sched_timer_initialize(u32 cpuid){
	u32 cpsr;
	u64 cntpct_val;
	u32 cpu0_tintctl_value;
	u32 loop_counter=0;

	_XDPRINTFSMP_("%s[%u]: ENTER\n", __func__, cpuid);

	//disable FIQs
	disable_fiq();
	cpsr = sysreg_read_cpsr();
	_XDPRINTFSMP_("%s[%u]: CPSR[after disable_fiq]=0x%08x; CPSR.A=%u, CPSR.I=%u, CPSR.F=%u\n",
			__func__, cpuid, cpsr, ((cpsr & (1UL << 8)) >> 8),
			((cpsr & (1UL << 7)) >> 7),
			((cpsr & (1UL << 6)) >> 6) );


	//enable cpu0 timer interrupt control to generate FIQs
	cpu0_tintctl_value = mmio_read32(LOCAL_TIMER_INT_CONTROL0);
	_XDPRINTFSMP_("%s[%u]: cpu0_tintctl_value[before]=0x%08x, CNTHPFIQ=%u, CNTHPIRQ=%u\n",
			__func__, cpuid,
			cpu0_tintctl_value,
			((cpu0_tintctl_value & (1UL << 6)) >> 6),
			((cpu0_tintctl_value & (1UL << 2)) >> 2)
			);

	cpu0_tintctl_value &= ~(1UL << 2); //disable IRQs
	cpu0_tintctl_value |= (1UL << 6); //enable FIQs
	mmio_write32(LOCAL_TIMER_INT_CONTROL0, cpu0_tintctl_value);


	cpu0_tintctl_value = mmio_read32(LOCAL_TIMER_INT_CONTROL0);
	_XDPRINTFSMP_("%s[%u]: cpu0_tintctl_value[after]=0x%08x, CNTHPFIQ=%u, CNTHPIRQ=%u\n",
			__func__, cpuid,
			cpu0_tintctl_value,
			((cpu0_tintctl_value & (1UL << 6)) >> 6),
			((cpu0_tintctl_value & (1UL << 2)) >> 2)
			);




	//enable FIQs
	//enable_fiq();
	cpsr = sysreg_read_cpsr();
	_XDPRINTFSMP_("%s[%u]: CPSR[after enable_fiq]=0x%08x; CPSR.A=%u, CPSR.I=%u, CPSR.F=%u\n",
			__func__, cpuid, cpsr, ((cpsr & (1UL << 8)) >> 8),
			((cpsr & (1UL << 7)) >> 7),
			((cpsr & (1UL << 6)) >> 6) );

	_XDPRINTFSMP_("%s[%u]: EXIT\n", __func__, cpuid);
}


//void uapp_sched_fiqhandler(u32 debug_val){
void uapp_sched_fiqhandler(void){

#if 0
	fiq_cpsr = sysreg_read_cpsr();
	bcm2837_miniuart_puts("\n[HYPTIMER]: Fired!: ");

	bcm2837_miniuart_puts("CPSR.A=0x");
	debug_hexdumpu32(((fiq_cpsr & (1UL << 8)) >> 8));
	bcm2837_miniuart_puts(" ");

	bcm2837_miniuart_puts("CPSR.I=0x");
	debug_hexdumpu32(((fiq_cpsr & (1UL << 7)) >> 7));
	bcm2837_miniuart_puts(" ");

	bcm2837_miniuart_puts("CPSR.F=0x");
	debug_hexdumpu32(((fiq_cpsr & (1UL << 6)) >> 6));
	bcm2837_miniuart_puts("\n");

	bcm2837_miniuart_puts("\n debug_val=0x");
	debug_hexdumpu32(debug_val);
	bcm2837_miniuart_puts("\n");

	bcm2837_miniuart_puts("\n sp=0x");
	debug_hexdumpu32(sysreg_read_sp());
	bcm2837_miniuart_puts("\n");

	bcm2837_miniuart_puts("\n stacktop=0x");
	debug_hexdumpu32(&uapp_sched_fiqhandler_stack_top);
	bcm2837_miniuart_puts("\n");


	//bcm2837_miniuart_puts("\n[HYPTIMER]: Halting!\n");
	//HALT();
#endif


	//fiq_sp = sysreg_read_sp();
	//_XDPRINTFSMP_("%s: Timer Fired: sp=0x%08x, cpsr=0x%08x\n", __func__,
	//		fiq_sp, sysreg_read_cpsr());
	//bcm2837_miniuart_puts("\n[HYPTIMER]: Fired!!\n");
    spin_lock(&hypmtscheduler_execution_lock);
	uapp_sched_timerhandler();
    spin_unlock(&hypmtscheduler_execution_lock);

	//bcm2837_miniuart_puts("\n[HYPTIMER]: Fired!!\n");
	//uapp_sched_start_physical_timer(3 * 20 * 1024 * 1024);
	//_XDPRINTFSMP_("%s: resuming\n", __func__);

}


void uapp_sched_timerhandler(void){
	//bcm2837_miniuart_puts("\n[HYPTIMER]: Fired\n");

	//stop physical timer
	uapp_sched_stop_physical_timer();

	//set timerevent_triggered flag to false (0)
	fiq_timer_handler_timerevent_triggered=0;

	//update sw timers
	uapp_sched_timers_update(uapp_sched_read_cpucounter() - time_timer_set);

	// start physical timer for next shortest time if one exists
	if (timer_next) {
		time_timer_set = uapp_sched_read_cpucounter();
		uapp_sched_start_physical_timer(timer_next->time_to_wait);
	}

	if (fiq_timer_handler_timerevent_triggered == 0){
		//no timers expired so just return from timer interrupt
    	//bcm2837_miniuart_puts("\n[HYPTIMER]: No timers expired EOI: elr_hyp=0x");
    	//debug_hexdumpu32(sysreg_read_elrhyp());
    	//bcm2837_miniuart_puts("spsr_hyp=0x");
    	//debug_hexdumpu32(sysreg_read_spsr_hyp());
    	//bcm2837_miniuart_puts("\n");
    	return;

	}else{
		//timer has expired, so let us look at the PE state
		//which triggered this timer FIQ to decide on our course of
		//action
		fiq_pemode = sysreg_read_spsr_hyp() & 0x0000000FUL;
		if(fiq_pemode == 0xA){
			//PE state was hyp mode, so we simply resume
	    	//bcm2837_miniuart_puts("\n[HYPTIMER]: Timer expired, PE state=HYP, queuing...\n");
	    	//HALT();
			return;
		}else{
			//PE state says we are in guest mode
			fiq_timer_handler_guestmode_pc = sysreg_read_elrhyp();
			fiq_timer_handler_guestmode_spsr = sysreg_read_spsr_hyp();
	    	//bcm2837_miniuart_puts("\n[HYPTIMER]: PE state=GUEST, PC=0x");
	    	//debug_hexdumpu32(fiq_timer_handler_guestmode_pc);
	    	//bcm2837_miniuart_puts(" SPSR=0x");
	    	//debug_hexdumpu32(fiq_timer_handler_guestmode_spsr);
	    	//bcm2837_miniuart_puts("\n");

	    	//bcm2837_miniuart_puts("Halting. Wip!\n");
	    	//HALT();

	    	//write scheduler main to elr_hyp
	    	//read spsr_hyp; change mode to hyp with all A, I and F masks set
	    	//0x000001DA
	    	//issue eret
	    	sysreg_write_elrhyp(&uapp_hypmtsched_schedentry);
	    	sysreg_write_spsr_hyp(0x000001DA);
	    	//cpu_eret();
	    	return;
		}
	}

	//uapp_sched_logic();
}





void uapp_sched_run_hyptasks(void){
	int status;
	u32 queue_data;
	int priority;
	struct sched_timer *task_timer;


	status=0;
	while(1){
		status = priority_queue_remove(&queue_data, &priority);
		if(status == 0)
			break;
		task_timer = (struct sched_timer *)queue_data;

		//interrupts enable
		enable_fiq();

		//bcm2837_miniuart_puts("\n[HYPSCHED]: HypTask completed run with Priority=0x");
    	//debug_hexdumpu32(task_timer->priority);
		if(task_timer->tfunc)
			task_timer->tfunc(task_timer);

		//interrupts disable
		disable_fiq();
	}

}


void uapp_sched_logic(void){
	struct sched_timer *task_timer;
	u32 queue_data;
	int priority;
	int status;
	volatile u32 sp, spnew;

	//bcm2837_miniuart_puts("\n[HYPSCHED]: Came in. Halting Wip!\n");
	//HALT();

	uapp_sched_process_timers(0); //TBD: remove hard-coded cpuid (0)
	//uapp_sched_run_hyptasks();

	#if 1
	while(1){
		uapp_sched_run_hyptasks();
		uapp_sched_process_timers(0); //TBD: remove hard-coded cpuid (0)
		if(priority_queue_isempty())
			break;
	}
	#endif

	//bcm2837_miniuart_puts("\n[HYPSCHED]: Finished all HypTasks. Now resuming guest...\n");
	//bcm2837_miniuart_puts("\n[HYPSCHED]: Halting WiP!\n");
	//HALT();

	//resume guest
	sysreg_write_elrhyp(fiq_timer_handler_guestmode_pc);
   	sysreg_write_spsr_hyp(fiq_timer_handler_guestmode_spsr);
   	fiq_timer_handler_guestmode_pc = 0;
   	fiq_timer_handler_guestmode_spsr = 0;
   	//cpu_eret();
   	return;
}



//////////////////////////////////////////////////////////////////////////////
// hyptask definitions
//////////////////////////////////////////////////////////////////////////////

void hyptask0(struct sched_timer *t){
	bcm2837_miniuart_puts("\n[HYPSCHED]: HypTask-0 completed run with Priority=0x");
	debug_hexdumpu32(t->priority);

}

void hyptask1(struct sched_timer *t){
	bcm2837_miniuart_puts("\n[HYPSCHED]: HypTask-1 completed run with Priority=0x");
	debug_hexdumpu32(t->priority);
}

void hyptask2(struct sched_timer *t){
	bcm2837_miniuart_puts("\n[HYPSCHED]: HypTask-2 completed run with Priority=0x");
	debug_hexdumpu32(t->priority);
}

void hyptask3(struct sched_timer *t){
	bcm2837_miniuart_puts("\n[HYPSCHED]: HypTask-3 completed run with Priority=0x");
	debug_hexdumpu32(t->priority);
}

__attribute__((section(".data"))) HYPTHREADFUNC hyptask_idlist[HYPMTSCHEDULER_MAX_HYPTASKID] =
		{
			&hyptask0,
			&hyptask1,
			&hyptask2,
			&hyptask3
		};

__attribute__((section(".data"))) hypmtscheduler_hyptask_handle_t hyptask_handle_list[HYPMTSCHEDULER_MAX_HYPTASKS];




//////////////////////////////////////////////////////////////////////////////
// hypmtscheduler hypercall APIs
//////////////////////////////////////////////////////////////////////////////

// create hyptask API
void uapp_hypmtscheduler_handlehcall_createhyptask(ugapp_hypmtscheduler_param_t *hmtsp){
	uint32_t hyptask_first_period = hmtsp->iparam_1;
	uint32_t hyptask_regular_period = hmtsp->iparam_2;
	uint32_t hyptask_priority = hmtsp->iparam_3;
	uint32_t hyptask_id = hmtsp->iparam_4;
	uint32_t i;
	uint32_t hyptask_handle_found;

	bcm2837_miniuart_puts("\n[HYPMTSCHED: CREATEHYPTASK]: first period=0x");
	debug_hexdumpu32(hyptask_first_period);
	bcm2837_miniuart_puts(", regular period=0x");
	debug_hexdumpu32(hyptask_regular_period);
	bcm2837_miniuart_puts(", priority=0x\n");
	debug_hexdumpu32(hyptask_priority);
	bcm2837_miniuart_puts("\n");
	bcm2837_miniuart_puts(", hyptask id=0x\n");
	debug_hexdumpu32(hyptask_id);
	bcm2837_miniuart_puts("\n");

	//allocate hyptask_handle
	hyptask_handle_found=0;
	for(i=0; i< HYPMTSCHEDULER_MAX_HYPTASKS; i++){
		if(!hyptask_handle_list[i].inuse){
			hyptask_handle_list[i].inuse = true;
			hyptask_handle_found=1;
			break;
		}
	}

	//check if we were able to allocate a hytask handle
	if(!hyptask_handle_found){
		hmtsp->status=0; //fail
		return;
	}

	//now check if the given hyptask_id is valid
	if(hyptask_id > (HYPMTSCHEDULER_MAX_HYPTASKID-1) ){
		hmtsp->status=0; //fail
		return;
	}

	//ok now populate the hyptask_id within the hyptask handle
	hyptask_handle_list[i].hyptask_id = hyptask_id;

	hyptask_handle_list[i].t = uapp_sched_timer_declare(hyptask_first_period, hyptask_regular_period,
			hyptask_priority,
			hyptask_idlist[hyptask_id]);

	//check if we were able to allocate a timer for the hyptask
	if(!hyptask_handle_list[i].t){
		hmtsp->status=0; //fail
		return;
	}

	bcm2837_miniuart_puts("\n[HYPMTSCHED: CREATEHYPTASK]: struct sched_timer=0x");
	debug_hexdumpu32((uint32_t)hyptask_handle_list[i].t);
	bcm2837_miniuart_puts("\n");

	hmtsp->oparam_1 = i;	//return hyptask handle
	hmtsp->status=1;	//success
}


// disable hyptask API
void uapp_hypmtscheduler_handlehcall_disablehyptask(ugapp_hypmtscheduler_param_t *hmtsp){
	uint32_t hyptask_handle = hmtsp->iparam_1;
	struct sched_timer *hyptask_timer;
	uint32_t i;
	uint32_t hyptask_handle_found;

	bcm2837_miniuart_puts("\n[HYPMTSCHED: DISABLEHYPTASK]: hyptask_handle=0x");
	debug_hexdumpu32(hyptask_handle);
	bcm2837_miniuart_puts("\n");

	//check if provided hyptask handle is within limits
	if(hyptask_handle >= HYPMTSCHEDULER_MAX_HYPTASKS){
		hmtsp->status=0; //fail
		return;
	}

	//check if provided hyptask handle is in use
	if(!hyptask_handle_list[hyptask_handle].inuse){
		hmtsp->status=0; //fail
		return;
	}

	//ok grab the timer for the hyptask
	hyptask_timer = hyptask_handle_list[hyptask_handle].t;

	//set the disabled flag so that hyptask function is not executed
	hyptask_timer->disable_tfunc = TRUE;

	hmtsp->status=1; //success

	bcm2837_miniuart_puts("\n[HYPMTSCHED: DISABLEHYPTASK]: struct sched_timer=0x");
	debug_hexdumpu32((uint32_t)hyptask_timer);
	bcm2837_miniuart_puts("\n");

	HALT();
}




// top-level hypercall handler hub
// return true if handled the hypercall, false if not
bool uapp_hypmtscheduler_handlehcall(u32 uhcall_function, void *uhcall_buffer,
		u32 uhcall_buffer_len){
	ugapp_hypmtscheduler_param_t *hmtsp;

	if(uhcall_function != UAPP_HYPMTSCHEDULER_UHCALL){
		return false;
	}

    spin_lock(&hypmtscheduler_execution_lock);

	hmtsp = (ugapp_hypmtscheduler_param_t *)uhcall_buffer;

	if(hmtsp->uhcall_fn == UAPP_HYPMTSCHEDULER_UHCALL_CREATEHYPTASK){
		uapp_hypmtscheduler_handlehcall_createhyptask(hmtsp);

	}else if(hmtsp->uhcall_fn == UAPP_HYPMTSCHEDULER_UHCALL_DISABLEHYPTASK){
		uapp_hypmtscheduler_handlehcall_disablehyptask(hmtsp);

	}else{
		bcm2837_miniuart_puts("\nHYPMTSCHED: UHCALL: ignoring unknown uhcall_fn=0x");
		debug_hexdumpu32(hmtsp->uhcall_fn);
		bcm2837_miniuart_puts("\n");
	}

    spin_unlock(&hypmtscheduler_execution_lock);

	return true;
}



//////////////////////////////////////////////////////////////////////////////
// main hypmtscheduler initialization function
//////////////////////////////////////////////////////////////////////////////

void uapp_sched_initialize(u32 cpuid){
	int value;
	int priority;
	struct sched_timer *task_timer;
	u32 queue_data;
	int status;
	volatile u32 sp, spnew;


	if(cpuid == 0){
		_XDPRINTFSMP_("%s[%u]: Initializing scheduler...\n", __func__, cpuid);

		normal_sp =sysreg_read_sp();
		_XDPRINTFSMP_("%s[%u]: FIQ Stack pointer base=0x%08x\n", __func__, cpuid,
				&uapp_sched_fiqhandler_stack);
		_XDPRINTFSMP_("%s[%u]: normal_sp=0x%08x\n", __func__, cpuid, normal_sp);
		_XDPRINTFSMP_("%s[%u]: cpsr=0x%08x\n", __func__, cpuid, sysreg_read_cpsr());

		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		//zero-initialize hyptask_handle_list
		memset(&hyptask_handle_list, 0, sizeof(hyptask_handle_list));

		//initialize timers
		uapp_sched_timer_initialize(cpuid);

		//declare a keep-alive timer to initialize timer subsystem
		uapp_sched_timer_declare(3 * 20 * 1024 * 1024, 3 * 20 * 1024 * 1024, 1, NULL);

	}else{
		_XDPRINTFSMP_("%s[%u]: AP CPU: nothing to do, moving on...\n", __func__, cpuid);
	}

}
