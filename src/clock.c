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

// Time variables
unsigned char hour, minute, second;
unsigned char time_string[6];

// choosen display variables
unsigned char choosen_display, choosen_display_flag;

// selection and data buffers for 7seg display
__xdata unsigned char *CSDS = (__xdata unsigned char *) 0xFF30;
__xdata unsigned char *CSDB = (__xdata unsigned char *) 0xFF38;

// Digits in 7-seg representation
__code unsigned char segments[10] = {
										0b00111111, 0b00000110, 0b01011011, 0b01001111, 
										0b01100110, 0b01101101, 0b01111101, 0b00000111, 
										0b01111111, 0b01101111
									};

/*--------------------------------*
 *     Function delcarations      *
 *--------------------------------*/

void timer_init();
void increment_time();

void _7seg_refresh();
void _7seg_init();

void t0_int(void) __interrupt(1);

/*--------------------------------*
 *              MAIN              *
 *--------------------------------*/
void main()
{
    _7seg_init();
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

void _7seg_init() 
{
    unsigned char i;
    hour = 0;
    minute = 0;
    second = 0;
    for(i = 0; i < 6; i++) {
        time_string[i] = 0;
    }
	choosen_display = 0;
	choosen_display_flag = 0b00000001;
	_7seg_refresh();
}

/*
    Interrupt handling from Timer 0
*/
void t0_int(void) __interrupt(1)
{
    _7seg_refresh();

    interrupt_counter++;

    TH0 = TH0_INIT_VALUE;

    if (interrupt_counter >= INTERRUPT_COUNTER_OVERFLOW) {
        counter_overflow_flag = 1;
    }
}

/*
    Increments time by 1 second when called,
    changes values of other time variables if one of them overflown its maximum value,
    updates only fields in time_string that have to be changed
*/
void increment_time() 
{
    second++;
    if(second == 60) {
        second = 0;
        minute++;
        if (minute == 60) {
            minute = 0;
            hour++;
            if (hour == 24) {
                hour = 0;
            }
            time_string[5] = hour / 10;
            time_string[4] = hour % 10;
        }
        time_string[3] = minute / 10;
        time_string[2] = minute % 10;
    }
    time_string[1] = second / 10;
    time_string[0] = second % 10;
}

/*
    Refreshes one of the displays
*/
void _7seg_refresh()
{
	P1_6 = 1;
	*CSDS = choosen_display_flag;
	*CSDB = segments[time_string[choosen_display]];
    P1_6 = 0;
	
	if (++choosen_display == 6) choosen_display = 0;

	choosen_display_flag = choosen_display_flag << 1;
	if (choosen_display_flag == 0b01000000) choosen_display_flag = 0b00000001;
}
