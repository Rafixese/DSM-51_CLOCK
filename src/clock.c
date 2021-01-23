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

/*
    Serial transmission
*/
__bit recv_flag;
__bit send_flag;
unsigned char recv_buf[12];
unsigned char recv_index;
unsigned char send_buf[8];
unsigned char send_index;
unsigned char expected_number_of_symbols;

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

void serial_init();
void handle_command();

void update_time_string();

void t0_int(void) __interrupt(1);
void serial_int(void) __interrupt(4)  __using(4);

/*--------------------------------*
 *              MAIN              *
 *--------------------------------*/
void main()
{
    edit_init();
    keyboard_action_init();
    _7seg_init();
    timer_init();
    serial_init();

    while(1) {
        // Counter overflow handling
        if (counter_overflow_flag) {
            counter_overflow_flag = 0;
            interrupt_counter -= INTERRUPT_COUNTER_OVERFLOW;
            increment_time();
        }

        // Serial transmision
        if(recv_flag) {
            recv_flag = 0;
            if(recv_index == 1) {
                if(recv_buf[recv_index - 1] == 'S') {
                    expected_number_of_symbols = 12;
                }
                else if(recv_buf[recv_index - 1] == 'G') {
                    expected_number_of_symbols = 3;
                }
                else if(recv_buf[recv_index - 1] == 'E') {
                    expected_number_of_symbols = 4;
                }
                else {
                    // Error
                    recv_index = 0;
                }
            }
            else if (recv_index == expected_number_of_symbols) {
                handle_command();
            }
        }
        if(send_flag) {
            send_flag = 0;
            // if(send_index > 0) {
            //     SBUF = send_buf[send_index--];
            // }
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

void serial_init()
{
    /*
        SCON configuration
        target mode -> 8bit, M0=0, M1=1
    */
    SCON = 0b01010000;

    /*
        Timer 1 configuration -> mode 2
    */
    TMOD = TMOD & 0b00101111; // GATE1=0, CT1=0, T1M1=?, T1M0=0, GATE0=?, CT0=?, T0M1=?, T0M0=?
    TMOD = TMOD | 0b00100000; // GATE1=?, CT1=?, T1M1=1, T1M0=?, GATE0=?, CT0=?, T0M1=?, T0M0=?

    /*
        T1 = 250 => baudrate = 4800
    */
    TL1 = 250;
    TH1 = 250;

    /*
        SCON = 0
    */
    PCON = PCON & 0b01111111;

    // TF1
    TF1 = 0;

    // TR1, start counting
    TR1 = 1;

    // Interrupts
    ES = 1;
    EA = 1;

    // Variable init
    recv_flag = 0;
    send_flag = 0;
    recv_index = 0;
    send_index = 0;
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
    Interrupt handling from serial port
*/
void serial_int(void) __interrupt(4)  __using(4)  
{
    if (RI) {
        recv_buf[recv_index++] = SBUF;
        RI = 0;
        recv_flag = 1;
    }
    else {
        TI = 0;
        send_flag = 1;
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
            update_time_string();
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

/*
    Handles command input from serial port
*/
void handle_command()
{
    __bit parse_error;
    parse_error = 0;
    if (recv_buf[0] == 'S' && recv_buf[1] == 'E' && recv_buf[2] == 'T' && recv_buf[3] == ' ' && recv_buf[6] == '.' && recv_buf[9] == '.') {
        unsigned char set_hour, set_minute, set_second;
        P1_7 = !P1_7;
        set_hour = (recv_buf[4] - 48) * 10 + recv_buf[5] - 48;
        set_minute = (recv_buf[7] - 48) * 10 + recv_buf[8] - 48;
        set_second = (recv_buf[10] - 48) * 10 + recv_buf[11] - 48;

        if(set_hour > 23 || set_minute > 59 || set_second > 59) {
            parse_error = 1;
        }
        else {
            hour = set_hour;
            minute = set_minute;
            second = set_second;
            update_time_string();
        }
    }

    recv_index = 0;
}

/*
    Updates whole time string array based on hour, minute and second variables
*/
void update_time_string() {
    time_string[1] = second / 10;
    time_string[0] = second % 10;
    time_string[3] = minute / 10;
    time_string[2] = minute % 10;
    time_string[5] = hour / 10;
    time_string[4] = hour % 10;
}
