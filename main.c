// Procyon includes
#include <spi.h>

// avrlibc includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "descriptors.h"

#define USB_LED_OFF 0
#define USB_LED_ON  1
#define USB_DATA_OUT 2
#define NOTE_C2 36
#define NOTE_C4 60  // middle C
#define SR_COUNT 2  // number of 74hc165 shift registers attached to SPI
#define HIST_LEN 2  // HIST_LEN samples are used from history for debouncing

// circular buffer where we keep previous samples for debouncing
// old samples are constantly overwriten with new ones, there is no need to
// keep it ordered
uchar history[HIST_LEN][SR_COUNT];
uchar history_idx;
// here we store the tmp result of filtering loop
uchar filtered[SR_COUNT];
// here we store previous states of (debounced) keys
uchar prev_state[SR_COUNT];


int main() {
    uchar i, j, bitpos,  sr_idx, iii, channel;
    uchar midiMsg[8];
    uint16_t cnt = 0, io_number, note;

    // pin PB0 - debug diode
    sbi(PORTB, 0);  // PB0 hi
    sbi(DDRB, 0);   // PB0 output

    // pin PB1 controlls PL(active low) strobing of parallel load of SRs
    sbi(PORTB, 1);  // PB1 hi
    sbi(DDRB, 1);  // PB1 output

    usbInit();
    spiInit();
    wdt_enable(WDTO_1S); // enable 1s watchdog timer
        
    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();
    sei(); // Enable interrupts after re-enumeration

    while(1) {
        wdt_reset();    // keep the watchdog happy
        usbPoll();      // V-USB housekeeping

        // strobe PL (parallel load)
        cbi(PORTB, 1);
        _delay_us(1);
        sbi(PORTB, 1);

        // shift data out of SRs into uC
        for (sr_idx=0; sr_idx < SR_COUNT; sr_idx++){
            i = spiTransferByte(0x00);
            history[history_idx][sr_idx] = i;
        }

        // history_idx increment & modulo
        history_idx = (history_idx + 1 == HIST_LEN ? 0 : history_idx + 1);

        // debounce keys
        for (sr_idx=0; sr_idx < SR_COUNT; ++sr_idx){

            // uchar mask - the heart of debouncing algorithm
            // this byte will be ANDed with all the bytes form history, for
            // given SR. There are three cases:
            //  There is some zero bit in history 
            //      -> the corresponding bit in mask will be zero
            //         (there are still some glitches in history)
            //  All bits are "1" in history
            //      -> the corresponding bit in mask will be one
            //         (switch contact is now stable for some time)
            //  All bits are "0" in history
            //      -> ...
            uchar mask = 0xff;
            for (j=0; j < HIST_LEN; ++j){
                mask &= history[j][sr_idx];
            }
            // mask now contains debounced input - save it
            filtered[sr_idx] = mask;
            // do diff with previous state to find changes
            mask ^= prev_state[sr_idx];
            
            if (mask){
                // locate changes and send them directly to USB host
                for (bitpos=0; bitpos < 8; bitpos++){
                    if (mask & (0x01 << bitpos)){
                        io_number=sr_idx*8+bitpos;
                        // decode channel and note code. Layout:
                        //                  hc165   hc165   hc165   hc165
                        // PC <=  DINX4-1   ped     ped     ped     ped     ...
                        //    <=  DINX4-2   I.      I.      I.      I.      ...
                        //    <=  DINX4-3   I.      I.      I.      II.     ...
                        //    <=  DINX4-4   II.     II.     II.     II.     ...
                        //    <=  DINX4-5   II.     II.     stops   stops
                        //
                        // it should map to these channels and notes
                        //                  |8notes |8notes |8notes |8notes|
                        //                  0/36..........................65
                        //                  1/36..........................65
                        //                  1/66.................91 2/36..43
                        //                  2/44..........................75
                        //                  2/76.........91 3/1...........16
                        channel=0;
                        note=NOTE_C2+io_number;
                        // we are already done for pedal inputs
                        if (io_number >= 32 && io_number < 88){
                            // I. man.
                            channel=1;
                            note-=32;
                        } else if (io_number >= 88 && io_number < 144){
                            // II. man.
                            channel=2;
                            note-=88;
                        } else if (io_number >= 144 && io_number < 160){
                            // stops
                            channel=3;
                            note=io_number-143;
                        } else {
                            // error - middle C on channel 4
                            channel=4;
                            note=NOTE_C4;
                        }

                        iii=0;
                        if (prev_state[sr_idx] & (0x01 << bitpos)){
                            // keypress 
                            midiMsg[iii++] = 0x09;  // USBMIDI note on/off
                            midiMsg[iii++] = 0x90+channel;  // MIDI note on/off
                            midiMsg[iii++] = note;
                            midiMsg[iii++] = 0x7f;  // velocity
                        }else{
                            // release
                            midiMsg[iii++] = 0x08;
                            midiMsg[iii++] = 0x80+channel;
                            midiMsg[iii++] = note;
                            midiMsg[iii++] = 0x00;
                        }
                        
                        if (iii){
                            while (!usbInterruptIsReady()){
                                // wait until previous transmission is done
                                usbPoll();      // do not forget this
                                wdt_reset();    // do not forget this
                            }
                            
                            usbSetInterrupt(midiMsg, iii); // send it to host
                            iii = 0;
                            PORTB^= 0x01;

                        }
                    }
                }
            }
        }
        
        for (sr_idx=0; sr_idx < SR_COUNT; ++sr_idx){
            // prepare variables for the next round
            prev_state[sr_idx] = filtered[sr_idx];
        }


        // debug timer
        if (++cnt == 2000){
            PORTB ^= 0x01;  // toggle debug LED
            cnt = 0;
        }
    }
        
    return 0;
}


// this gets called when host wants to read device descriptors
uchar usbFunctionDescriptor(usbRequest_t * rq)
{
	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (usbMsgPtr_t) deviceDescrMIDI;
		return sizeof(deviceDescrMIDI);
	} else {		/* must be config descriptor */
		usbMsgPtr = (usbMsgPtr_t) configDescrMIDI;
		return sizeof(configDescrMIDI);
	}
}

// this gets called when custom control message is received
USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *)data;
        
    switch(rq->bRequest) { // custom command is in the bRequest field
    // these commands can be invoked from commandline with ./test (usbtest.c)
    case USB_LED_ON:
        PORTB |= 0x01; // turn debug LED on
        return 0;
    case USB_LED_OFF: 
        PORTB &= ~0x01; // turn debug LED off
        return 0;
    case USB_DATA_OUT:
        // dump key states to host
        usbMsgPtr = (usbMsgPtr_t) filtered;
        return SR_COUNT;
    }

    return 0;
}

