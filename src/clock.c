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
unsigned char prev_hour, prev_minute, prev_second;
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

// Multiplex keyboard bit
__sbit __at (0xB5) MUXK;

// Prev multiplex keyboard state
unsigned char prev_mux_kbrd_state;

/*
    Edit mode
    2 bits controlling edit mode for clock

    high_b  low_b   meaning
    0       0       edit mode off
    0       1       edit seconds
    1       0       edit minutes
    1       1       edit hours
*/
__bit edit_mode_high;
__bit edit_mode_low;

/*--------------------------------*
 *     Function delcarations      *
 *--------------------------------*/

void timer_init();
void increment_time();

void _7seg_refresh();
void _7seg_init();

void edit_init();

void keyboard_action_init();
void handle_user_input();

void t0_int(void) __interrupt(1);

/*--------------------------------*
 *              MAIN              *
 *--------------------------------*/
void main()
{
    edit_init();
    keyboard_action_init();
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

void edit_init()
{
    edit_mode_high = 0;
    edit_mode_low = 0;
}

void keyboard_action_init()
{
    prev_mux_kbrd_state = 0b00000000;
}

/*
    Interrupt handling from Timer 0
*/
void t0_int(void) __interrupt(1)
{
    _7seg_refresh();
    handle_user_input();

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
    if(edit_mode_high == 0 && edit_mode_low == 0) {
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
}

/*
    Refreshes one of the displays
*/
void _7seg_refresh()
{
    if (++choosen_display == 6) choosen_display = 0;

	choosen_display_flag = choosen_display_flag << 1;
	if (choosen_display_flag == 0b01000000) choosen_display_flag = 0b00000001;

	P1_6 = 1;
	*CSDS = choosen_display_flag;
	*CSDB = segments[time_string[choosen_display]];
    if((edit_mode_high == 0 && edit_mode_low == 0) || 
        interrupt_counter < INTERRUPT_COUNTER_OVERFLOW/2 ||
        (edit_mode_high == 0 && edit_mode_low == 1 && choosen_display > 1) ||
        (edit_mode_high == 1 && edit_mode_low == 0 && (choosen_display < 2 || choosen_display > 3)) ||
        (edit_mode_high == 1 && edit_mode_low == 1 && choosen_display < 4)) {
        P1_6 = 0;
    }
}

/*
    Handles mux keyboard input
*/
void handle_user_input() 
{
    unsigned char mux_kbd_state_diff;

    if(MUXK) {
        mux_kbd_state_diff = (~prev_mux_kbrd_state) & choosen_display_flag;
        prev_mux_kbrd_state = prev_mux_kbrd_state | choosen_display_flag;
	}
	else {
        mux_kbd_state_diff = 0b00000000;
        prev_mux_kbrd_state = (~choosen_display_flag) & prev_mux_kbrd_state;
	}

    // Left arrow
	if (mux_kbd_state_diff & 0b00100000) {
		if(edit_mode_high == 0 && edit_mode_low == 1) {
            edit_mode_high = 1;
            edit_mode_low = 0;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 0) {
            edit_mode_low = 1;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 1) {
            edit_mode_high = 0;
        }
	}

    // Downarrow
	if (mux_kbd_state_diff & 0b00010000) {
		if(edit_mode_high == 0 && edit_mode_low == 1) {
            if(second-- == 0) {
                second = 59;
            }
            time_string[1] = second / 10;
            time_string[0] = second % 10;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 0) {
            if (minute-- == 0) {
                minute = 59;
            }
            time_string[3] = minute / 10;
            time_string[2] = minute % 10;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 1) {
            if (hour-- == 0) {
                hour = 23;
            }
            time_string[5] = hour / 10;
            time_string[4] = hour % 10;
	    }
    }

    // Up arrow
	if (mux_kbd_state_diff & 0b00001000) {
		if(edit_mode_high == 0 && edit_mode_low == 1) {
            second++;
            if(second == 60) {
                second = 0;
            }
            time_string[1] = second / 10;
            time_string[0] = second % 10;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 0) {
            minute++;
            if (minute == 60) {
                minute = 0;
            }
            time_string[3] = minute / 10;
            time_string[2] = minute % 10;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 1) {
            hour++;
            if (hour == 24) {
                hour = 0;
            }
            time_string[5] = hour / 10;
            time_string[4] = hour % 10;
        }
	}

    // Right arrow
	if (mux_kbd_state_diff & 0b00000100) {
		if(edit_mode_high == 0 && edit_mode_low == 1) {
            edit_mode_high = 1;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 0) {
            edit_mode_high = 0;
            edit_mode_low = 1;
        }
        else if(edit_mode_high == 1 && edit_mode_low == 1) {
            edit_mode_low = 0;
        }
	}

    // ESC
	if (mux_kbd_state_diff & 0b00000010) {
        if(!(edit_mode_high == 0 && edit_mode_low == 0)) {
            edit_mode_high = 0;
            edit_mode_low = 0;
            hour = prev_hour;
            minute = prev_minute;
            second = prev_second;
            time_string[1] = second / 10;
            time_string[0] = second % 10;
            time_string[3] = minute / 10;
            time_string[2] = minute % 10;
            time_string[5] = hour / 10;
            time_string[4] = hour % 10;
        }
	}

    // ENTER
	if (mux_kbd_state_diff & 0b00000001) {
        // Enable edit
        if(edit_mode_high == 0 && edit_mode_low == 0) {
            edit_mode_low = 1;
            prev_hour = hour;
            prev_minute = minute;
            prev_second = second;
        }
        else {
            edit_mode_high = 0; 
            edit_mode_low = 0;
        }
	}
}
