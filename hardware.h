#ifndef _HARDWARE_H_
#define _HARDWARE_H_

// ====================================
// HW definitions
// ------------------------------------
/**
PB0 = 
PB1 = 
PB2 = 
PB3 = ISP @ MOSI
PB4 = ISP @ MISO
PB5 = ISP @ SCK
PB6 = 
PB7 = 

PC0 = (ADC0)
PC1 = (ADC1)
PC2 = (ADC2)
PC3 = (ADC3)
PC4 = (ADC4)
PC5 = (ADC5)
PC6 = RST

PD0 = RxD
PD1 = TxD
PD2 = 
PD3 = 
PD4 = 
PD5 = 
PD6 = 
PD7 = 

     +----  ----+
PC6 =|	  --	|= PC5
PD0 =|		|= PC4
PD1 =|		|= PC3
PD2 =|		|= PC2
PD3 =|		|= PC1
PD4 =|		|= PC0
Vcc =|		|= GND
GND =|		|= Aref
PB6 =|		|= Avcc
PB7 =|		|= PB5
PD5 =|		|= PB4
PD6 =|		|= PB3
PD7 =|		|= PB2
PB0 =|		|= PB1
     +----------+

**/

#define	ZDI_O		PORTC
#define	ZDI_D		DDRC
#define	ZDI_I		PINC
#define	RST_BIT		0
#define	ZCL_BIT		1
#define	ZDA_BIT		2

#define	RST_O		ZDI_O, RST_BIT
#define	ZCL_O		ZDI_O, ZCL_BIT
#define	ZDA_O		ZDI_O, ZDA_BIT

#define	RST_D		ZDI_D, RST_BIT
#define	ZCL_D		ZDI_D, ZCL_BIT
#define	ZDA_D		ZDI_D, ZDA_BIT

#define	RST_I		ZDI_I, RST_BIT
#define	ZCL_I		ZDI_I, ZCL_BIT
#define	ZDA_I		ZDI_I, ZDA_BIT

#define	UART_OUT	PORTD
#define	UART_INP	PIND
#define	UART_RXD	0
#define	UART_TXD	1
#define	UART_RST	3
#define	UART_BOOT	4
#define	APP_RXD		2
#define	APP_TXD		5

//#define	RSTin		PIND, 3
//#define	BOOTin		PIND, 4
//#define	TXAin		PIND, 2
//#define	RXAout		PORTD, 5
#define	BOOTin		UART_INP, UART_BOOT
#define	RSTin		UART_INP, UART_RST
#define	TXD		UART_OUT, UART_TXD
#define	RXAout		UART_OUT, APP_TXD

// H = out
#define	PB_OUT		0b00000000
#define	PB_DIR		0b11111111
#define	PC_OUT		0b01000111
#define	PC_DIR		0b10111110
#define	PD_OUT		0b00111111
#define	PD_DIR		0b11100010

#ifdef _AVR_IOM328P_H_
// ATmega 328 compatibility
// UART
#define	UCSRA		UCSR0A
#define	UCSRB		UCSR0B
#define	UCSRC		UCSR0C
#define	UDRE		UDRE0
#define	UDR		UDR0
#define	RXC		RXC0
#define	UBRRL		UBRR0L
#define	UBRRH		UBRR0H
// Timer0
#define	TCCR0		TCCR0B
#define	TIFR		TIFR0

#endif


#endif
