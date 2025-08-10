
/*Lab 2 maria sara m
 * registri per la periferica UART
 * XPAR_UARTLITE_0_BASEADDR 0x40600000
 * obiettivi del lab:
 * 'a' accende tutti i led quindi 0xFFFF
 * questo lavora solo sui primi 4 led, gli altri restano spenti a meno che non si riceva 'a'`
 * 0 -> binario 0b0000 nessun led acceso
 * 1-> 0b0001 acceso led0
 * 2-> 0b0010 acceso led1
 * 3-> 0b0011 acceso led0 e led1
 * 4-> 0b0100 acceso led2
 * 5-> 0b0101 acceso led2 e led0
 * 6-> 0b0110 acceso led2 e led1
 * 7-> 0b0111 acceso led0 led1 led2
 * 8-> 0b1000 acceso led3
 * 9-> 0b1001 acceso led3 e led0
 * visto che dobbiamo lavorare con i led serve anche quel registro
 * XPAR_GPIO_0_BASEADDR 0x40000000 serve per i led
 *
 * mi serve una funzione che converte il numero perch� quando ricevo un dato ogni byte � un carattere ASCII non un valore numerico direttamente attribuibile a 0-9
 * e XUartLite_RecvByte(UINTPTR BaseAddress); che riceve il dato
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"

//definizione registro per la UART
#define UART XPAR_UARTLITE_0_BASEADDR
#define LED XPAR_GPIO_0_BASEADDR

//funzione che converte il carattere in un numero
int convert_dato(char dato);
int main()
{
    init_platform();

    xil_printf("Polling UART started...\n");
    xil_printf("il carattere a accende tutti i led\n");
    xil_printf("i numeri da 0-9 accendono solo i led in base alla loro rappresentazione binaria \n");
    while(1){
    	//funzione che legge un singolo byte ricevuto sulla UART
    	char received_data = XUartLite_RecvByte(UART);

    	//se il byte in ricezione � 'a' si accendono tutti i led
    	if(received_data == 'a'){
    		*(int *)LED = 0xFFFF;
    	}else {
    		//i led vengono convertiti in intero e accesi in base alla loro rappresentazione binaria
    		int converted_dato = convert_dato(received_data);
    		if(converted_dato != -1){ //se il numero non � -1 allora � un numero
    			 *(int*)LED = converted_dato;
    		}
    	}
    }

    cleanup_platform();
    return 0;
}
int convert_dato(char dato){
	//Converte numero di tipo char quindi ASCII nel suo equivalente di tipo intero
	if(dato < '0' || dato > '9'){
		return -1; //non � un numero valido
	}else{
		//converti il numero nel suo corrispettivo int
		return dato - '0'; //sottraendo '0'=48 da un carattere numerico ottieni il suo valore intero.
	}
}
