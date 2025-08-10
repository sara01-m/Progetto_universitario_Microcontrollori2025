
/*Lab 4 maria sara m
 * registri per la periferiche che ci servono
 * #define XPAR_AXI_7SEGS_GPIO_BASEADDR 0x40040000
 * #define XPAR_AXI_7SEGSAN_GPIO_BASEADDR 0x40050000
 * #define XPAR_AXI_TIMER_0_BASEADDR 0x41C00000U timer
 * #define XPAR_INTC_0_BASEADDR 0x41200000U attiva il sistema di interrupt
 * obiettivi del lab:
 *integrazione di timer, interrupt e GPIO per il controllo software di un display a 7 segmenti a 8 cifre
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include <stdlib.h>
#include <string.h>

// Definizioni Macro Timer
#define TmrCtrNumber    0 //timer 0
#define TIMER_INT_SRC   0b0100 // Timer interrupt source in INTC
#define TIMER_RESET     0x56 // Comando per resettare il timer
#define TIMER_MASK     0x100 // Timer interrupt mask
#define TIMER_VALUE  250000 //timer con un valore di carico fisso

// Bit di controllo per il punto decimale del display
#define SEVEN_SEG_CDP 0b10000000 //catodo dei display

//Indirizzi delle periferiche
#define SEVEN_SEG_ANODE XPAR_AXI_7SEGSAN_GPIO_BASEADDR //registro che controlla gli anodi dei display
#define SEVEN_SEG_CATODE XPAR_AXI_7SEGS_GPIO_BASEADDR //registro che controlla i catodi dei display
#define INTERRUPT XPAR_AXI_INTC_0_BASEADDR //registro del controller interrupt
#define TIMER     XPAR_AXI_TIMER_0_BASEADDR //registro del timer

/*definizione registri per il controllo degli interrupt
*registri ISR, IER, IAR, e MER sono utilizzati per il monitoraggio e l'abilitazione/disabilitazione degli interrupt
*/
#define ISR 0x00    //Interrupt Status register
#define IAR 0x0C // Interrupt Acknowledge Register
#define IER 0x08 // Interrupt Enable Register
#define MER 0x1C // Master Enable Register


//risoluzione memoria dei registri
volatile int *seven_seg_anode = (int *) SEVEN_SEG_ANODE;  // Puntatore al registro degli anodi
volatile int *seven_seg_catode = (int *) SEVEN_SEG_CATODE;  // Puntatore al registro dei catodi

//creo una funzione che mi seleziona il numero del display che userò
void use_display_n(u8 display_n);  // Attiva solo il display "n"
//creo la funzione dello step 3
void write_digit(u8 digit, u8 dotted); // Scrive un numero o lettera
//interrupt routine
void timerISR(void) __attribute__((interrupt_handler));
void setup_interrupt(); // Configura interrupt e timer

char buffer[8]; //contiene i caratteri da visualizzare
int disp_n = 0; //tiene traccia del display attualmente attivo

int main()
{
    init_platform();

    setup_interrupt();
    strcpy(buffer, "91ABCDEF"); //Carica la stringa nei display

    cleanup_platform();
    return 0;
}

void setup_interrupt() {
	// gestione interrupt
	*(int *)(INTERRUPT + MER) = 0b11; // Abilita MER
	*(int *)(INTERRUPT + IER) = 0b110; // Abilita IER

	// Abilita interrupt nel processore
	microblaze_enable_interrupts();

	// Configura il timer:
    XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, TIMER_RESET); // Reset timer
    XTmrCtr_SetLoadReg(TIMER, TmrCtrNumber, TIMER_VALUE);  // Imposta valore di carico
    XTmrCtr_LoadTimerCounterReg(TIMER, TmrCtrNumber); // Carica il contatore


    u32 ctrlStatus = XTmrCtr_GetControlStatusReg(TIMER, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(TIMER, TmrCtrNumber, ctrlStatus & (~XTC_CSR_LOAD_MASK));
    // Avvia il timer
    XTmrCtr_Enable(TIMER, TmrCtrNumber);
}
//Interrupt Service Routine del Timer
void timerISR() {
	// Attiva il display corrente
	use_display_n(disp_n);

	// Legge il carattere da visualizzare
	u8 current_char = buffer[7-disp_n++]; // Ciclo da buffer[7] a buffer[0] (ordine inverso)
	// Conversione ASCII → numero da 0 a 15:
	current_char = 0x1 & (current_char >> 6)
			? current_char - 0x41 + 10 // Se è una lettera (A–F), converte da 'A' (65) a 10
			: current_char - 0x30;     // Se è un numero (0–9), converte da '0' (48) a 0
	// Mostra la cifra sul display
	write_digit(current_char, 0);
	// Passa al prossimo display
	if(disp_n > 7) disp_n = 0;

	// Riconosce l'interrupt del timer (reset del flag di interrupt interno)
    *(int*)TIMER |= TIMER_MASK;

    // Riconosce l'interrupt globale nel controller
    *(int*)(INTERRUPT + IAR) = TIMER_INT_SRC;	// Acknowledge Global Interrupt
}
// Attiva un singolo display tra gli 8
void use_display_n(u8 display_n) {
	// 1 << display_n mi consente di spostare il bit di `display_n` volte
	// inverto il numero affinche' l'unico 1 diventi 0 e il resto faccia da mask
	// imposto a 0 il bit desiderato
	*seven_seg_anode = 0xFF & (~(1 << display_n));
}

void write_digit(u8 digit, u8 dotted) {
	digit &= 0x0F; // Mantiene solo i 4 bit meno significativi
	dotted &= 0x1; // Mantiene solo il bit meno significativo
	 // Mappa binaria dei segmenti per numeri e lettere (7 bit + 1 bit per DP)
	u8 numbers_map[] = {
			0b00111111, //0
			0b00000110, //1
			0b01011011, //2
			0b01001111, //3
			0b01100110, //4
			0b01101101, //5
			0b01111101, //6
			0b00000111, //7
			0b01111111, //8
			0b01101111, //9
			0b01110111, // A
			0b01111100, // B
			0b00111001, // C
			0b01011110, // D
			0b01111001, // E
			0b01110001  // F
	};
	// 0b00000001 -> center top
	// 0b00000010 -> right top
	// 0b00000100 -> right bottom
	// 0b00001000 -> center bottom
	// 0b00010000 -> left bottom
	// 0b00100000 -> left top
	// 0b01000000 -> center center
	// 0b10000000 -> point

    // Composizione finale del valore da scrivere sui catodi
	*seven_seg_catode = (~(dotted) << 7) | ~numbers_map[digit];
}
