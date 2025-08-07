
/*
 * LAB 1 POLLING MARIA SARA M
 * riferimento agli indirizzi base in memoria che ci servono per questo codice
 * XPAR_GPIO_0_BASEADDR 0x40000000 serve per i led
 * XPAR_GPIO_1_BASEADDR 0x40010000 serve per i buttons
 * XPAR_GPIO_2_BASEADDR 0x40020000 serve per gli switch
 * */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
//definizione indirizzi delle periferiche (mappate in memoria)
#define LED XPAR_GPIO_0_BASEADDR
#define SWITCH XPAR_GPIO_2_BASEADDR

//leggere periodicamente le periferiche di input, la funzione peek legge il valore e lo restituisce
int peek( volatile int* position);
//aggiornare i led con l'ultimo valore letto da input, la funzione poke scrive il valore
void poke(volatile int* position, int value);

int main()
{
    init_platform();
    //un loop che controlla periodicamente gli input e aggiorna i led di conseguenza
    while(1){

    	//Legge lo stato degli switch tramite la funzione peek
    	int status_input = peek((volatile int*)SWITCH);
    	//Se il bit meno significativo (status_input & 1) � impostato a 1, scrive 0xFFFE sui LED
    	if(status_input){
    		poke((volatile int*)LED, (status_input & 0xFFFE)); // Scrive 0xFFFE sui LED
    	}else{
    		//// Se il bit meno significativo degli switch � 0, accende tutti i LED (complemento di status_input)
    		poke((volatile int*)LED, ~(status_input & 0xFFFE)); //Scrive il complemento sui LED
    	}
    	// Delay semplice per rallentare il polling (per evitare che il ciclo giri troppo velocemente)
    	for ( int i = 0; i < 100000; ++i);
    }

    cleanup_platform();
    return 0;
}

//definizione delle funzioni
int peek(volatile int* position){ //legge dal registro all'indirizzo specificato
	return *position; //Restituisce il valore letto dal registro all'indirizzo specificato
}


void poke(volatile int* position, int value){ //scrive un valore nel registro all'indirizzo specificato
	(*position) = value;  //Scrive il valore nel registro all'indirizzo specificato
}

