/*
 * guitar_tuner_finished.c
 *
 * Created: 7/21/2016 1:02:01 PM
 *  Author: chandima
 */ 


#define F_CPU (8000000UL)
	//set definitions
#define infinit_T 2000
#define bias 110

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <math.h> // for sine
#include <stdio.h> // for sprintf
#include <string.h>
#include <stdlib.h>




// Sampling variables
unsigned char sOld,S; // hold past two samples
unsigned int measured_T; // measured period
unsigned int target_T; // target period
unsigned char edgesSeen=0; // number of periods seen


//variables to hold string frequency bounds
unsigned int T_low, T_high; //largest bounds
unsigned int T_low2, T_high2; //medium bounds
unsigned int low_bound, high_bound; //small bounds

//arrays to hold string specific values
unsigned int T_array[6]; // hold target periods for strings
unsigned int low_array[6];
unsigned int high_array[6];
unsigned int low_array2[6];
unsigned int high_array2[6];
unsigned int tight_array[6];

//state variables
unsigned char string; //string to be tuned

//variable for arithmetic
unsigned int i;
float c1,c2;

//variables for filter
unsigned int filter_array[6]; // hold filter settings
unsigned int filter1; // alpha
unsigned int filter2; // 1alpha

unsigned int time1,time2;

void initTimer0(void);
void init(void);
//void checkString(void);
void calcTime(void);
void displayLED(void);
unsigned int readADC(void);
/*
// void initStringBounds(void);
// void initTightStringBound(void);
// void initFilters(void);
// void initStringValues(void);
*/
void set_string(void);

int main(void){
	initTimer0();
	init();
	sei();
	interrupt_Enable();
	while (1){
		 //Decrement timer variables
    if (time1>0)   --time1;  // goes to the calcTime() 
    if (time2>0)    --time2; // goes to the checkString() 

	}
	return 0;
}
//Timer0 initialisation 
void initTimer0(void){
	TCCR0 |= 1<<FOC0  |  1<<WGM01 | // CTC mode enabled
	1<<CS01 ; // Prescaler is 8.
	OCR0=199; // (8/8)*200  = 200 microseconds
}

void init(void){
	DDRB= 0x00; /* PortB made input */
	DDRC=0xff; /* PORTC made output */
	DDRA=0x00; /* PORTA made input */
	/* ADC0 chosen as the input pin */
	/* ADC is enabled, ADC prescaler selected as 64*/
	ADCSRA |=(1<<ADEN)  | (1<<ADPS1) | (1<<ADPS2); 
	
 	time1 = time2 = 0;   //init the task timer	
	
		T_array[0] = 607-8-2;
		T_array[1] = 455-7-2;
		T_array[2] = 341-7-1;
		T_array[3] = 255-3-2;
		T_array[4] = 202-4+1;
		T_array[5] = 152-3;
	
		//initialize tight string bounds
		tight_array[0] = 2;
		tight_array[1] = 2;
		tight_array[2] = 1;
		tight_array[3] = 1;
		tight_array[4] = 0;
		tight_array[5] = 0;

		//initialize string bounds
		c1 = 1.05; //1.0293?
		c2 = 1/c1; //0.9715?
		for( i=0;i<6;i++){
			high_array[i] = (unsigned int)(c1*((float)T_array[i]));
			low_array[i] = (unsigned int)(c2*((float)T_array[i]));
			high_array2[i] = T_array[i] + (tight_array[i]+3);
			low_array2[i] = T_array[i]-(tight_array[i]+3);
		
		}
	
		//filter values of the 6 guitar strings
		filter_array[0] = 15;
		filter_array[1] = 15;
		filter_array[2] = 12;
		filter_array[3] = 12;
		filter_array[4] = 4;
		filter_array[5] = 4;
		////set initial string to E
		string = 0;
}

// method to check which string is selected
void checkString(void){
	 time2 = 150;  // return to checkString in 30 ms

	if (PORTB==0b00000001){
		string=0;//E sixth string selected
		set_string();
	} 
	else if(PORTB==0b00000010){
		string=1; //A fifth string selected
		set_string();
	} 
	else if(PORTB==0b00000100){
		string=2; //D fourth string selected
		set_string();
	}
	else if(PORTB==0b00001000){
		string=3; //G third string selected
		set_string();
	}
	else if(PORTB==0b00010000){
		string=4; //B second string selected
		set_string();
	}
	else if(PORTB==0b00100000){
		string=5; //e first string selected
		set_string();
	}
		set_string();
}

//ADC output 
unsigned int readADC(void){
	ADCSRA |= (1<<ADSC); //ADC conversion started
	while (ADCSRA & (1<<ADSC)); //ADC is running and the conversion is progressing
	return ADC;
}

//method to indicate the level of tuning through the LED s
void displayLED(void){
	if(measured_T > T_high2){
		PORTC = 0b00001000;
	}
	if(measured_T > high_bound && measured_T <= T_high2){
		PORTC = 0b00010000;
	}
	if(measured_T >= low_bound && measured_T <= high_bound){
		PORTC = 0b00100000;
	}
	if(measured_T > T_low2 && measured_T < low_bound){
		PORTC = 0b01000000;
	}
	if(measured_T <= T_low2){
		PORTC = 0b10000000;
	}
}

void calcTime(void){
	 time1 = 1; //return in 200 us
//Past 2 samples are saved
	sOld = S; //store old value
	S = readADC(); //sample
	//filter input signal by applying IIR filter
	S= (char)((filter1*(int)sOld + filter2*(int)S)>>4); 
	//Increment time if it is not maxed out
	if (measured_T < infinit_T ){
		measured_T++;
	}
	if(sOld < bias && S >= bias){ //if trigger
		edgesSeen++; //increment number of edges seen
			//make sure T is not infinite
		if(measured_T < infinit_T){
			if(edgesSeen == 10){ //after seeing 10 complete periods
					edgesSeen = 0; //reset number of edges seen
					if(measured_T < T_high && measured_T > T_low){
						//set LEDs based on range of measured_T
						displayLED();
						
					}
				measured_T = 0;
			}
		}
	}
	else {
		// if measured_T is infinite
		measured_T = 0;
	}
	
}

void interrupt_Enable(){
	TIMSK |= 1<<OCIE0; // enable interrupts on compare
}

//Interrupt method to control the time calculation
ISR(TIMER0_COMP_vect){
	if (time1>0)   --time1;  // goes to the calcTime()
    if (time2>0)    --time2; // goes to the checkString()
}
//set string bounds from arrays
void set_string(void){
	target_T = T_array[string];
	//assign the a values specific to the played string
	filter1 = filter_array[string];
	filter2 = 16-filter1;
	T_low = low_array[string];
	T_high = high_array[string];
	T_low2 = low_array2[string];
	T_high2 = high_array2[string];
	high_bound = target_T + tight_array[string];
	low_bound = target_T - tight_array[string];
}
