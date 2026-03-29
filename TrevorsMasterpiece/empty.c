#include "ti_msp_dl_config.h"

// ------------------------------------------------------------------
// GLOBAL VARIABLES
// ------------------------------------------------------------------
volatile uint16_t voltage1 = 0;
volatile uint16_t current1 = 0;
volatile uint16_t voltage2 = 0;
volatile uint16_t current2 = 0;
volatile uint16_t inductorVoltage = 0;

// Function prototypes
void set_PWM_Edges(uint32_t ch1_up_tick, uint32_t ch0_down_tick);
void test_PWM_Output(void);

// ------------------------------------------------------------------
// MAIN ROUTINE
// ------------------------------------------------------------------
int main(void)
{
    // 1. Initialize hardware via SysConfig
    // (Ensure your Action Configurations are set in the GUI first!)
    SYSCFG_DL_init();

    // 2. Enable the interrupt in the NVIC for ADC0
    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);

    // 3. Wake up the ADCs
    DL_ADC12_enableConversions(ADC12_0_INST);
    DL_ADC12_enableConversions(ADC12_1_INST);

    // 4. Run our static test to verify the asymmetric waveform on the scope
    test_PWM_Output();

    // 5. Start the Advanced Timer (Begins generating PWM and triggering ADCs)
    DL_TimerA_startCounter(PWM_0_INST);

    while (1) {
        // Put the CPU into a low-power sleep state until the next interrupt
        __WFI(); 
    }
}

// ------------------------------------------------------------------
// INTERRUPT SERVICE ROUTINE (Runs at 120 kHz)
// ------------------------------------------------------------------
void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_MEM1_RESULT_LOADED:
            
            // Read all ADC channels
            voltage1 = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
            current1 = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_1);
            voltage2 = DL_ADC12_getMemResult(ADC12_1_INST, DL_ADC12_MEM_IDX_0);
            current2 = DL_ADC12_getMemResult(ADC12_1_INST, DL_ADC12_MEM_IDX_1);
            inductorVoltage = DL_ADC12_getMemResult(ADC12_1_INST, DL_ADC12_MEM_IDX_2);

            // Once you are ready, you will calculate your new edge ticks 
            // based on these ADC readings and call set_PWM_Edges() here.
            
            break;
            
        default:
            break;
    }
}

// ------------------------------------------------------------------
// HELPER & TEST FUNCTIONS
// ------------------------------------------------------------------

// Sets the exact timer ticks (0 to 133) where the pins drop LOW
void set_PWM_Edges(uint32_t ch1_up_tick, uint32_t ch0_down_tick)
{
    // Max timer count is 133 for 120kHz at 32MHz (32MHz / 120kHz / 2 - 1 for center, but 133 here)
    if (ch1_up_tick > 133) ch1_up_tick = 133;
    if (ch0_down_tick > 133) ch0_down_tick = 133;

    // Calculate the midpoint between the two falling edges for ADC trigger
    uint32_t midpoint_tick = (ch1_up_tick + ch0_down_tick) / 2;

    // PWM edges (Edge-Aligned: both start HIGH at 0, fall at CC match)
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, ch1_up_tick, DL_TIMER_CC_1_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, ch0_down_tick, DL_TIMER_CC_0_INDEX);

    // Set the ADC trigger point halfway between the edges
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, midpoint_tick, DL_TIMER_CC_2_INDEX);
}

// A simple starter control function to verify the oscilloscope
void test_PWM_Output(void)
{
    // PB9 will go HIGH at 0, and drop LOW early at tick 33 on the way up.
    // PA8 will go HIGH at 0, stay HIGH all the way over the top of the triangle, 
    // and drop LOW late at tick 33 on the way down.
    
    set_PWM_Edges(60, 80);

}