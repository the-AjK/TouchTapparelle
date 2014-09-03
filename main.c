/************************************************************************************************\
 
    TouchTapparelle v1.0 - PIC12F1840
    www.ajk.altervista.com
 
    Copyright (c) 2012, Alberto Garbui aka AjK
    All rights reserved.
 
    Redistribution and use in source and binary forms, with or without modification, 
    are permitted provided that the following conditions are met:
 
    -Redistributions of source code must retain the above copyright notice, this list 
     of conditions and the following disclaimer.
    -Redistributions in binary form must reproduce the above copyright notice, this list 
     of conditions and the following disclaimer in the documentation and/or other 
     materials provided with the distribution.
    -Neither the name of the AjK Elettronica Digitale nor the names of its contributors may be 
     used to endorse or promote products derived from this software without specific prior 
     written permission.
 
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
    SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
\************************************************************************************************/
 #include "mTouch.h"                            // Required Include

//definizione connessioni
            
//LEDS UP/DOWN (singolo pin)
#define LED_UP();		TRISA5=0;LATA5=0;
#define LED_DOWN();		TRISA5=0;LATA5=1;
#define LED_TOGGLE();	TRISA5=0;LATA5=!LATA5;
#define LEDS_OFF();		LATA5=0;RA5=0;TRISA5=1;

//RELE' UP/DOWN
#define SWITCH_UP();	LATA0=1;
#define	SWITCH_DOWN();	LATA0=0;

//RELE' MOTORE ON/OFF
#define	MOTOR_ON();		LATA1=1;
#define MOTOR_OFF();	LATA1=0;

#define TOUCHDELAY		1000	//tempo permanenza touch per attivare il motore (ms)
#define	TIMEOUTMOTORE	17500	//tempo attivazione motore per una corsa intera (ms)

//definizioni non modificabili
#define TOUCH_UP 0
#define TOUCH_DOWN 1
#define OFF	2
#define SWITCHDELAY	500	//intervallo tra switch ed attivazione motore (ms) (necessario!)
#define MAXDELAY	65000 //48ms * 65535 =circa 52minuti
#define INTERRUPTPERIOD	50	//ms di periodo interrupt

// CONFIGURATION SETTINGS
__CONFIG(FOSC_INTOSC & WDTE_OFF & PWRTE_OFF & MCLRE_OFF & CP_OFF & CPD_OFF & BOREN_OFF & CLKOUTEN_OFF & IESO_OFF & FCMEN_OFF);
__CONFIG(WRT_OFF & PLLEN_ON & STVREN_OFF & BORV_25 & LVP_OFF);

//variabili globali
unsigned int tempoDelay;
unsigned char statoTapparella;
unsigned char timerMotore=0;		//flag avvio conteggio motore
unsigned int tempoMotore=0;			//conteggio tempo motore

// PROTOTIPI
void System_Init(void);
void interrupt ISR(void);
void setLed(unsigned char touch);
void setSwitch(unsigned char touch);
void waitMTOUCH_RELEASED(unsigned char touch);
void setupTimer1(void);
void delay_ms(unsigned int ms);
void checkTouch(unsigned char touch);
unsigned char checkPresenza(unsigned char touch, unsigned int ms);

void main(void)
{

	System_Init();          //init porte/oscillatore/interrupt
    mTouch_Init();          //init mTouch
    setupTimer1();			//avvio conteggio tempo con timer1 ed interrupts

	setLed(TOUCH_UP);		//accendo led up
	unsigned int i,j;
	for(i=500;i>50;i-=50)	//animazione iniziale
	{
		for(j=0;j<3;j++)
		{LED_TOGGLE();		
		delay_ms(i);}
	}
	for(i=0;i<20;i++)
	{
		LED_TOGGLE();		
		delay_ms(100);
	}
	LEDS_OFF();				//spengo i led
	delay_ms(1000);
	
    while(1)				//ciclo infinito principale
    {  
		if(mTouch_isDataReady())   		
        {
			mTouch_Service();       
			checkTouch(TOUCH_UP);		//controllo entrambi i sensori 
			checkTouch(TOUCH_DOWN);		//UP/DOWN
		}
    }//end while(1) 
}//end main()


void checkTouch(unsigned char touch)
{
	if((mTouch_GetButtonState(touch) == MTOUCH_PRESSED))
	{ 
		setLed(touch);		//se rilevo la prensenza del touch, attivo il led corrispondente

		if(checkPresenza(touch,TOUCHDELAY))					//se rilevo una pressione prolungata
		{
			MOTOR_OFF();									//mi assicuro che il motore sia spento
			delay_ms(SWITCHDELAY);							//delay per scaricare tensioni residue
			setSwitch(touch);								//imposto lo switch UP/DOWN
			delay_ms(200);
			MOTOR_ON();										//attivo il motore
			tempoMotore=0;									//azzero il tempo motore
			timerMotore=1;									//avvio il conteggio del tempo
			
			waitMTOUCH_RELEASED(touch);						//aspetto il rilascio

			//ora aspetto la fine del timeoutMotore oppure una pressione per lo stop
			while(1)
			{
				if(tempoMotore>(TIMEOUTMOTORE/INTERRUPTPERIOD))	//se supero il timeout
				{	
					MOTOR_OFF();								//spengo il motore
					timerMotore=0;								//fermo il conteggio del tempo
					break;										//esco
				}
				if(mTouch_isDataReady())  
        		{
					mTouch_Service();  						//se rilevo un touch qualsiasi esco     
					if((mTouch_GetButtonState(TOUCH_UP) == MTOUCH_PRESSED))
					{
						//setLed(TOUCH_UP);					//accendo il led UP		
						MOTOR_OFF();						//spengo il motore
						waitMTOUCH_RELEASED(TOUCH_UP);		//aspetto il rilascio
						break;
					}
					if((mTouch_GetButtonState(TOUCH_DOWN) == MTOUCH_PRESSED))
					{
						//setLed(TOUCH_DOWN);					//accendo il led DOWN		
						MOTOR_OFF();						//spengo il motore
						waitMTOUCH_RELEASED(TOUCH_DOWN);	//aspetto il rilascio
						break;
					}
				}
			}
			delay_ms(SWITCHDELAY);							//delay per scaricare tensioni residue
			setSwitch(TOUCH_DOWN);							//imposto lo switch DOWN (relè a riposo)
			LEDS_OFF();										//spengo i led dopo un'eventuale animazione
		}else{
			LEDS_OFF();
		}
		delay_ms(1000);										//aspetto 1sec					
	}else{ 
		LEDS_OFF();				//se non rilevo la presenza spengo i led
	}
}

void waitMTOUCH_RELEASED(unsigned char touch)
{
	//per sicurezza inserisco un timeout di 30sec
	unsigned int const timeout=30000/INTERRUPTPERIOD;	 
	tempoDelay=0;									//inizio il conteggio del tempo per il timeout
	while(1)										//aspetto il rilascio
	{
		if(tempoDelay>timeout)break;				//esco con timeout
		if(mTouch_isDataReady())  
    	{
			mTouch_Service();       
			if((mTouch_GetButtonState(touch) != MTOUCH_PRESSED))break;
		}
	}
	delay_ms(500);									//delay antirimbalzo dopo il rilascio
}

void setLed(unsigned char touch)
{
	switch(touch)
	{
		case TOUCH_UP:
			LED_UP();
			break;
		case TOUCH_DOWN:
			LED_DOWN();
			break;
		default:
			LEDS_OFF();
			break;
	}
}

void setSwitch(unsigned char touch)
{
	if(touch==TOUCH_UP)
	{
		SWITCH_UP();
	}else{
		SWITCH_DOWN();
	}
}


unsigned char checkPresenza(unsigned char touch, unsigned int ms)
{
	unsigned int timeout=ms/INTERRUPTPERIOD;
	if(timeout>MAXDELAY)timeout=MAXDELAY;
	tempoDelay=0;
	while(tempoDelay<timeout)
	{
		if(mTouch_isDataReady())   
        {
			mTouch_Service();       
			if((mTouch_GetButtonState(touch) != MTOUCH_PRESSED))break;
		}
	}
	if(tempoDelay>=timeout)return 1;
	else return 0;
}

void delay_ms(unsigned int ms)
{
	unsigned int timeout=ms/INTERRUPTPERIOD;
	if(timeout>MAXDELAY)timeout=MAXDELAY;
	tempoDelay=0;
	while(tempoDelay<timeout);
}

void setupTimer1() {  
	TMR0IE = 1;	
	TMR1IE = 1;      		// abilito interrupt overflow timer1
	TMR1IF = 0;      		// resetto il flag di interrupt
	TMR1H = 60;    			// set high byte
	TMR1L = 176;    		// set low byte  (overflow ogni 50ms) 
	T1GCON = 0;      		// disabilito tutte le funzioni gate sul timer1
	T1CON = 0b00110001; 	// fosc/4, prescale=8, no LP, timer abilitato
	INTCON = 0b11000000;   	//abilito interrupt globali e periferici
	TMR1IF = 0;      		// resetto il flag di interrupt
}

void interrupt ISR(void)
{
    SAVE_STATE();                       // mTouch Framework-supplied general ISR save state macro. 
                                        // Not required, but convenient. 
	if(TMR1IF==1)
	{
		TMR1ON=0;									//fermo il timer1
		if(tempoDelay<MAXDELAY)tempoDelay++;		//incremento il tempo se posso
		if(tempoMotore<MAXDELAY && timerMotore)
			tempoMotore++;							//se il flag è attivo incremento il tempoMotore
		setupTimer1();								//resetto il timer1 ed il flag di interrupt
	}

	RESTORE_STATE();                    // mTouch Framework-supplied general ISR restore state macro. 
}

void System_Init() 
{
    OSCCON  = 0b11110000;       // 32 MHz Fosc w/ PLLEN_ON (config bit)  
    
    // mTouch sensor pins impostati come uscite digitali
    ANSELA  = 0b00000000;
    LATA    = 0b00000000;
    TRISA   = 0b00000000;
	WPUA	= 0;			//pull up disabilitate

	LEDS_OFF();				//led spenti
	MOTOR_OFF();			//relè motore off
	SWITCH_DOWN();			//relè switch off
    
    //la libreria Mtouch richiede l'utilizzo del timer0 ad 8bit
    OPTION_REG  = 0b10000000;   // TMR0 Prescaler  = 1:2
}
