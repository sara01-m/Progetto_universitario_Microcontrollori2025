
/*Lab 5 maria sara m
 * registri per la periferiche che ci servono
 * #define XPAR_AXI_16LEDS_GPIO_BASEADDR 0x40000000 led
 * #define XPAR_AXI_TIMER_0_BASEADDR 0x41C00000U timer
 * #define XPAR_INTC_0_BASEADDR 0x41200000U attiva il sistema di interrupt
 * obiettivi del lab:
 *Usare i registri AXI GPIO per accendere i singoli colori (R, G, B) dei LED sinistro e destro.\
 *Simulare il controllo di luminosit� (duty cycle) accendendo/spegnendo i LED ad alta frequenza.
 *Simulare dal main impostarne i valori
 **/
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include "sleep.h"

// Definizioni Macro Timer
#define TmrCtrNumber    0 //timer 0
#define TIMER_INT_SRC   0b0100 // Timer interrupt source in INTC
#define TIMER_RESET     0x56 // Comando per resettare il timer
#define TIMER_MASK     0x100 // Timer interrupt mask
#define TIMER_VALUE  250000 //timer con un valore di carico fisso

//Indirizzi delle periferiche
#define LED XPAR_AXI_16LEDS_GPIO_BASEADDR //registro per i led
#define INTERRUPT XPAR_AXI_INTC_0_BASEADDR //registro del controller interrupt
#define TIMER     XPAR_AXI_TIMER_0_BASEADDR //registro del timer

/*definizione registri per il controllo degli interrupt
*registri ISR, IER, IAR, e MER sono utilizzati per il monitoraggio e l'abilitazione/disabilitazione degli interrupt
*/
#define ISR 0x00    //Interrupt Status register
#define IAR 0x0C // Interrupt Acknowledge Register
#define IER 0x08 // Interrupt Enable Register
#define MER 0x1C // Master Enable Register

// LED
#define LED_RIGHT 0b000111
#define LED_LEFT 0b111000


// Indirizzo Registro LED
volatile int* AXI_RGBLEDS = (int*)XPAR_AXI_RGBLEDS_GPIO_BASEADDR;

// Variabili Globali
const int maxDutyCycle = ( 512 / 2 ) / 32;	// Intensita' Colore (Treshold consigliato 256)
int dutyCycleCounter = 0;  // Contatore PWM (si resetta a 512)

int leftRLevel, leftGLevel, leftBLevel; // Colori LED sinistro (0�255)
int rightRLevel, rightGLevel, rightBLevel; // Colori LED destro (0�255)

void timerISR(void) __attribute__((interrupt_handler)); // ISR per il timer
void setup_interrupt(); // Configura interrupt
void set_timer(int value); //configura timer

int main()
{
init_platform();
    // Inizializzazione intterupt e timer
    setup_interrupt();
    set_timer(2000); // Imposta frequenza PWM (~20kHz)

    // Impostazione colori
    leftRLevel = 255; // Rosso  al massimo
    leftGLevel = 128; // Verde medio
    leftBLevel = 0;  // Blu spento
    rightRLevel = 128; // Rosso medio
    rightGLevel = 255; //Verde al massimo
    rightBLevel = 255; //blu al massimo

    while (1) {
        // Background
    }

    cleanup_platform();
    return 0;
}
//ISR: aggiorna PWM e controlla RGB
void timerISR(void){
	// Se la sorgente dell'interrupt � il timer
	int interruptSource = *(int*)INTERRUPT;
		    if (interruptSource & TIMER_INT_SRC) {
		    	// Incrementa il contatore PWM
		        dutyCycleCounter = ( dutyCycleCounter < 512) ? (dutyCycleCounter + 1) : 0;
		        // Se all'interno del duty cycle, attiva LED
		        if ( dutyCycleCounter < maxDutyCycle ) {
		            // OR per alzare i bit, XOR ( A and not B ) per abbassarli
		            *AXI_RGBLEDS =
		            		 // LEFT R/G/B
		                ( ( dutyCycleCounter < leftRLevel ) ? ( *AXI_RGBLEDS | 0b1000 ) : ( *AXI_RGBLEDS & ~0b1000) ) |
		                ( ( dutyCycleCounter < leftGLevel ) ? ( *AXI_RGBLEDS | 0b10000 ) : ( *AXI_RGBLEDS & ~0b10000) ) |
		                ( ( dutyCycleCounter < leftBLevel ) ? ( *AXI_RGBLEDS | 0b100000 ) : ( *AXI_RGBLEDS & ~0b100000) ) |
						 // RIGHT R/G/B
						( ( dutyCycleCounter < rightRLevel ) ? ( *AXI_RGBLEDS | 0b1 ) : ( *AXI_RGBLEDS & ~0b1) ) |
		                ( ( dutyCycleCounter < rightGLevel ) ? ( *AXI_RGBLEDS | 0b10 ) : ( *AXI_RGBLEDS & ~0b10) ) |
		                ( ( dutyCycleCounter < rightBLevel ) ? ( *AXI_RGBLEDS | 0b100 ) : ( *AXI_RGBLEDS & ~0b100) );
		        }
		        else {
		            *AXI_RGBLEDS = 0; // Spegne i LED se sono fuori dal duty cycle
		        }

		        // Riconosce l'interrupt del timer (reset del flag di interrupt interno)
		        *(int*)TIMER |= TIMER_MASK;

		        // Riconosce l'interrupt globale nel controller
		        *(int*)(INTERRUPT + IAR) = TIMER_INT_SRC;	// Acknowledge Global Interrupt
		    }
}
//Abilita gli interrupt nel controller
void setup_interrupt(){
	// Abilita interrupt
	*(int *)(INTERRUPT + MER) = 0b11;  // Abilita MER
	*(int *)(INTERRUPT + IER) = 0b110; // Abilita IER

	// Abilita interrupt nella CPU
	microblaze_enable_interrupts();
}
//Configura e avvia il timer
void set_timer(int value){
	   // Configurazione Timer
	    XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, TIMER_RESET); 	// Configura Status Register (SR)
	    XTmrCtr_SetLoadReg(TIMER, TmrCtrNumber, value);  // Load register impostato su value
	    XTmrCtr_LoadTimerCounterReg(TIMER, TmrCtrNumber);   // Inizializza il timer

	    // Fa partire il timer resettando il bit di LOAD0
	    u32 controlStatus = XTmrCtr_GetControlStatusReg(TIMER, TmrCtrNumber);
	    XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);

	    // Attiva il timer
	    XTmrCtr_Enable(TIMER, TmrCtrNumber);
}




















