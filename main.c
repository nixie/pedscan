// Procyon
#include <spi.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "descriptors.h"

#define USB_LED_OFF 0
#define USB_LED_ON  1
#define USB_DATA_OUT 2

#define SR_COUNT 2

uchar SRbuf[SR_COUNT];

uchar usbFunctionDescriptor(usbRequest_t * rq)
{
	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (uchar *) deviceDescrMIDI;
		return sizeof(deviceDescrMIDI);
	} else {		/* must be config descriptor */
		usbMsgPtr = (uchar*) configDescrMIDI;
		return sizeof(configDescrMIDI);
	}
}


/* this gets called when custom control message is received */
USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *)data; // cast data to correct type
        
    switch(rq->bRequest) { // custom command is in the bRequest field
    case USB_LED_ON:
        PORTB |= 2; // turn LED on
        return 0;
    case USB_LED_OFF: 
        PORTB &= ~2; // turn LED off
        return 0;
    case USB_DATA_OUT: // send data to PC
        usbMsgPtr = SRbuf;
        return sizeof(SRbuf);
    }

    return 0;
}


int main() {
    uchar i, sr_idx, iii;
    uchar midiMsg[8];
    uint16_t sent = 0;

    // pin PB0 controlls PL(active low) strobing of parallel load of SRs
    sbi(PORTB, 0);  // PB0 hi
    sbi(DDRB, 0);   // PB0 output

    // pin PB1 - debug diode
    sbi(PORTB, 1);  // PB1 hi
    sbi(DDRB, 1);  // PB1 output

    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    usbInit();
        
    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();

    sei(); // Enable interrupts after re-enumeration
     

    spiInit();

    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();

        // strobe PL
        cbi(PORTB, 0);
        _delay_us(1);
        sbi(PORTB, 0);

        // shift data out of SRs into uC
        for (sr_idx=0; sr_idx < SR_COUNT; sr_idx++){
            i = spiTransferByte(0x00);
            SRbuf[sr_idx] = i;
        }

        // debounce keys


        // send out changes
        sent++;

        if (sent == 10000){
            sent = 0;
            PORTB ^= 0x02;

            if (usbInterruptIsReady()){
                iii=0;
                midiMsg[iii++] = 0x09;  // press
                midiMsg[iii++] = 0x90;
                midiMsg[iii++] = 60;    // middle C
                midiMsg[iii++] = 0x7f;
                usbSetInterrupt(midiMsg, iii);
            }
        }

    }
        
    return 0;
}

