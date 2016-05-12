#include "miosix.h"
#include "interfaces/cstimer.h"
#include "interfaces/portability.h"
#include "kernel/kernel.h"
#include "kernel/logging.h"
#include "kernel/scheduler/timer_interrupt.h"
#include <cstdlib>

using namespace miosix;

//TODO: comment me
static const uint32_t threshold = 0xffffffff/4*3;
static long long ms32time = 0; //most significant 32 bits of counter
static long long ms32chkp = 0; //most significant 32 bits of check point
static bool lateIrq=false;

static inline long long nextInterrupt()
{
    return ms32chkp | TIM2->CCR1;
}

static inline long long IRQgetTick()
{
    //If overflow occurs while interrupts disabled
    //counter should be checked before rollover interrupt flag
    uint32_t counter = TIM2->CNT;
    if((TIM2->SR & TIM_SR_UIF) && counter < threshold)
        return (ms32time | static_cast<long long>(counter)) + 0x100000000ll;
    return ms32time | static_cast<long long>(counter);
}

void __attribute__((naked)) TIM2_IRQHandler()
{
    saveContext();
    asm volatile ("bl _Z9cstirqhndv");
    restoreContext();
}

void __attribute__((used)) cstirqhnd()
{
    if(TIM2->SR & TIM_SR_CC1IF || lateIrq)
    {
        //Checkpoint met
        //The interrupt flag must be cleared unconditionally whether we are in the
        //correct epoch or not otherwise the interrupt will happen even in unrelated
        //epochs and slowing down the whole system.
        TIM2->SR = ~TIM_SR_CC1IF;
        if(ms32time==ms32chkp || lateIrq)
        {
            lateIrq=false;
            
            IRQtimerInterrupt(nextInterrupt());
        }

    }
    //Rollover
    //On the initial update SR = UIF (ONLY)
    if(TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR = ~TIM_SR_UIF; //w0 clear
        ms32time += 0x100000000;
    }
}

//
// class ContextSwitchTimer
//

namespace miosix {

ContextSwitchTimer& ContextSwitchTimer::instance()
{
    static ContextSwitchTimer instance;
    return instance;
}

void ContextSwitchTimer::IRQsetNextInterrupt(long long tick)
{
    ms32chkp = tick & 0xFFFFFFFF00000000;
    TIM2->CCR1 = static_cast<unsigned int>(tick & 0xFFFFFFFF);
    if(IRQgetTick() > nextInterrupt())
    {
        NVIC_SetPendingIRQ(TIM2_IRQn);
        lateIrq=true;
    }
}

long long ContextSwitchTimer::getNextInterrupt() const
{
    return nextInterrupt();
}

long long ContextSwitchTimer::getCurrentTick() const
{
    bool interrupts=areInterruptsEnabled();
    //TODO: optimization opportunity, if we can guarantee that no call to this
    //function occurs before kernel is started, then we can use
    //fastInterruptDisable())
    if(interrupts) disableInterrupts();
    long long result=IRQgetTick();
    if(interrupts) enableInterrupts();
    return result;
}

long long ContextSwitchTimer::IRQgetCurrentTick() const
{
    return IRQgetTick();
}

ContextSwitchTimer::~ContextSwitchTimer() {}

ContextSwitchTimer::ContextSwitchTimer()
{
    // TIM2 Source Clock (from APB1) Enable
    {
        InterruptDisableLock idl;
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
        RCC_SYNC();
        DBGMCU->APB1FZ|=DBGMCU_APB1_FZ_DBG_TIM2_STOP; //Tim2 stops while debugging
    }
    // Setup TIM2 base configuration
    // Mode: Up-counter
    // Interrupts: counter overflow, Compare/Capture on channel 1
    TIM2->CR1=TIM_CR1_URS;
    TIM2->DIER=TIM_DIER_UIE | TIM_DIER_CC1IE;
    NVIC_SetPriority(TIM2_IRQn,3); //High priority for TIM2 (Max=0, min=15)
    NVIC_EnableIRQ(TIM2_IRQn);
    // Configure channel 1 as:
    // Output channel (CC1S=0)
    // No preload(OC1PE=0), hence TIM2_CCR1 can be written at anytime
    // No effect on the output signal on match (OC1M = 0)
    TIM2->CCMR1 = 0;
    TIM2->CCR1 = 0;
    // TIM2 Operation Frequency Configuration: Max Freq. and longest period
    TIM2->PSC = 0;
    TIM2->ARR = 0xFFFFFFFF;
    
    // Enable TIM2 Counter
    ms32time = 0;
    TIM2->EGR = TIM_EGR_UG; //To enforce the timer to apply PSC (and other non-immediate settings)
    TIM2->CR1 |= TIM_CR1_CEN;
    
    // The global variable SystemCoreClock from ARM's CMSIS allows to know
    // the CPU frequency.
    timerFreq=SystemCoreClock;

    // The timer frequency may however be a submultiple of the CPU frequency,
    // due to the bus at whch the periheral is connected being slower. The
    // RCC->CFGR register tells us how slower the APB1 bus is running.
    // This formula takes into account that if the APB1 clock is divided by a
    // factor of two or greater, the timer is clocked at twice the bus
    // interface. After this, the freq variable contains the frequency in Hz
    // at which the timer prescaler is clocked.
    if(RCC->CFGR & RCC_CFGR_PPRE1_2) timerFreq/=1<<((RCC->CFGR>>10) & 0x3);
}

} //namespace miosix