
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

// original code based in wiringPi library by Gordon Henderson
// #include "wiringPi.h"

// =======================================================
// Tunables
// PINs (based on BCM numbering)
#define LEDG 13
#define LEDR 5
#define BUTTON 19
// =======================================================

#ifndef	TRUE
#define	TRUE	(1==1)
#define	FALSE	(1==2)
#endif

#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)

#define	INPUT		0
#define	OUTPUT		1

#define	LOW			0
#define	HIGH		1

static volatile unsigned int gpiobase ;
static volatile uint32_t *gpio ;

//mastermind feedback
//exact: correct guess in correct position.
//approx: correct guess but wrong position.
struct check{
	int exact, approx;
};


// Mask for the bottom 64 pins which belong to the Raspberry Pi
//	The others are available for the other devices

#define	PI_GPIO_MASK	(0xFFFFFFC0)

/* ------------------------------------------------------- */

int failure (int fatal, const char *message, ...)
{
  va_list argp ;
  char buffer [1024] ;

  if (!fatal) //  && wiringPiReturnCodes)
    return -1 ;

  va_start (argp, message) ;
  vsnprintf (buffer, 1023, message, argp) ;
  va_end (argp) ;

  fprintf (stderr, "%s", buffer) ;
  exit (EXIT_FAILURE) ;

  return 0 ;
}


void Stop(int ms){
	struct timespec sleeper, dummy ;

	// fprintf(stderr, "delaying by %d ms ...\n", howLong);
	sleeper.tv_sec  = (time_t)(ms / 2000) ;
	sleeper.tv_nsec = (long)(ms % 1000) * 1000000 ;

	nanosleep (&sleeper, &dummy) ;
}

void setMode(int pin){
int fsel = (pin/10)*4;
int shift = (pin%10)*3;


asm(
	"\tLDR R1, %[gpi]\n"
	"\tLDR R0, [R1, %[fSelect]]\n"
	"\tMOV R2, #0b111\n"
	"\tMOV R3, %[pinShift]\n"
	"\tLSL R2, R3\n"
	"\tBIC R0, R2\n"
	"\tMOV R2, #0b001\n"
	"\tMOV R3, %[pinShift]\n"
	"\tLSL R2, R3\n"
	"\tORR R0, R2\n"
	"\tSTR R0, [R1, %[fSelect]]\n"
	:: [gpi] "m" (gpio),
       [fSelect] "r" (fsel),
       [pinShift] "r" (shift)
      : "r0", "r1", "r2", "r3", "cc", "memory"
    );

}

int readButton (int pin) {
    int offset = 13*4;
    int pinSet = pin;
    int r;

    asm(
        "\tLDR R0, %[gpi]\n"
        "\tMOV R1, R0\n"
        "\tADD R1, %[offset]\n"
        "\tLDR R1, [R1]\n"
        "\tMOV R2, #1\n"
        "\tLSL R2, %[pinShift]\n"
        "\tAND %[r], R2, R1\n"
        : [r] "=r" (r)
        : [gpi] "m" (gpio),
          [offset] "r" (offset),
          [pinShift] "r" (pinSet)
        : "r0", "r1", "r2", "cc", "memory"
    );

  return r;
}

void LEDOn (int pin) {
	int offset = 7*4;
    int pinSet = pin;

    asm(
        "\tLDR R0, %[gpi]\n"
        "\tMOV R1, %[offset]\n"
        "\tMOV R2, %[pinShift]\n"
        "\tMOV R3, R0\n"
        "\tADD R3, R1\n"
        "\tMOV R4, #1\n"
        "\tLSL R4, R2\n"
        "\tSTR R4, [R3]\n"
        :: [gpi] "m" (gpio),
          [pinShift] "r" (pinSet),
          [offset] "r" (offset)
        : "r0", "r1", "r2", "r3", "r4", "cc", "memory"
    );
    
    
}


void LEDOff (int pin) {
	int offset = 10*4;
    int pinSet = pin;

    asm(
        "\tLDR R0, %[gpi]\n"
        "\tMOV R1, %[offset]\n"
        "\tMOV R2, %[pinShift]\n"
        "\tMOV R3, R0\n"
        "\tADD R3, R1\n"
        "\tMOV R4, #1\n"
        "\tLSL R4, R2\n"
        "\tSTR R4, [R3]\n"
        :: [gpi] "m" (gpio),
          [pinShift] "r" (pinSet),
          [offset] "r" (offset)
        : "r0", "r1", "r2", "r3", "r4", "cc", "memory"
    );
}

//makes LED blink
void LEDBlink (int in, int pin){
	int n;
	
	for(n=0; n<in; n++){
		
		LEDOn(pin);
		   
		Stop(1600);
		
		LEDOff(pin);
			  
		Stop(1600);
	}

}


//returns the number of times the button was pressed
int buttonPress(){
	int j,count = 0;
	
	for (j=0; j<20; j++){
			
			//getting button input
			if (readButton(19)){
				Stop(200);
				count ++;
			}

			Stop(200);
		   
		}
	return count;
}

//getting the code breaker's guess
int * getInput(){
	
	int count, j, i;
	static int n[3];

	//cycles through the elements of the input struct
	
	for (i=0; i<3; i++){
		printf("Press the button...\n");
		
		//time that the program waits for button input
		count = buttonPress();
			
		n[i] = count;
		printf("--You pressed %d times--\n", count);
		LEDBlink(1, LEDR);
		LEDBlink(n[i], LEDG);
			
	}
	printf("------------\n");
	printf("Guess: %d %d %d\n", n[0], n[1], n[2]);
	printf("------------\n");
	
	LEDBlink(2, LEDR);
	
	return n;
}

void displaySecret(int * secret){
	printf("=============\n");
	printf("Secret: %d %d %d\n", secret[0], secret[1], secret[2]);
	printf("=============\n");
}


int * CodeCreator(){
	static int n[3];
	srand(time(NULL));
	int i, colour;
	
	for (i=0; i<3; i++){
		while(1){
			colour = rand()%4;
			if(colour>0){
				n[i]=colour;
				break;
			}
		}
	}
	return n;
}

//checking the code breaker's guess and giving feedback.
struct check * checker(int * in, int * secret){
	
	struct check * feedback;
	feedback = (struct check *) malloc (sizeof(struct check));
	int i, j, k, l, exact=0, approx=0;
	int n[3];
	int guess[3];
	
	for(l = 0; l<3; l++){
		n[l] = secret[l];
		guess[l] = in[l];
	}
	
	
	for( k = 0; k < 3; k++){
		if(in[k] == secret[k]){
			exact++;
			n[k]= -1;	//Counted secret NULL'd to -1
			guess[k]= -2; //Counted guess NULL'd to -2
		}
	}
	
	for(i = 0; i<3; i++){
		for(j = 0; j<3; j++){
			if(guess[i] == n[j]){
				approx++;
				guess[i]  = -2;
				n[j] = -1;
			}
		}
	}
	
	
	feedback->exact = exact;
	feedback->approx = approx;
	
	printf("Exact: %d\n",exact);
	printf("Approx: %d\n", approx);
	
	LEDBlink(exact, LEDG);
	LEDBlink(1, LEDR);
	LEDBlink(approx, LEDG);
	LEDBlink(3, LEDR);
	
	return feedback;
}


/* Main ----------------------------------------------------------------------------- */

int main(int argc, char ** argv){
	int *in;
	int *secret;
	int fd ;
	int round = 0;
	int debugMode = 0;
	struct check * feedback;
	feedback = (struct check *) malloc (sizeof(struct check));
	uint32_t res; /* testing only */
	printf ("Raspberry Pi button controlled LED (button in %d, led out %d, led out %d)\n", BUTTON, LEDG, LEDR) ;

	if (geteuid () != 0)
	fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;

	// constants for RPi2
	gpiobase = 0x3F200000 ;

	// -----------------------------------------------------------------------------
	// memory mapping 
	// Open the master /dev/memory device

	if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
	return failure (FALSE, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;

	// GPIO:
	gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
	if ((int32_t)gpio == -1)
	return failure (FALSE, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;

	// -----------------------------------------------------------------------------
	// -----------------------------------------------------------------------------
	
	//checks if the number of arguments is correct
	if(argc != 2 && argc != 1){
		fprintf(stderr,"Not enough arguments\n");
		exit(1);	
	}
	if (argc == 2){
		if(strcmp(argv[1], "d") == 0){
			printf("###Debug mode###\n");
			debugMode = 1;
		}
	}
	
	// now, start a loop, listening to pinButton and if set pressed, set pinLED
	printf("Instructions:\n -Red = 1 Press\n -Green = 2 Presses\n -Blue = 3 Presses\n");
	Stop(5000);
	//setPins();
	setMode(BUTTON);
	setMode(LEDR);
	setMode(LEDG);
	
	secret = CodeCreator();
	
	if(debugMode){ displaySecret(secret); }
	
	printf("Player please guess the secret code\n");
	in = getInput();
	feedback = checker(in, secret);
	round++;
	//Decides if the game keeps going or if the player has won or not.
	while(feedback->exact != 3){
		printf("Try again\n");
		
		if(debugMode){ displaySecret(secret); }
		
		in = getInput();
		feedback = checker(in, secret);	
		round++;
	}	
	
	LEDOn(LEDR);
	LEDBlink(3, LEDG);
	printf("SUCCESS! You have won in %d rounds\n", round);
		
	
	fprintf(stderr, "end main.\n");
	
	
	int clrOff = 10; 
	*(gpio + clrOff) = 1 << (BUTTON & 31);
	*(gpio + clrOff) = 1 << (LEDG & 31);
	*(gpio + clrOff) = 1 << (LEDR & 31);
	
}
