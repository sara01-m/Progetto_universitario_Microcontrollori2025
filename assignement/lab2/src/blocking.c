
/*Lab 2 maria sara m
 * registri per la periferica UART
 * XPAR_UARTLITE_0_BASEADDR 0x40600000
 * define presenti in xuartlite_l.h che ho usato per leggere:
 * #define XUartLite_ReadReg(BaseAddress, RegOffset)
 * registro per RegOffset: #define XUL_RX_FIFO_OFFSET	0 ->receive FIFO, read only
 * #define XUartLite_IsReceiveEmpty(BaseAddress)
 *
 * define presenti in xuartlite_l.h che ho usato per scrivere:
 * #define XUartLite_IsTransmitFull(BaseAddress)
 * #define XUartLite_WriteReg(BaseAddress, RegOffset, Data)
 * registro per Offset:#define XUL_TX_FIFO_OFFSET	1 ->transmit FIFO, write only
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"

//definizione registro per la UART
#define UART XPAR_UARTLITE_0_BASEADDR
#define READ_UART XUL_RX_FIFO_OFFSET
#define WRITE_UART XUL_TX_FIFO_OFFSET

char blocking_read_from_UART(); //funzione che legge i dati della UART non appena ci sono dati validi nel FIFO
void blocking_write_from_UART(char dato); //funzione per scrivere i dati nella UART

int main()
{
    init_platform();

    xil_printf("UART attiva. Invia caratteri dal terminale: \r\n"); //

    while(1){
    	char sent_data = blocking_read_from_UART(); //legge un carattere dalla UART
    	blocking_write_from_UART(sent_data); //restituisce il carattere
    }

    cleanup_platform();
    return 0;
}
//lettura bloccante
//char perch� restituisce un dato
char blocking_read_from_UART(){
	while(XUartLite_IsReceiveEmpty(UART)){ //verifica se la FIFO � vuota
		//resta in attesa finch� non riceve un dato
	}
	return XUartLite_ReadReg(UART, READ_UART);
}
void blocking_write_from_UART(char dato){
	while(XUartLite_IsTransmitFull(UART)){ //verifica se la FIFO � piena
		//resta in attesa finch� non c'� spazio nella FIFO
	}
	//scrive il dato ricevuto
	XUartLite_WriteReg(UART, WRITE_UART, dato);
}
