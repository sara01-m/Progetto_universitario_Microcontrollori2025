
/*Lab 2 maria sara m
 * registri per la periferica UART
 * XPAR_UARTLITE_0_BASEADDR 0x40600000
 * prototipi di funzioni presenti in xuartlite_l.h che ho usato per il loop:
 * void XUartLite_SendByte(UINTPTR BaseAddress, u8 Data); masnda il dato
 * u8 XUartLite_RecvByte(UINTPTR BaseAddress); riceve il dato
 * il loopback � una funzionalit� in cui ogni carattere ricevuto dalla UART viene
 * rispedito indietro allo stesso mittente senza modificarlo.
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"

//definizione registro per la UART
#define UART XPAR_UARTLITE_0_BASEADDR

int main()
{
    init_platform();

    while(1){
    	//funzione che riceve i dati dalla periferica UART
    	char dato = XUartLite_RecvByte(UART);

    	//funzione che rispedisce indietro gli stessi dati creando cos� un loopback
    	XUartLite_SendByte(UART, dato);
    }

    cleanup_platform();
    return 0;
}
