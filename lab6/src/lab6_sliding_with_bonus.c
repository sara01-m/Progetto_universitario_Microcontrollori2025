
/*Lab 6 maria sara m
 * registri per la periferiche che ci servono
 * #define XPAR_AXI_16LEDS_GPIO_BASEADDR 0x40000000 led
 * #define XPAR_AXI_BUTTONS_GPIO_BASEADDR 0x40010000
 * #define XPAR_AXI_TIMER_0_BASEADDR 0x41C00000U timer
 * #define XPAR_INTC_0_BASEADDR 0x41200000U attiva il sistema di interrupt
 * obiettivi del lab:
 *Progettare una Finite State Machine (FSM) in linguaggio C, eseguita su un processore MicroBlaze,
 *Progettare per simulare il funzionamento di un sistema di frecce direzionali (destra/sinistra) di un�auto,
 *Progettare usando LED e pulsanti
 */

#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include "xil_io.h"

//maschere per i bottoni
//mappa i tasti fisici a numeri di bit, usati per estrarre lo stato dei singoli pulsanti.
#define IO_BTN_0 0 // sopra
#define IO_BTN_1 1
#define IO_BTN_2 2 // sotto
#define IO_BTN_3 3
#define IO_BTN_4 4

//Indirizzi delle periferiche
#define BUTTON XPAR_AXI_BUTTONS_GPIO_BASEADDR //base address dei pulsanti
#define LED XPAR_AXI_16LEDS_GPIO_BASEADDR //base address dei led
#define INTERRUPT XPAR_INTC_0_BASEADDR //interrupt controller
#define TIMER XPAR_AXI_TIMER_0_BASEADDR //timer base address

//definizioni macro per timer e interrupt
#define TmrCtrNumber 0 //timer 0
#define ONE_SECOND_PERIOD 100000000 //definire un secondo
#define TIMER_DELAY 5000000
#define TMR_MASK 0x100 // Timer interrupt mask
#define TIMER_INT_SRC 0b100  // Timer interrupt source in INTC

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

//variabili globali per i pulsanti e per i led
volatile int * const btns_reg = (int *)BUTTON; //puntatore ai registri dei bottoni
volatile int * const leds_reg = (int *)LED; //puntatore ai registri dei LED


//per ogni bottone viene tenuto lo stato attuale della macchina
typedef struct _btn_status {
	const u8 btn_n; //numero del bottone (bit)
	u8 last_value; //ultimo valore letto
	u8 counter; //contatore per il debounce
} button_t;

//struttura per la gestione dei vari led
typedef struct _led_status {
	/*lo status inizia da IDLE (nulla) con valore 0x0X
	*successivamente vi son 3 macro stati quali Left 0x1X (X = 0-7), Center 0x2X (X = 0-3), Right 0x3X (X = 0-7) */
	u8 status;
	u16 led_single_status; //valore attuale dei led
} led_t;

//funzioni per l�inizializzazione di interrupt, periferiche, timer, e gestione ISR.
void init_interrupt(); //abilita l'interrupt
void init_timer(int value); //abilita il timer
void blinkISR(void) __attribute__((interrupt_handler)); // Interrupt Service Routine

// funzione per controllare lo stato del bottone
u8 check_button_value(button_t *btn);

// funzione per avanzamento di status
void leds_next_fsm(led_t *leds, button_t *left_btn, button_t *center_btn, button_t *right_btn);
// funzione per attivazione singoli led
void _sync_led_with_status(led_t *led_status);

//inizializza pulsanti e stato dei led
button_t left_btn = (button_t){IO_BTN_1, 0, 0}, //pulsante sinistra
    		center_btn = (button_t){IO_BTN_4, 0, 0}, //pulsante centrale
			right_btn = (button_t){IO_BTN_3, 0, 0}; //pulsante destra
led_t leds = {0x00, 0};

int main()
{
    init_platform();

    //abilita interrupt
    init_interrupt();

    //inizializza timer con il tempo prestabilito
    init_timer(TIMER_DELAY);

    while(1){
     //backgroud task
    }

    cleanup_platform();
    return 0;
}

u8 check_button_value(button_t *btn) {
#ifndef DEBOUNCE_DELAY
#define DEBOUNCE_DELAY 3 //valore di default, aspetta un numero di cicli per evitare che rimbalzi
#endif
	u8 raw = (*btns_reg >> btn->btn_n) & 1; //estrae il valore attuale del bottone

	//controllo dello stato letto
	if (raw == btn->last_value) {
		btn->counter = 0;
	    return 0;  //se lo satto � ugualke al precedente nessun cambiamento
	}

	btn->counter++;

	if (btn->counter >= DEBOUNCE_DELAY) {
		btn->last_value = raw; //registra il nuovo stato
	    btn->counter = 0; //reset contatore
	    if (raw == 1) return 1;  // bottone premuto stabilmente
	}

	return 0;
}

// funzione per avanzamento di status
void leds_next_fsm(led_t *leds, button_t *left_btn, button_t *center_btn, button_t *right_btn) {

	u8 dir = (leds->status >> 4) & 0b11; //estrae lo stato dei pulsanti
	u8 status_number = leds->status & 7; //estrae lo stato attuale dei led

	//legge e verificqa la pressione legata ai tre pulsanti
	u8 left_value = check_button_value(left_btn);
	u8 center_value = check_button_value(center_btn);
	u8 right_value = check_button_value(right_btn);

	if(dir == 0) { // IDLE quindi inizio ciclo
		if(left_value) dir = 1;
		else if(center_value) dir = 2;
		else if(right_value) dir = 3;
	} else { //se siamo gi� in uno stato
		if(left_value && dir == 1){
			status_number = 0;
			dir = 0;
		}else if(center_value && dir == 2){
			status_number = 0;
			dir = 0;
		}else if(right_value && dir == 3){
			status_number = 0;
			dir = 0;
		}else status_number++;
	}

	leds->status = (dir << 4) | (status_number & 7);
	_sync_led_with_status(leds); //aggiorna i led
}
// funzione per attivazione singoli led
void _sync_led_with_status(led_t *led_status) {
	//cerco il macro status
	u8 dir = led_status->status >> 4;
	u8 status_number = led_status->status & 0x0F;
	switch(dir){
		//left
		case 1:
			led_status->led_single_status = (u16)(1 << (status_number + 8));
			break;
		//center
		case 2:
			led_status->led_single_status = (u16)(1 << (status_number + 8)) | (1 << (7 - status_number));
			break;
		//right
		case 3:
			led_status->led_single_status = (u16)(1 << (7 - status_number));
			break;
		default: //idle
			led_status->led_single_status = (u16) 0;
	}
	*leds_reg = led_status->led_single_status; //scrive sui led
}

// Interrupt Service Routine
void blinkISR(void){
	//legge l'interrupt dalla fonte
	int interrupt_source = *(int*)INTERRUPT;

	//verifica se l'interrupt arriva dal timer0
	if(interrupt_source & TIMER_INT_SRC){
		leds_next_fsm(&leds, &left_btn, &center_btn, &right_btn);

		//riconosce l'interrupt nel registro del timer quindi
		*(int*)TIMER |= TMR_MASK ; //scrivendo 1 sul bit che segnala l�avvenuta interruzione
		// Notifica all�interrupt controller che l�interrupt del timer � stato servito
		*(int*)(INTERRUPT + IAR) = TIMER_INT_SRC;
	}
}
void init_interrupt(){
	//Abilita interrupt nel controller INTC
	*(int*)(INTERRUPT + MER) = 0b11;        // Abilita master interrupt
	*(int*)(INTERRUPT + IER) = 0b110;       // Abilita interrupt timer (INT2) e switch (INT1)

	// Abilita interrupt nel processore
	microblaze_enable_interrupts();
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

