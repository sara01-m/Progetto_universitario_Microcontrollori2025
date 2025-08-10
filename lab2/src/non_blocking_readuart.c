
/*Lab 2 maria sara m
 * registri per la periferica UART
 * XPAR_UARTLITE_0_BASEADDR 0x40600000
 * prototipi di funzioni presenti in xuartlite_l.h che ho usato per il loop:
 * void XUartLite_SendByte(UINTPTR BaseAddress, u8 Data); masnda il dato
 * u8 XUartLite_RecvByte(UINTPTR BaseAddress); riceve il dato
 *
 * se il programma deve leggere led, switch e botton mi servono i loro registri
 * #define XPAR_GPIO_0_BASEADDR 0x40000000 0x40000000 //indirizzo base led
 * #define XPAR_GPIO_3_BASEADDR 0x40010000 //indirizzo base button
 * #define XPAR_GPIO_5_BASEADDR 0x40020000 0x40020000 //indirizzo base switch
 *
 * per verificare quando niente � stato ricevuto uso:
 * #define XUartLite_IsReceiveEmpty(BaseAddress)
 * #define XUartLite_ReadReg(BaseAddress, RegOffset)
 *
 * con gli switch controllo i 4 led meno significativi (led0-led3)
 * con i bottoni controllo i successivi led, dal 4 al 7 (i pulsanti 0-3)
 * i led dal 8-11 vengono controllati dall'UART con '3' si accendono i led 8-11, con 'a' si accendono gli altri
 * i led 12-15 non cambiano mai
 */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"

//definizione registro per la UART
#define UARTB XPAR_UARTLITE_0_BASEADDR
#define LED XPAR_GPIO_0_BASEADDR //indirizzo base led
#define BUTTON XPAR_GPIO_3_BASEADDR //indirizzo base button
#define SWITCH XPAR_GPIO_5_BASEADDR //indirizzo base switch

#define READ_UART XUL_RX_FIFO_OFFSET

#define UART     (u8)2
#define SWITCHES (u8)1
#define BUTTONS  (u8)0

#define UART_NO_DATA 4
#define UART_IGNORE_CHAR -1

int my_XUartLite_RecvByte(UINTPTR BaseAddress);
int convert_dato(char dato);
void update_leds(u32 data, u8 mode);

int main()
{
    init_platform();

    // Registri di input
    u32 uart = 0;
    u32 buttons = 0;
    u32 switches = 0;

    while(1) {
    	//Aggiornamento leds in lettura da buttons
    	buttons = Xil_In32(BUTTON);
    	update_leds(buttons, BUTTONS);

    	//Aggiornamento leds in lettura dagli switch
    	switches = Xil_In32(SWITCH);
    	update_leds(switches, SWITCHES);

    	// Aggiornamento leds in lettura da non-blocking UART
    	uart = my_XUartLite_RecvByte(UARTB);
    	update_leds(uart, UART);

     }

    cleanup_platform();
    return 0;
}
int my_XUartLite_RecvByte(UINTPTR BaseAddress){
	if(XUartLite_IsReceiveEmpty(BaseAddress)){
		return UART_NO_DATA; //niente � ricevuto ma non blocca
	}
	//riceve e legge il dato
	u8 dato = XUartLite_ReadReg(BaseAddress, READ_UART);
	//scarta il dato se � CR
	if(dato == '\r'){
		return UART_IGNORE_CHAR; //ignora il CR
	}
	return dato;
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
void update_leds(u32 data, u8 mode){
	//legge lo stato attuale dei LED
	u32 currentLedStatus = *(int*)LED;
	u32 ledMask          = currentLedStatus;
	u32 newLedOutput     = 0;

	//estrae solo gli 8 bit meno significativi,come char, per la UART
	char received_dato = (u8)data;

	switch(mode){
		//modalit� UART controlla i LED[11:8] con gli ultimi 4 bit ricevuti
		case UART:
			if(received_dato !=4){ // 4 = EOT (End Of Transmission), UART vuota
				if (received_dato == 'a') {
					newLedOutput = 0xFF00; //accende LED[15:8]
				}else{
					//converte il numero da char a int (solo se � '0'-'9')
					int converted_dato = convert_dato(received_dato);
					if(converted_dato != -1){
						//verifico se il numero � valido per i primi 8 led,posiziona i 4 bit su LED[11:8]
						newLedOutput = (converted_dato << 8); //led 8-11

					}else{
						//l'output resta invariato
						//Carattere non valido, non aggiornare nulla
						newLedOutput = currentLedStatus;
					}
				}
				// Gli ultimi 8 led rimangono invariati
				ledMask &= 0x00FF; // mantieni LED 0-7 invariati
			}
		break;

	//SWITCHES: controlla solo LED[3:0] con Switch[3:0]
	case SWITCHES:
		ledMask &= 0xFFF0; // mantieni LED 4-15 invariati
		newLedOutput = ( ~(data) & 0xF ); //switch invertiti su LED 0-3
		break;

	//BUTTONS: controlla solo LED[7:4] con Button[3:0]
	case BUTTONS:
		// mantieni LED 0-3 e 8-15 invariati
		ledMask &= 0xFF0F;
		newLedOutput = ( (data & 0xF) << 4 ); // bottoni su LED 4-7
		break;
	default:
		return;

	}
	//accendo i led tenendo presente lo stato precedente
	Xil_Out32(LED, ledMask | newLedOutput);

}
