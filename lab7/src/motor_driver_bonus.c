
/*Laboratorio 7 Motor Driven  Maria Sara M
 * obiettivi del lab:
 * emulate motor dirvier with IR and LEDS
 * controllare un driver per motori DC, il TB6612FNG, utilizzando segnali PWM per la velocità e un telecomando a infrarossi (IR) per i comandi
 * timer 0 incrementa us_from_start for IR timing
 * timer 1 aggiorna periodicamente lo stato del motore (pwm e direzione)
 *
 * #define XPAR_INTC_0_TMRCTR_0_VEC_ID XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR
 * #define XPAR_INTC_0_TMRCTR_1_VEC_ID XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_1_INTERRUPT_INTR
 * #define XPAR_GPIO_4_BASEADDR 0x40000000
 * #define XPAR_GPIO_0_BASEADDR 0x40050000
 * Definitions for peripheral AXI_TIMER_0 e AXI_TIMER_1
 * #define XPAR_AXI_TIMER_0_DEVICE_ID 0U
 * #define XPAR_AXI_TIMER_1_DEVICE_ID 1U
 *Definitions for peripheral GP_LEDS
 *#define XPAR_GP_LEDS_BASEADDR 0x40000000
 *Definitions for peripheral GPIO_IR
 *#define XPAR_GPIO_IR_BASEADDR 0x40040000
 * */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h" //controllo timer
#include "xintc.h" //funzioni per interazioni interrupt

//indirizzi Led (in xparameters.h)
#define LED XPAR_GP_LEDS_BASEADDR //indirizzo led rgb controllati in pwm
#define LED_GPIO_0 XPAR_GPIO_0_BASEADDR //inidizzo led che indicano la posizione
#define IR_GPIO XPAR_GPIO_IR_BASEADDR

//definizioni per il timer0 (in xparameters.h)
#define TIMER_0 XPAR_INTC_0_TMRCTR_0_VEC_ID //vettore di interrupt
#define TIMER_ID_0 XPAR_AXI_TIMER_0_DEVICE_ID //Id del timer richiesta da XTmCtr_lookupconfig
#define TIMER_BASE_0 XPAR_AXI_TIMER_0_BASEADDR //0x41C00000U
#define RESET_VALUE (u32)1000 // 10 microsecondo a 100 MHz
#define INTERRUPT_0 XPAR_INTC_0_DEVICE_ID //identificativo del dispositivo per il controller di interrupt

//definizioni per il timer1 (in xparameters.h)
#define TIMER_1  XPAR_INTC_0_TMRCTR_1_VEC_ID //vettore di interrupt
#define TIMER_ID_1 XPAR_AXI_TIMER_1_DEVICE_ID  //Id del timer richiesta da XTmCtr_lookupconfig
#define TIMER_BASE_1 XPAR_AXI_TIMER_1_BASEADDR //0x41C00000U
#define RESET_VALUE (u32)1000 // 10 microsecondo a 100 MHz
#define INTERRUPT_1 XPAR_INTC_0_DEVICE_ID //identificativo del dispositivo per il controller di interrupt

#define TmrCtrNumber  0 //costante per il numero del timer
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

//costanti simboliche per rappresentare i quattro stati di funzionamento del motore
#define MOTOR_STOP   0
#define MOTOR_CW     1
#define MOTOR_CCW    2
#define MOTOR_BRAKE  3

//valore dei Led
#define LED_RIGHT 0b1 //led motore A
#define LED_LEFT 0b10 //led motore A
#define RGB_OFF 0b111 //led motore B
#define RGB_RED 0b011 //led motore B
#define RGB_BLUE 0b110 //led motore B
#define RGB_GREEN 0b101 //led motore B

//variabile aggiornata ad ogni interrupt del timer 0
volatile static u64 us_from_start = 0; //contatore µs dall'inizio
volatile int * const ir_input = (int *)IR_GPIO; //input IR da GPIO

//istanze per il timer 0 e dell'interrupt 0 collegate con il timer
XTmrCtr_Config *timerConfig_0 = NULL; //config timer 0
XTmrCtr timerControl_0; //controllo timer 0
XIntc interruptControl_0; //controllo interrupt 0

//istanze per il timer 1 e dell'interrupt 1 collegate con il timer
XTmrCtr_Config *timerConfig_1 = NULL; //config timer 1
XTmrCtr timerControl_1; //controllo timer 1
XIntc interruptControl_1; //controllo interrupt 1

//struttura per il messaggio Nec
typedef struct _NEC_MESSAGE {
	u8 address;
	u8 inverse_address;
	u8 command;
	u8 inverse_command;
} NECMessage;

//struttura per associare un nome ai codici dei tasti ricevuti tramite telecomando IR
typedef enum _BTN_MAP{
	// controlli motore A
	POWER_ON = 0xA2, //stop A
	VOL_UP = 0x62, //aumento pwm (velocita motore A)
	VOL_DOWN = 0xA8, //diminuisco pwm (velocita motore A)
	PREV = 0x22, //CCW counter clock wise (rotazione antioraria del motore A)
	PAUSE = 0x02, //short brake A
	NEXT = 0xC2, // CW clock wise (rotazione oraria del motore A)
	// controlli motore B
	ONE = 0x30, // stop B
	TWO = 0x18, //aumento pwm (velocita motore B)
	FOUR = 0x10, //CCW counter clock wise (rotazione antioraria del motore B)
	FIVE = 0x38, //short brake B
	SIX = 0x5A, // CW clock wise (rotazione oraria del motore B)
	EIGHT = 0x4A //diminuisco pwm (velocita motore B)
} button_value_t;

typedef struct direction{
	int pwmA;
	int pwmB;
	int dirA;
	int dirB;
}Direction;

//puntatore che punta direttamente all'indirizzo di memoria del registro hardware che controlla i LED
volatile Direction dir = {.pwmA = 0, .pwmB = 0, .dirA = 0, .dirB = 0};

// Indirizzo Registro LED
volatile int* AXI_LEDS = (int*)LED;
// Indirizzo registro RGB LED
volatile int* AXI_RGBLEDS = ((int*)LED) + 2;

// Variabili Globali per pwm
int rightLedA = 0; //variabile che memorizza il livello di luminosità
int leftLedA = 0; //variabile che memorizza il livello di luminosità
int redLedB = 0; //variabile che memorizza il livello di luminosità
int blueLedB = 0; //variabile che memorizza il livello di luminosità
int greenLedB = 0; //variabile che memorizza il livello di luminosità
int cnt = 0; //contatore per generare il segnale a modulazione di larghezza di impulso pwm che simula la luminosità variabile dei led

void enable_interrupts(); //set interrupts
void timer_handler_0(void *_, u8 timer_0); //ISR  timer0
void timer_handler_1(void *_, u8 timer_1); //ISR  timer1
void set_dir(Direction dir); //Funzione per impostare la direzione del motore
void manage_pwm(); //Funzione per impostare il PWM
void set_led_state(int, int); //imposta i livelli di luminosità in base alla direzione
void enable_timer_channel_0(); //inizializza timer0
void enable_timer_channel_1(); //inizializza timer1
u32 handle_NEC_protocol(); //gestisce ricezione NEC
NECMessage validate_NEC_data(u32 data); //valida messaggio NEC
void manage_command(NECMessage message); //gestisce comandi stato motore

//funzioni per il debug
void print_b(u32 n, u32 dim);
void print_h(u32 n, u32 dim);

int main()
{
    init_platform();
    *AXI_RGBLEDS = RGB_OFF;

    print("Motor spento.\n\r"); //messaggio iniziale

    //inizializza timer
    enable_timer_channel_0();// upcount
    enable_timer_channel_1();// downcount
    enable_interrupts(); //inizializza l'interrupt

    print("Motor on.\n\r");

    while(1){
    	 u32 raw_data = handle_NEC_protocol(); //riceve messaggio IR
    	 print("Messaggio NEC ricevuto:\n\r");
    	 print_b(raw_data, 32);

    	 NECMessage message = validate_NEC_data(raw_data); //estrae dati ricevuti
    	 manage_command(message); //aggiorna stato motor driver

    	 //stampe per il debug
    	 print("PWMA attuale: ");
    	 print_h(dir.pwmA, 8);
    	 print("PWMB attuale: ");
    	 print_h(dir.pwmB, 8);
    }

    cleanup_platform();
    return 0;
}
//funzioni
void print_b(u32 n, u32 dim) {  //stampa n in binario con dimensione bit
	print("0b"); //prefisso binario
	for (int i = 0; i < dim; i++){ // scorri i bit da sinistra
		print((n >> ((dim-1) - i)) & 1 ? "1" : "0");   //stampa 1 o 0
	}
	print("\n\r");
}
void print_h(u32 n, u32 dim) { //stampa n in esadecimale
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
void enable_interrupts(){
	//Inizializza il controller degli interrupt
	XIntc_Initialize(&interruptControl_0, INTERRUPT_0);

	//connetti gli hadler dei due timer
	XIntc_Connect(&interruptControl_0, TIMER_0, (XInterruptHandler)XTmrCtr_InterruptHandler, (void *)&timerControl_0);
	XIntc_Connect(&interruptControl_0, TIMER_1, (XInterruptHandler)XTmrCtr_InterruptHandler, (void *)&timerControl_1);

	//imposta handler custom
	XTmrCtr_SetHandler(&timerControl_0, timer_handler_0, (void *)&timerControl_0);
	XTmrCtr_SetHandler(&timerControl_1, timer_handler_1, (void *)&timerControl_1);

	//avvia l'interrupt controller
	XIntc_Start(&interruptControl_0, XIN_REAL_MODE);

	//abilita interrupt per il timer 0 e timer 1
	XIntc_Enable(&interruptControl_0, TIMER_0);
	XIntc_Enable(&interruptControl_0, TIMER_1);

	//abilita gli interrupt
	microblaze_enable_interrupts();
}
void timer_handler_0(void *_, u8 timer_0) {
    // Incrementa la variabile condivisa.
    // L'interrupt viene gestito (azzerato il flag) dal driver principale
    // XTmrCtr_InterruptHandler, quindi non dobbiamo farlo qui.
    us_from_start += 10; // ISR timer: aumenta contatore µs (ogni 10 µs circa)
}
void timer_handler_1(void *_, u8 timer_1){
	//routine di interrupt (ISR)
	//aggiorna costantemente l'output fisico (i LED) per riflettere lo stato attuale del motore memorizzato nella variabile globale dir
	manage_pwm(); //gestisce la velocità del motore che emula la luminosità dei led
	set_dir(dir); //imposta la direzione del motore e imposta i pin del GPIO di direzione, infatti indica la modalità attuale
}

void set_dir(Direction dir){
	//traduce lo stato logico della direzione del motore in un segnale fisico da inviare ai pin del GPIO che controllano la direzione
	//volatile int *led_out = AXI_LEDS;
	volatile u32 *led_out = (u32 *)LED_GPIO_0;

	switch(dir.dirA) { //la variabile dirA memorizza la modalità di funzionamento attuale del motore
		case MOTOR_CW:
			*led_out = 0x01; // rotazione oraria → accendi bit 0
	        break;
	    case MOTOR_CCW:
	    	*led_out = 0x02; // antioraria → accendi bit 1
	        break;
	    case MOTOR_BRAKE:
	    	*led_out = 0x03; // freno → accendi entrambi
	        break;
	    case MOTOR_STOP:
	    default:
	    	*led_out = 0x00; // tutto spento
	        break;
	}
}

void manage_pwm(){
	cnt = (cnt < 512) ? (cnt + 1) : 0;

	//Inizia da tutti i LED spenti
	u32 new_led_valueA = 0,
			new_led_valueB = 0b111;

	// Applica il livello di luminosità al LED DESTRO (controllato dal bit 0)
	if (cnt < rightLedA) {
		new_led_valueA |= LED_RIGHT;
	}
	// Applica il livello di luminosità al LED SINISTRO (controllato dal bit 1)
	if (cnt < leftLedA) {
		new_led_valueA |= LED_LEFT;
	}
	//applica il livello di luminosità ai LED RGB
	if (cnt < blueLedB) {
			new_led_valueB &= RGB_BLUE; //blu
	}
	if (cnt < greenLedB) {
		new_led_valueB &= RGB_GREEN; //verde
	}
	if (cnt < redLedB){
		new_led_valueB &= RGB_RED; //rosso
	}
	//Scrive il valore finale completo al registro dei LED
	*AXI_LEDS = new_led_valueA;
	*AXI_RGBLEDS = new_led_valueB;

}

void set_led_state(int directionA, int directionB) {
	//imposta i livelli di luminosità in base alla direzione
	// Mappa il livello PWM [0-255] al range del ciclo PWM [0-511]
	int pwm_intensityA = dir.pwmA * 2,
			pwm_intensityB = dir.pwmB * 2;

	// Per evitare che un valore > 255 causi un overflow visivo
	if (pwm_intensityA > 511) {
	    pwm_intensityA = 511; //valore massimo di intensità raggiungibile
	}
	if (pwm_intensityB > 511) {
		pwm_intensityB = 511;
	}
	//Resetta sempre entrambi i livelli all'inizio.
	leftLedA = 0;
	rightLedA = 0;
	redLedB = 0;
	blueLedB = 0;
	greenLedB = 0;

    switch(directionA) {
        case MOTOR_STOP:
        	//per lo stop i livelli rimangono a zero
            leftLedA = rightLedA = 0;
			break;
        case MOTOR_CW:
        	//Per CW, accende il LED DESTRO
        	rightLedA = pwm_intensityA;
            break;
        case MOTOR_CCW:
        	//Per CCW, accende il LED SINISTRO
        	leftLedA = pwm_intensityA;
            break;
        case MOTOR_BRAKE:
        	//Per il freno, accende entrambi
        	leftLedA = 255;
        	rightLedA = 255;
            break;
        default:
        	leftLedA = rightLedA = 0;
            break;
    }
    switch(directionB) {
    	case MOTOR_CW:
    		//Per CW, accende il LED blu
    		blueLedB = pwm_intensityB;
    		break;
    	case MOTOR_CCW:
    		//Per CCW, accende il LED verde
    		greenLedB = pwm_intensityB;
    		break;
    	case MOTOR_BRAKE:
    		//per il freno si accende solo il rosso
    		redLedB = 255;
    		blueLedB = 0;
    		greenLedB = 0;
    		break;
    	default: //valido anche per lo stato di stop
    		redLedB = blueLedB = greenLedB = 0;
    		break;
    }
}

void enable_timer_channel_0(){
	//creo la configurazione per timer0
	timerConfig_0 = XTmrCtr_LookupConfig(TIMER_ID_0);
	//inizializzo l'oggetto che gestisce il timer0
	XTmrCtr_CfgInitialize(&timerControl_0, timerConfig_0, TIMER_BASE_0);

	//inizializzo componente hardware
	XTmrCtr_InitHw(&timerControl_0);

	//inizializzo timer0
	XTmrCtr_Initialize(&timerControl_0, TIMER_ID_0);
	//imposto la configurazione del timer0
	//XTC_DOWN_COUNT_OPTION -> abilita il conto alla rovescia
	//XTC_INT_MODE_OPTION -> abilita l'interrupt del timer0
	//XTC_AUTO_RELOAD_OPTION -> quando il timer0 completa si auto ricarica
	XTmrCtr_SetOptions(
			&timerControl_0,TmrCtrNumber,
			XTC_DOWN_COUNT_OPTION | XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION
	);
	//imposta il valore che deve esser ricaricato ogni volta che il timer0 si riavvia
	//oppure quando esso si avvia per la prima volta
	XTmrCtr_SetResetValue(&timerControl_0, TmrCtrNumber, RESET_VALUE);

	//avvia il timer0
	XTmrCtr_Start(&timerControl_0, TmrCtrNumber);
}
void enable_timer_channel_1(){
	//carico oggetto di configurazione per timer1
	timerConfig_1 = XTmrCtr_LookupConfig(TIMER_ID_1);
	//inizializzo l'oggetto che gestisce il timer1
	XTmrCtr_CfgInitialize(&timerControl_1, timerConfig_1, TIMER_BASE_1);
	//inizializzo componente hardware
	XTmrCtr_InitHw(&timerControl_1);
	//inizializzo timer1
	XTmrCtr_Initialize(&timerControl_1, TIMER_ID_1);
	//imposto la configurazione del timer1
	//XTC_DOWN_COUNT_OPTION -> abilita il conto alla rovescia
	//XTC_INT_MODE_OPTION -> abilita l'interrupt del timer1
	//XTC_AUTO_RELOAD_OPTION -> quando il timer1 completa si auto ricarica
	XTmrCtr_SetOptions(
			&timerControl_1,TmrCtrNumber,
			XTC_DOWN_COUNT_OPTION | XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION
	);
	//imposta il valore che deve esser ricaricato ogni volta che il timer1 si riavvia
	//oppure quando esso si avvia per la prima volta
	XTmrCtr_SetResetValue(&timerControl_1, TmrCtrNumber, RESET_VALUE);
	//avvia il timer1
	XTmrCtr_Start(&timerControl_1, TmrCtrNumber);
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

		// Se la durata dello spazio è maggiore della soglia, il bit è '1', altrimenti è '0'.
		// La soglia è calcolata come la media tra la durata di uno '0' e di un '1'.
		u8 bit_value = !((us_from_start - time_start) < ((NEC_BIT_ONE_END_US + NEC_BIT_ZERO_END_US)/2.0));
		message = (message << 1) | bit_value;
	}

	return message; // ritorna messaggio a 32 bit
}
void manage_command(NECMessage message){
	//funzione che gestisce i comandi ricevuti
	switch((button_value_t) message.command) {   // controlla comando ricevuto
		//Se è stato premuto il tasto "Volume Su"/2
	    case VOL_UP: // Aumento PWMA
	    	if (dir.pwmA < 256) dir.pwmA += 15;
	        print("aumento di velocità A");
	        break;
	    case TWO: // Aumento PWMB
	        if (dir.pwmB < 256) dir.pwmB += 15;
	        print("aumento di velocità B");
	        break;
	    // Se è stato premuto il tasto "Volume giù"/8
	    case VOL_DOWN: // Diminuzione PWMA
	        if (dir.pwmA > 0) dir.pwmA -= 15;
	        print("diminuzione di velocità A");
	        break;
	    case EIGHT: // Diminuzione PWMB
	        if (dir.pwmB > 0) dir.pwmB -= 15;
	        print("diminuzione di velocità B");
	        break;
	    // Se è stato premuto il tasto "freccia a destra"/6
	    case NEXT: // Rotazione Oraria (CW) A
	        dir.dirA = MOTOR_CW;
	        print("rotazione oraria motore A");
	        break;
	    case SIX: // Rotazione Oraria (CW) B
	        dir.dirB = MOTOR_CW;
	        print("rotazione oraria motore B");
	        break;
	    // Se è stato premuto il tasto "freccia a sinistra"/4
	    case PREV: // Rotazione Antioraria (CCW) A
	        dir.dirA = MOTOR_CCW;
	        print("rotazione antioraria motore A");
	        break;
	    case FOUR: // Rotazione Antioraria (CCW) B
	        dir.dirB = MOTOR_CCW;
	        print("rotazione antioraria motore B");
	        break;
	    // Se è stato premuto il tasto "pausa"/5
	    case PAUSE: // Freno rapido (Short brake) A
	        dir.dirA = MOTOR_BRAKE;
	        print("short brake motore A");
	        break;
	    case FIVE: // Freno rapido (Short brake) B
	        dir.dirB = MOTOR_BRAKE;
	        print("short brake motore B");
	        break;
	    // Se è stato premuto il tasto "on"
	    case POWER_ON: // Stop A
	        dir.dirA = MOTOR_STOP;
	        dir.pwmA = 0;
	        print("stop motore A");
	        break;
	    case ONE: // Stop B
	        dir.dirB = MOTOR_STOP;
	        dir.pwmB = 0;
	        print("stop motore B");
	        break;
		default:
			print("comando non riconosciuto");
	}
	/* Dopo aver aggiornato le variabili di stato (dir.dirA, dir.dirB, dir.pwmA, dir.pwmB), chiama la funzione
		 che si occupa di tradurre questo stato in un output visivo sui LED.*/
	set_led_state(dir.dirA, dir.dirB);
	print("\n\r");
}
