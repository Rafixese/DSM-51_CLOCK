/*--------------------------------*
 *          DSM-51 Clock          *
 *     Embedded systems course    *
 *          Rafał Hrabia          *
 *--------------------------------*/

#include <8051.h>

/*--------------------------------*
 *     Compile-time constant      *
 *--------------------------------*/

 // Value to reset TH0 to achieve 1200 interrupts / sec
#define TH0_INIT_VALUE 253

// Interrupts number that equals 1 second
#define INTERRUPT_COUNTER_OVERFLOW 1200

/*--------------------------------*
 *         Global variables       *
 *--------------------------------*/

// Flag to sygnalize that some number of interrupts occurred, defined in INTERRUPT_COUNTER_OVERFLOW
__bit counter_overflow_flag;

// Stores number of interrupts that occurred since last overflow
unsigned short interrupt_counter;

/*--------------------------------*
 *     Function delcarations      *
 *--------------------------------*/

void timer_init();
void increment_time();

void t0_int(void) __interrupt(1);

/*--------------------------------*
 *              MAIN              *
 *--------------------------------*/
void main()
{
    timer_init();

    while(1) {
        // Counter overflow handling
        if (counter_overflow_flag) {
            counter_overflow_flag = 0;
            interrupt_counter -= INTERRUPT_COUNTER_OVERFLOW;
            increment_time();
        }
    }
}

/*--------------------------------*
 *     Function definitions       *
 *--------------------------------*/

/*
    Configuration of timer related registers and variables
*/
 void timer_init()
{
    // Variable initialization
    interrupt_counter = 0;
    counter_overflow_flag = 0;

    /*
        Interrupts handling config
    */

    IE = 0b00000000; // Disable interrupts
    ET0 = 1; // Allow interrupts from Timer 0
    EA = 1; // Globally allow interrupts

    /*
        Timer configuration
    */
    TMOD = TMOD & 0b11110001; // Mode 1, GATE=0, C/T=0
    TMOD = TMOD | 0b00000001; // Mode 1

    TL0 = 0;
    TH0 = TH0_INIT_VALUE;

    TF0 = 0; // Overflow flag clear
    TR0 = 1; // Start counting
}

/*
    Interrupt handling from Timer 0
*/
void t0_int(void) __interrupt(1)
{
    interrupt_counter++;

    TH0 = TH0_INIT_VALUE;

    if (interrupt_counter >= INTERRUPT_COUNTER_OVERFLOW) {
        counter_overflow_flag = 1;
    }
}

void increment_time() {
    P1_7 = !P1_7;
}
