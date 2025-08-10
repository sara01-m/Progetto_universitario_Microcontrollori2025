
/*Lab 3 maria sara m
 * registri per la periferiche che ci servono
 * #define XPAR_AXI_TIMER_0_BASEADDR 0x41C00000U timer
 * #define XPAR_INTC_0_BASEADDR 0x41200000U attiva il sistema di interrupt
 * #define XPAR_AXI_16LEDS_GPIO_BASEADDR 0x40000000 led
 * #define XPAR_AXI_SWITHES_GPIO_BASEADDR 0x40020000 switch
 * obiettivi del lab:
 * usare timer0 per generare un interrupt ogni secondo per accendere e spegnere uno dei led a ogni esecuzione dell'interrupt
 * permettere all'utente di cambiare il periodo in base alla configurazione degli switch nella scheda

 *
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xtmrctr_l.h"
#include "xparameters.h"
//macro
#define TmrCtrNumber 0 //timer 0
#define ONE_SECOND_PERIOD 100000000	   //definire un secondo
#define TMR_MASK 0x100           // Timer interrupt mask
#define TIMER_INT_SRC 0b100  // Timer interrupt source in INTC
#define SWITCH_INT_SRC 0b0010  // Switch interrupt source in INTC

//definizione degli indirizzi
#define LED XPAR_AXI_16LEDS_GPIO_BASEADDR
#define SWITCHES XPAR_AXI_SWITHES_GPIO_BASEADDR
#define INTERRUPT XPAR_INTC_0_BASEADDR
#define TIMER XPAR_AXI_TIMER_0_BASEADDR

/*definizione registri per il controllo degli interrupt
*registri ISR, IER, IAR, e MER sono utilizzati per il monitoraggio e l'abilitazione/disabilitazione degli interrupt
*/
#define ISR 0x00    //Interrupt Status register
#define IER 0x08 	//Interrupt Enable register
#define IAR 0x0C 	//Interrupt Acknowledge register
#define MER 0x1C	//Master Enable register

// GPIO Peripheral registri, ermettono di abilitare e monitorare gli interrupt per ciascuna periferica
#define IP_IER 0x0128   //Interrupt Enable register
#define GIER 0x011C	    //Global Interrupt Enable register
#define IP_ISR 0x0120	//Interrupt Status Clear register

//funzioni per l�inizializzazione di interrupt, periferiche, timer, e gestione ISR.
void led_ISR(void)__attribute__((interrupt_handler));
void init_interrupt(); //abilita l'interrupt
void init_interrupt_switch(); //abilita l'interrupt switches
void init_timer(int value); //abilita il timer

int main()
{
    init_platform();

    //reset valore led
    *(int* )LED = 0;

    //abilita interrupt
    init_interrupt();

    //abilita interrupt switch
    init_interrupt_switch();

    //inizializza timer ad 1 secondo
    init_timer(ONE_SECOND_PERIOD);


    while (1) {
        //loop
    }
    cleanup_platform();
    return 0;
}
void led_ISR(void){
	//legge l'interrupt dalla fonte
	int interrupt_source = *(int*)INTERRUPT;

	//verifica se l'interrupt arriva dal timer0
	if(interrupt_source & TIMER_INT_SRC){
		//inverti stato led
		*(int*)LED = ~(*(int*)LED);
		//riconosce l'interrupt nel registro del timer quindi
		*(int*)TIMER |= TMR_MASK ; //scrivendo 1 sul bit che segnala l�avvenuta interruzione
		// Notifica all�interrupt controller che l�interrupt del timer � stato servito
		*(int*)(INTERRUPT + IAR) = TIMER_INT_SRC;
	}
	//verifica eventuali interrupt dagli switch
	 if (interrupt_source & SWITCH_INT_SRC) {
	     // Leggi stato switch (inverso per mappare correttamente)
	     int switchInput = ~(*(int*)SWITCHES);
	     int newCounterValue = ONE_SECOND_PERIOD;
	     //switch sulla destra (bit 0-7) periodo dovrebbe aumentare (lampeggio pi� lento)
	     //switch sulla sinistra (bit 7-15) periodo si divide (lampeggio pi� veloce)
	     if (switchInput & 0x00FF ) {
	    	 // Quadruplica il periodo di ciclo del timer in base all'attivazione degli 8 switch piu' a destra
	         int inputSwitch = ( switchInput & 0x00FF );
	    	 newCounterValue = ONE_SECOND_PERIOD * inputSwitch * 2;
	     }else if (switchInput & 0xFF00 ) {
	         // Divide il periodo quando gli 8 switch piu' a sinistra vengono attivati
	    	 int inputSwitch = (( switchInput & 0xFF000 ) >> 8 );
	    	 newCounterValue = ONE_SECOND_PERIOD / ( inputSwitch * 2 );
	     }

	     // Aggiorna timer con nuovo valore
	     XTmrCtr_SetLoadReg(TIMER, TmrCtrNumber, newCounterValue);

	     // Reset interrupt del GPIO
	    *(int*)(SWITCHES + IP_ISR) = 0b11;

	    // Acknowledge interrupt nel controller INTC (INT 1)
	    *(int*)(INTERRUPT + IAR) = 0b10;
	 }
}
void init_interrupt(){
	//Abilita interrupt nel controller INTC
	*(int*)(INTERRUPT + MER) = 0b11;        // Abilita master interrupt
	*(int*)(INTERRUPT + IER) = 0b110;       // Abilita interrupt timer (INT2) e switch (INT1)

	// Abilita interrupt nel processore
	microblaze_enable_interrupts();
}
void init_interrupt_switch(){
	// Abilita interrupt globali e per canali switch
	*(int*)(SWITCHES + GIER) = (1 << 31);   // Global interrupt enable
	*(int*)(SWITCHES + IP_IER) = 0b11;      // Abilita canali interrupt switch
}
void init_timer(int value){
	// Configura timer: abilita down counter, auto reload, interrupt enable
	XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, 0x56);

	// Carica valore timer
	XTmrCtr_SetLoadReg(TIMER, TmrCtrNumber, value);

	// Inizializza contatore timer
	XTmrCtr_LoadTimerCounterReg(TIMER, TmrCtrNumber);

	// Disabilita bit LOAD0 per far partire il timer
	u32 controlStatus = XTmrCtr_GetControlStatusReg(TIMER, TmrCtrNumber);
	XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, controlStatus & (~XTC_CSR_LOAD_MASK));

	// Abilita timer
	XTmrCtr_Enable(TIMER, TmrCtrNumber);
}



