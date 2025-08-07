
/*Laboratorio 7 Maria Sara M
 * obiettivi del lab:
 * decodificare i segnali inviati da un telecomando IR che utilizza il protocollo NEC
 * */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h" //controllo timer
#include "xintc.h" //funzioni per interazioni interrupt

//definizioni per il timer (in xparameters.h)
#define TIMER_N XPAR_AXI_TIMER_0_DEVICE_ID
#define TIMER_BASE XPAR_AXI_TIMER_0_BASEADDR //0x41C00000U
#define RESET_VALUE (u32)1000 // 10 microsecondo a 100 MHz
#define INTERRUPT_N XPAR_INTC_0_DEVICE_ID //identificativo del dispositivo per il controller di interrupt

#define IR (*ir_input ^ 1) //legge il valore puntato da ir_input e lo inverte
#define NOT_IR (*ir_input) //valore non invertito dell'ingresso IR

//NEC protocol definition timing in micro seconds
//specificano la durata (in microsecondi) del segnale di start del protocollo NEC
#define NEC_START_SIGNAL_US 9000  //9000 µs di burst iniziale
#define NEC_START_DATA_SIGNAL_US 4500 //4500 µs di spazio successivo

#define NEC_BIT_START_US 563 //ogni bit inizia con un burst da 563 µs
#define NEC_BIT_ZERO_END_US NEC_BIT_START_US //un "0" logico è seguito da uno spazio breve (563 µs)
#define NEC_BIT_ONE_END_US 1690 //un "1" logico da uno spazio lungo (1690 µs)
#define NEC_MESSAGE_LEN 32 //lunghezza messaggio NEC
#define NEC_1_THRESHOLD 1000 //Soglia per distinguere tra un "0" e un "1" in base alla durata dello spazio dopo il burst

#define DEBUG

//variabile aggiornata ad ogni interrupt del timer 0
volatile static u64 us_from_start = 0; //contatore µs dall'inizio
volatile int * const ir_input = (int *)XPAR_GPIO_IR_BASEADDR; //input IR da GPIO

XTmrCtr_Config *timerConfig = NULL; //config timer
XTmrCtr timerControl; //controllo timer
XIntc interruptControl; //controllo interrupt

//struttura per il messaggio Nec
typedef struct _NEC_MESSAGE {
	u8 address;
	u8 inverse_address;
	u8 command;
	u8 inverse_command;
} NECMessage;

typedef struct _SIMIL_TV {
	u8 status_bit; // 1: on/off, 2: eq on/off, 3: PAUSE/START
	u8 current_channel;
	u8 volume_value;
} TV;

//struttura per associare un nome ai codici dei tasti ricevuti tramite telecomando IR
typedef enum _BTN_MAP{
	POWER_ON = 0xA2, FUNC = 0xE2,
	VOL_UP = 0x62, VOL_DOWN = 0xA8, PREV = 0x22, PAUSE = 0x02, NEXT = 0xC2,
	ARROW_UP = 0x90, ARROW_DOWN = 0xE0, EQ = 0x98, ST_REPT = 0xB0,
	ZERO = 0x68, ONE = 0x30, TWO = 0x18, THREE = 0x7A, FOUR = 0x10, FIVE = 0x38, SIX = 0x5A, SEVEN = 0x42, EIGHT = 0x4A, NINE = 0x52
} button_value_t;

TV tv_object = (TV) {0, 0, 0}; //stato iniziale tv

void print_b(u64 n, u64 dim); //stampa binario
void print_h(u64 n, u64 dim); //stampa esadecimale
u32 handle_NEC_protocol(); //gestisce ricezione NEC
void timer_handler(void *_, u8 timer_n); //ISR  timer
void setup_interrupt(); //set interrupt
void init_timer(u16 timer_n); //inizializza timer
NECMessage validate_NEC_data(u32 data); //valida messaggio NEC
void manage_tv_status(NECMessage message); //gestisce stato TV


int main()
{
    init_platform();

    print("TV NEC\ntelevisione spenta.\n\r"); //messaggio iniziale

    init_timer(TIMER_N); //inizializza timer

    while(1){
    	u32 nec_message = handle_NEC_protocol(); //ricevi messaggio IR
    	if(nec_message != 0){ //se valido
    		NECMessage message = validate_NEC_data(nec_message); //estrae dati ricevuti
#ifdef DEBUG
    		print("ricevuto: ");
    		print_b(nec_message, 32); //stampa binaria
    		print("Comando ricevuto: ");
    		print_h(message.command, 8); //stampa comando

#endif
    		manage_tv_status(message); //aggiorna stato tv
    	}
    }

    cleanup_platform();
    return 0;
}
//funzioni
void print_b(u64 n, u64 dim) {  //stampa n in binario con dimensione bit
	print("0b"); //prefisso binario
	for (int i = 0; i < dim; i++){ // scorri i bit da sinistra
		print((n >> ((dim-1) - i)) & 1 ? "1" : "0");   //stampa 1 o 0
	}
	print("\n\r");
}

void print_h(u64 n, u64 dim) { //stampa n in esadecimale
	char buf[2] = {0}; //buffer per un carattere
	if(dim < 4) dim = 4; // minimo 1 cifra hex
	print("0x");  // prefisso esadecimale
	for(int i = dim; i > 0; i -= 4){ //ogni 4 bit → 1 cifra hex
		u8 partial = n >> (i - 4) & 0xF;
		buf[0] = partial < 10 ? 0x30 + partial : 0x41 + partial - 10; //converte in ASCII ('0'-'9')
		print(buf);
	}
	print("\n\r");
}

u32 handle_NEC_protocol() {
	u32 message = 0; // messaggio finale
	u64 time_start; // tempo di inizio bit

	//attesa
	while(NOT_IR);

	//partenza del messaggio
	while(IR);
	//gap prima dei dati
	while(NOT_IR); // attesa inizio dati

	for(int bit_n = 0; bit_n < NEC_MESSAGE_LEN; bit_n++) {
		while(IR);
		time_start = us_from_start;
		while(NOT_IR);

		u8 bit_value = !((us_from_start - time_start) < ((NEC_BIT_ONE_END_US + NEC_BIT_ZERO_END_US)/2.0));
		message = (message << 1) | bit_value;
	}

	return message; // ritorna messaggio a 32 bit
}

void timer_handler(void *_, u8 timer_n) {
    // Incrementa la variabile condivisa.
    // L'interrupt viene gestito (azzerato il flag) dal driver principale
    // XTmrCtr_InterruptHandler, quindi non dobbiamo farlo qui.
    us_from_start += 10; // ISR timer: aumenta contatore µs (ogni 10 µs circa)
}

void setup_interrupt(){
	//inizializzo oggetto interrupt
	XIntc_Initialize(&interruptControl, INTERRUPT_N);
	//connetti con il timer per gestire il suo interrupt
	XIntc_Connect(&interruptControl, TIMER_N, (XInterruptHandler)XTmrCtr_InterruptHandler, (void *)&timerControl);
	//imposta handler custom
	XTmrCtr_SetHandler(&timerControl, timer_handler, (void *)&timerControl);
	//starta l'interrupt controller
	XIntc_Start(&interruptControl, XIN_REAL_MODE);
	//abilita interrupt per il timer 0
	XIntc_Enable(&interruptControl, TIMER_N);
	//abilita gli interrupt
	microblaze_enable_interrupts();
}

void init_timer(u16 timer_n) {
	//carico oggetto di configurazione per timer_n
	timerConfig = XTmrCtr_LookupConfig(timer_n);
	//inizializzo l'oggetto che gestisce il timer
	XTmrCtr_CfgInitialize(&timerControl, timerConfig, TIMER_BASE);
	//inizializzo componente hardware
	XTmrCtr_InitHw(&timerControl);
	//inizializzo timer
	XTmrCtr_Initialize(&timerControl, timer_n);
	//imposto la configurazione del timer_n
	//XTC_DOWN_COUNT_OPTION -> abilita il conto alla rovescia
	//XTC_INT_MODE_OPTION -> abilita l'interrupt del timer
	//XTC_AUTO_RELOAD_OPTION -> quando il timer completa si auto ricarica
	XTmrCtr_SetOptions(
			&timerControl, (u8)timer_n,
			XTC_DOWN_COUNT_OPTION | XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION
	);
	//imposta il valore che deve esser ricaricato ogni volta che il timer si riavvia
	//oppure quando esso si avvia per la prima volta
	XTmrCtr_SetResetValue(&timerControl, timer_n, RESET_VALUE);
	//imposto la funzione di handling
	setup_interrupt();
	//avvia il timer
	XTmrCtr_Start(&timerControl, timer_n);
}
NECMessage validate_NEC_data(u32 data) {
	NECMessage message = {
			.address = data >> 24, // byte 1: indirizzo
			.inverse_address = (data >> 16) & 0xFF, // byte 2: inv. indirizzo
			.command = (data >> 8) & 0xFF,  // byte 3: comando
			.inverse_command = data & 0xFF // byte 4: inv. comando
	};
	// Validazione corretta: indirizzo e comando devono essere il complemento
	// bit a bit dei loro inversi. (valore ^ ~valore) deve dare 0.
	// Oppure, (valore ^ valore_inverso) deve dare 0xFF
	if (((message.address ^ message.inverse_address) == 0xFF) &&
	   ((message.command ^ message.inverse_command) == 0xFF)) {
	        return message;
	}

	print("checksum errato");
	return (NECMessage){0, 0, 0, 0};
}
/*questa funzione riceve un comando dal telecomando IR,
 * lo confronta con i codici conosciuti tramite switch su button_value_t e aggiorna lo stato della TV*/
void manage_tv_status(NECMessage message) {
	switch((button_value_t) message.command) {   // controlla comando ricevuto
		case POWER_ON:
			tv_object.status_bit ^= 1; //on -> off; off -> on
			print(tv_object.status_bit & 1 ? "TV accesa" : "TV spenta");
			break;
		case FUNC:
			print("FUNC attualmente non supportato");
			break;
		case VOL_UP:
			if(tv_object.volume_value != ~((u8)0))tv_object.volume_value++;
			print("Volume aumentato");
			break;
		case VOL_DOWN:
			if(tv_object.volume_value != 0)tv_object.volume_value--;
			print("Volume diminuito");
			break;
		case PREV:
			print("Esegui upgrade a TV+ per questa funzione!");
			break;
		case PAUSE:
			print("Esegui upgrade a TV+ per questa funzione!");
			break;
		case NEXT:
			print("Esegui upgrade a TV+ per questa funzione!");
			break;
		case ARROW_UP:
			tv_object.current_channel++;
			print("Prossimo canale");
			break;
		case ARROW_DOWN:
			tv_object.current_channel--;
			print("Canale precedente");
			break;
		case EQ:
			tv_object.status_bit ^= 2; //on -> off; off -> on
			print(tv_object.status_bit >> 1 & 1 ? "Equalizatore acceso" : "Equalizatore spento");
			break;
		case ST_REPT:
			print("REPT attualmente non supportato");
			break;
		case ZERO:
			print("pulsante 0");
			break;
		case ONE:
			print("pulsante 1");
			break;
		case TWO:
			print("pulsante 2");
			break;
		case THREE:
			print("pulsante 3");
			break;
		case FOUR:
			print("pulsante 4");
			break;
		case FIVE:
			print("pulsante 5");
			break;
		case SIX:
			print("pulsante 6");
			break;
		case SEVEN:
			print("pulsante 7");
			break;
		case EIGHT:
			print("pulsante 8");
			break;
		case NINE:
			print("pulsante 9");
			break;
		default:
			print("comando non riconosciuto");
	}
	print("\n\r");
}

