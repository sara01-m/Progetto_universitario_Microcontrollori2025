
/*
 * LAB 1 INTERRUPT MARIA SARA M
 * riferimento agli indirizzi base in memoria che ci servono per questo codice
 * XPAR_GPIO_0_BASEADDR 0x40000000 serve per i led
 * C 0x40010000 serve per i buttons
 * XPAR_GPIO_2_BASEADDR 0x40020000 serve per gli switch
 * XPAR_AXI_INTC_0_BASEADDR 0x41200000 controlla l'interrupt
 * */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

//definizione registri
#define LED XPAR_GPIO_0_BASEADDR //indirizzo base led
#define BUTTON XPAR_GPIO_1_BASEADDR //indirizzo base button
#define SWITCH XPAR_GPIO_2_BASEADDR //indirizzo base switch
#define INTERRUPT XPAR_AXI_INTC_0_BASEADDR //indirizzo base interrupt

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

//Questi bit sono usati per identificare quale periferica ha generato l'interruzione
#define GPIO_1 0b01		// buttons (1)
#define GPIO_2 0b10		// switches (2)

//leggere periodicamente le periferiche di input, la funzione peek legge il valore e lo restituisce
int peek( volatile int* position);
//aggiornare i led con l'ultimo valore letto da input, la funzione poke scrive il valore
void poke(volatile int* position, int value);

void my_interrupt(void) __attribute__((interrupt_handler));
int main()
{
    init_platform();

    //controllo dell'interrupt
    poke( (volatile int *)(INTERRUPT + MER), (int)0b11); // Abilita la gestione degli interrupt a livello master
    poke( (volatile int *)(INTERRUPT + IER), (int)0b11); // Abilita gli interrupt per tutte le periferiche

    //switch
    poke( (volatile int *)(SWITCH + GIER), (int)1<<31); //abilita l'interrupt globale per gli switch
    poke( (volatile int *)(SWITCH + IP_IER), (int)0b11); //abilita le periferiche di interrupt per gli switch

    //button
    poke( (volatile int *)(BUTTON + GIER), (int)1<<31); //abilita l'interrupt globale per i bottoni
    poke( (volatile int *)(BUTTON + IP_IER), (int)0b11); //abilita le periferiche di interrupt per i bottoni

    //serve per abilitare l'interrupt nel processore
    microblaze_enable_interrupts();

    poke( (volatile int *)LED, (int)0); //inizializzazione dei led

   while (1) {
      // loop infinito che continua a girare finch� il programma � in esecuzione
   };

    cleanup_platform();
    return 0;
}
//definizione delle funzioni
int peek(volatile int* position){
	return *position; //legge dal registro all'indirizzo specificato
}

void poke(volatile int* position, int value){
	(*position) = value;  //un valore nel registro all'indirizzo specificato
}
void my_interrupt(void){
	int peripheral_input = peek( (volatile int *)INTERRUPT) & 0b11;	//identifica la periferica che ha generato l'interruzione

	if ( peripheral_input == GPIO_1 ) {	 //Se l'interruzione � causata dai bottoni (GPIO_1
			poke( (volatile int *)LED, (int)0); //spegne i led

			poke( (volatile int *)(BUTTON + IP_ISR), (int)0b11); 	//Pulisce lo stato dell'interruzione,  reset GPIO
			poke( (volatile int *)(INTERRUPT + IAR), (int)0b01); 	//Riconosce l'interruzione
		}
		else { //Se l'interruzione � causata dagli switch (GPIO_2)
			poke( (volatile int *)LED, (int)0xFFFF); // Accende tutti i LED
			poke( (volatile int *)(SWITCH + IP_ISR), (int)0b11); 	//Pulisce lo stato dell'interruzione, reset GPIO
			poke((volatile int *)(INTERRUPT + IAR), (int)0b10);		//Riconosce l'interruzione
		}

}
