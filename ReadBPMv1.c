// ReadBPMv1.c

#include <C8051f38x.h>
#include <LCD_4bit_F381.h>
#include <stdio.h>

#define SYSCLK      48000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps

unsigned char overflow_count;

void PORT_Init (void)
{
	P0MDOUT |= 0x10; // Enable UART TX as push-pull output
	XBR0=0b_0000_0001; // Enable UART on P0.4(TX) and P0.5(RX)                    
	XBR1=0b_0101_0000; // Enable crossbar.  Enable T0 input.
	XBR2=0b_0000_0000;
}

void SYSCLK_Init (void)
{
	// CLKSEL&=0b_1111_1000; // Not needed because CLKSEL==0 after reset
#if (SYSCLK == 12000000L)
	//CLKSEL|=0b_0000_0000;  // SYSCLK derived from the Internal High-Frequency Oscillator / 4 
#elif (SYSCLK == 24000000L)
	CLKSEL|=0b_0000_0010; // SYSCLK derived from the Internal High-Frequency Oscillator / 2.
#elif (SYSCLK == 48000000L)
	CLKSEL|=0b_0000_0011; // SYSCLK derived from the Internal High-Frequency Oscillator / 1.
#else
	#error SYSCLK must be either 12000000L, 24000000L, or 48000000L
#endif
	OSCICN |= 0x03;   // Configure internal oscillator for its maximum frequency
	RSTSRC  = 0x04;   // Enable missing clock detector
}
 
void UART0_Init (void)
{
	SCON0 = 0x10;
   
#if (SYSCLK/BAUDRATE/2L/256L < 1)
	TH1 = 0x10000-((SYSCLK/BAUDRATE)/2L);
	CKCON &= ~0x0B;                  // T1M = 1; SCA1:0 = xx
	CKCON |=  0x08;
#elif (SYSCLK/BAUDRATE/2L/256L < 4)
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/4L);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 01                  
	CKCON |=  0x01;
#elif (SYSCLK/BAUDRATE/2L/256L < 12)
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/12L);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 00
#else
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2/48);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 10
	CKCON |=  0x02;
#endif
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit autoreload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
}

void TIMER0_Init(void)
{
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit timer
	TR0=0; // Stop Timer/Counter 0
}

void main (void) 
{
	float period;
	float bpm;
	char periodString[8];
	char BPMString[8];
	WriteCommand(0x40);
	WriteData(0);
	WriteData(0);
	WriteData(10);
	WriteData(31);
	WriteData(31);
	WriteData(14);
	WriteData(4);
	WriteData(0);

	PCA0MD &= ~0x40; // WDTE = 0 (clear watchdog timer enable)
	PORT_Init();     // Initialize Port I/O
	SYSCLK_Init ();  // Initialize Oscillator
	UART0_Init();    // Initialize UART0
	TIMER0_Init();
	LCD_4BIT();		// initialize the LCD

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	printf ("Period measurement at pin P0.1 using Timer 0.\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);

    while (1)
    {
    	// Reset the counter
		TL0=0; 
		TH0=0;
		TF0=0;
		overflow_count=0;
		
		while(P0_1!=0); // Wait for the signal to be zero
		WriteCommand(0x88);
		WriteData(' ');
		while(P0_1!=1); // Wait for the signal to be one
		WriteCommand(0x88);
		WriteData(0x0);
		TR0=1; // Start the timer
		while(P0_1!=0) // Wait for the signal to be zero
		{
			if(TF0==1) // Did the 16-bit timer overflow?
			{
				TF0=0;
				overflow_count++;
			}
		}
		WriteCommand(0x88);
		WriteData(' ');
		while(P0_1!=1) // Wait for the signal to be one
		{
			if(TF0==1) // Did the 16-bit timer overflow?
			{
				TF0=0;
				overflow_count++;
			}
		}
		WriteCommand(0x88);
		WriteData(0x0);
		

		TR0=0; // Stop timer 0, the 24-bit number [overflow_count-TH0-TL0] has the period!
		period=(overflow_count*65536.0+TH0*256.0+TL0)*(12.0/SYSCLK);
		// Send the period to the serial port
		//printf( "\rperiod=%fs\n" , period);
		bpm=60/period;
		//printf("bpm=%fbeats per min\n", bpm);
		// ftoa(float n, char *res, int afterpoint) located in LCD_4bit_F381.h file
		// convert floats to strings
		printf( "\rperiod=%s\n" , periodString);
		printf("bpm=%sbeats per min\n", BPMString);
		
		ftoa(period, periodString, 3);
		ftoa(bpm, BPMString, 3);
		// print to LCD
		LCDprint("BPM:", 1, 0);
		LCDprint(BPMString, 2, 1);
		
	}
}


 


 
