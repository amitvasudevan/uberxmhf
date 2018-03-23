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
extern __attribute__(( section(".data") )) u32 priority_queue_lock=1;


//////
// forward function prototypes
//////

u64 uapp_sched_read_cpucounter(void);
void_uapp_sched_logic(void);


//////
// global variables
//////

volatile u32 fiq_sp = 0;
volatile u32 normal_sp = 0;

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
	disable_fiq();

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

	enable_fiq();
}


//////
// declare a timer
// time = time to wait in clock ticks
// returns NULL if something went wrong
//////
struct sched_timer *uapp_sched_timer_declare(u32 time, char *event, int priority){
  struct sched_timer *t;

  //disable_fiq();

  for (t=sched_timers;t<&sched_timers[MAX_TIMERS];t++) {
    if (!t->inuse) break;
  }

  // out of timers?
  if (t == &sched_timers[MAX_TIMERS]) {
    enable_fiq();
    return(0);
  }

  // install new timer
  //t->event = event;
  t->event = FALSE;
  t->time_to_wait = time;
  t->sticky_time_to_wait = time;
  t->priority = priority;

  //_XDPRINTF_("%s,%u: event=%u, time_to_wait=%016llx, sticky_time_to_wait=%016llx, priority=%u\n",
	//	  __func__, __LINE__,
	//		t->event, t->time_to_wait, t->sticky_time_to_wait, t->priority);


  if (!timer_next) {
    // no timers set at all, so this is shortest
    time_timer_set = uapp_sched_read_cpucounter();
    uapp_sched_start_physical_timer((timer_next = t)->time_to_wait);
	//_XDPRINTF_("%s,%u: ENTER, time_to_wait=%016llx\n", __func__, __LINE__,
	//		t->time_to_wait);
  } else if ((time + uapp_sched_read_cpucounter()) < (timer_next->time_to_wait + time_timer_set)) {
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
			priority_queue_insert((void *)&sched_timers[i], sched_timers[i].priority);
			//normal_sp = sysreg_read_sp();

			//_XDPRINTFSMP_("%s[%u]: normal_sp=0x%08x\n", __func__, cpuid, normal_sp);

			//_XDPRINTFSMP_("%s[%u]: timer expired; priority=%u, time_to_wait=%u\n", __func__, cpuid,
			//		sched_timers[i].priority, sched_timers[i].sticky_time_to_wait/ (1024*1024));
			time_to_wait = sched_timers[i].sticky_time_to_wait; //reload
			priority = sched_timers[i].priority;
			uapp_sched_timer_declare(time_to_wait, NULL, priority);
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


void uapp_sched_fiqhandler(void){

	//fiq_sp = sysreg_read_sp();
	//_XDPRINTFSMP_("%s: Timer Fired: sp=0x%08x, cpsr=0x%08x\n", __func__,
	//		fiq_sp, sysreg_read_cpsr());
	uapp_sched_timerhandler();
	//bcm2837_miniuart_puts("\n[HYPTIMER]: Fired!!\n");
	//uapp_sched_start_physical_timer(3 * 20 * 1024 * 1024);
	//_XDPRINTFSMP_("%s: resuming\n", __func__);

}


void uapp_sched_timerhandler(void){
	uapp_sched_stop_physical_timer();
	//_XDPRINTFSMP_("%s,%u: ENTER\n", __func__, __LINE__);

	uapp_sched_timers_update(uapp_sched_read_cpucounter() - time_timer_set);

	// start physical timer for next shortest time if one exists
	if (timer_next) {
		//_XDPRINTFSMP_("%s, %u: starting physical timer with %u\n", __func__, __LINE__,
		//		timer_next->time_to_wait);
		time_timer_set = uapp_sched_read_cpucounter();
		uapp_sched_start_physical_timer(timer_next->time_to_wait);
	}

	//_XDPRINTFSMP_("%s,%u: ENTER\n", __func__, __LINE__);
	//uapp_sched_process_timers_fiq();
	uapp_sched_logic();

}






void uapp_sched_logic(void){
	struct sched_timer *task_timer;
	u32 queue_data;
	int priority;
	int status;
	volatile u32 sp, spnew;

	//TBD: remove hard-coded cpuid (0) below
	uapp_sched_process_timers(0);

	status=0;
	status = priority_queue_remove(&queue_data, &priority);

	if(status){
		task_timer = (struct sched_timer *)queue_data;
    	//bcm2837_miniuart_puts("\n[HYPSCHED]: Task timer expired. Priority=0x");
    	//debug_hexdumpu32(task_timer->priority);
    	//bcm2837_miniuart_puts("\n");
		//_XDPRINTFSMP_("\n[HYPSCHED]: task timer priority=%d expired!\n", task_timer->priority);
	}

}


void uapp_sched_initialize(u32 cpuid){
	int value;
	int priority;
	struct sched_timer *task_timer;
	u32 queue_data;
	int status;
	volatile u32 sp, spnew;


	if(cpuid == 0){
		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		_XDPRINTFSMP_("%s[%u]: Current CPU counter=0x%016llx\n", __func__, cpuid,
				uapp_sched_read_cpucounter());

		//hypvtable_setentry(cpuid, 7, (u32)&uapp_sched_fiq_handler);
		uapp_sched_timer_initialize(cpuid);

		//_XDPRINTFSMP_("%s[%u]: Starting timers...\n", __func__, cpuid);

		//uapp_sched_timer_declare(3 * 1024 * 1024, NULL, 1);
		//disable_fiq();
		//uapp_sched_timer_declare(9 * 1024 * 1024, NULL, 3);
		//enable_fiq();
		//uapp_sched_start_physical_timer(3 * 20 * 1024 * 1024);
		//uapp_sched_timer_declare(10 * 1024 * 1024, NULL, 3);

		uapp_sched_timer_declare(3 * 20 * 1024 * 1024, NULL, 1);
		uapp_sched_timer_declare(9 * 20 * 1024 * 1024, NULL, 3);



		_XDPRINTFSMP_("%s[%u]: Initializing scheduler...\n", __func__, cpuid);

		normal_sp =sysreg_read_sp();
		_XDPRINTFSMP_("%s[%u]: FIQ Stack pointer base=0x%08x\n", __func__, cpuid,
				&uapp_sched_fiqhandler_stack);
		_XDPRINTFSMP_("%s[%u]: normal_sp=0x%08x\n", __func__, cpuid, normal_sp);
		_XDPRINTFSMP_("%s[%u]: cpsr=0x%08x\n", __func__, cpuid, sysreg_read_cpsr());

	}else{
		_XDPRINTFSMP_("%s[%u]: AP CPU: nothing to do, moving on...\n", __func__, cpuid);
	}

}

//return true if handled the hypercall, false if not
bool uapp_hypmtscheduler_handlehcall(u32 uhcall_function, void *uhcall_buffer,
		u32 uhcall_buffer_len){
	ugapp_hypmtscheduler_param_t *hmtsp;
	uint32_t i;
	u32 uhcall_buffer_paddr;

	if(uhcall_function != UAPP_HYPMTSCHEDULER_FUNCTION_TEST)
		return false;

	//_XDPRINTFSMP_("%s: hcall: uhcall_function=0x%08x, uhcall_buffer=0x%08x, uhcall_buffer_len=0x%08x\n", __func__,
	//		uhcall_function, uhcall_buffer, uhcall_buffer_len);

	hmtsp = (ugapp_hypmtscheduler_param_t *)uhcall_buffer;

	hmtsp->out[0] = hmtsp->in[0];

	return true;
}
