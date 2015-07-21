//***************** FORMULATRIX ********************
//******************* NT8 V3.1 *********************
//***** SOLENOID CLAMP FIRMWARE V1.0 (JULY 2015)****
//******** CREATED BY AFENDY - FEBRUARY 2014 *******
// The inputs will be received by P3.7/P3.6/P3.0/P2.4/P2.3/P2.2/P2.1/P2.0 ports
// The outputs will be transmitted by P4 port
// MSP430 control the output can be in pulse, holding and turn-off slowly and Time Out to protect the solenoid from overheat if there is a accident in Software/ WAMBo.

#include "msp430x22x4.h"
#define Heart 0x01
int StatusOld 	= 0x00, StatusNew = 0x00;
int StatusOFF 	= 0;
int TimePulser 	= 0;
int TimeOFF 	= 0;
int tPWM 		= 0;
int tPulser		= 0;
int t12v 		= 0;
int t6v			= 0;
int TimeOut		= 0;
const int TimeOutConst= 6900;	// ~30s = 6900 count

void initializePort()
{
	P1DIR |= Heart;

	P2DIR = 0x00;
	P2REN = 0xFF;
	P3DIR = 0x00;
	P3REN = 0xFF;

	P4DIR = 0xFF;
}

void initTimer()
{
	// Timer A ======================================================================================
	TACTL 	= TASSEL_2 + MC_1;
	TACCTL1 = OUTMOD_7;								// TACCR1 reset/set (PULSE, HOLDING)
	tPWM 	= 800;			//48v
	tPulser = 400;			//24v
	t12v	= 200;			//12v
	t6v		= 100;			//6v
	TACCR0 	= tPWM - 1;

	// Timer B ======================================================================================
	TBCCTL0 = CCIE;                           		// TBCCR0 interrupt enabled
	TBCTL 	= TBSSEL_2 + MC_2; 						// 16 MHz == 62.5 ns

	__bis_SR_register(GIE);
}
void blink(volatile unsigned long i)
{
	volatile unsigned long j,k;
	if((P1IN & 0x01) && Heart) P1OUT &= 0xFE;
		else P1OUT |= Heart;
	i *= 2;
	j = i;
	k = i;
	while(i != 0)
	{
		i--;
		while(j != 0)
			{
			j--;
			while(k != 0) k--;
			}
	}
}

int main(void)
{
	BCSCTL1 = CALBC1_16MHZ;			// Set DCO to 16MHz
	DCOCTL 	= CALDCO_16MHZ;
	WDTCTL	= WDTPW + WDTHOLD;		// Stop watch dog timer

	initializePort();
	initTimer();
	P4OUT = 0x00;
	//TimeOFF = t12v;

	while(1)
	{
		StatusNew = (P2IN & 0x1F) | ((P3IN & 0xC0) | ((P3IN & 0x01)<<5));
		//==================================================================================== Time out
		if((TimeOut == TimeOutConst) && (StatusNew != 0x00))
		{
			{
				P4OUT 	= 0x00;
				TBCCTL0	&= ~CCIE;                           				// TBCCR0 interrupt disabled
				StatusOld 	= 0x00;
			}
		}
		//======================================================================================= Blink
		else if((StatusNew == 0x00) && StatusOld == 0x00)					// Blink
		{
			blink(10000);
			P4OUT 		= 0x00;
			TBCCTL0 	&= ~CCIE;                           				// TBCCR0 interrupt disabled
			TimeOut		= 0;
		}
		//====================================================================================== Holding
		else if((StatusNew == StatusOld) && (StatusNew != 0x00))			// Holding
		{
			TACCR1 = t12v;
			if(TAR < TACCR1)
			{
				P4OUT 		= StatusNew;
				StatusOld 	= StatusNew;
			}
			else
			{
				P4OUT = 0x00;
				if((P1IN & 0x01) && Heart) P1OUT &= 0xFE;
			}
		}
		//======================================================================================= TimePulser
		else if((StatusNew != StatusOld) && (StatusNew != 0x00))		// TimePulser
		{
			TACCR1 = tPulser;
			if(TimePulser < 25000)											//20000 = ~1s, 25000 = ~1.25s
			{
				if(TAR < TACCR1)
				{
					P4OUT = StatusNew;
				}
				else
				{
					P4OUT 	= 0x00;
					TimePulser += 1;
				}
			}
			else
			{
				StatusOld = StatusNew;
				TimePulser = 0;
			}
			TBCCTL0 = CCIE;                           					// TBCCR0 interrupt enabled
		}
		//======================================================================================== Turn OFF
		else if((StatusNew == 0x00) && (StatusOld != 0x00))				//Turn OFF to 6v @500 mS
		{
			if(StatusOFF == 0)	// Initial for first turnOFF
				{
				TACCR1 	= t6v;	// PWM == 6v
				TimeOFF = 10000;	//~500mS ( 20000 = 1S )
				StatusOFF 	= 1;
				}

			TimeOFF	-= 1;
			if((TAR < TACCR1) && (TimeOFF != 0) && (StatusOFF == 1))
				{
					P4OUT = StatusOld;
				}
				else
				{
					P4OUT = 0x00;
					if((P1IN & 0x01) && Heart) P1OUT &= 0xFE;
				}
			if(TimeOFF == 0)
				{
					StatusOFF 	= 0;
					StatusNew 	= 0x00;
					StatusOld 	= 0x00;									// P4OUT = StatusNew = StatusOld
					P4OUT 		= 0x00;
				}
		}
		//================================================================================================
	}
}

// Timer B0 interrupt service routine
#pragma vector=TIMERB0_VECTOR
__interrupt void Timer_B (void)
{
	if(TimeOut < TimeOutConst) TimeOut++;
	else if(TimeOut >= TimeOutConst)
	{
		TimeOut	= TimeOutConst;
	}
}
