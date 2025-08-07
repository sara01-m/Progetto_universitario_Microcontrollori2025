#include <stdio.h> 

#include "platform.h" 

#include "xil_printf.h" 

#include "xil_io.h" 

#include "xuartlite_l.h" 

  

#define UART (u8)2 

#define SWITCHES (u8)1 

#define BUTTONS (u8)0 

  

u32 my_XUartLite_RecvByte(UINTPTR BaseAddress) 

{ 

// insert your implementation of the non-blocking read here; 

} 

  

void update_leds(u32 data, u8 mode){ 

// insert your implementation of the led update here; 

// mode indicates which input you are considering for the update; 

} 

  

} 

  

int main() 

{ 

    init_platform(); 

    u32 uart; 

    u32 buttons; 

    u32 switches; 

  

    print("Hello World\n\r"); 

  

    while(1){ 

    buttons=Xil_In32(0x40010000); 

    update_leds(buttons, BUTTONS); 

    switches=Xil_In32(0x40020000); 

    update_leds(switches, SWITCHES); 

    uart=my_XUartLite_RecvByte(0x40600000); 

    update_leds(uart, UART); 

    } 

  

    cleanup_platform(); 

    return 0; 

} 

  