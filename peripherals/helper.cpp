/**
 * \file helper.cpp
 * \brief Helper functions
 *
 * Helper functions
 *
 * chip: STM32L1xx
 *
 * \author: Mazeryt Freager
 * \date 2014-07-20
 */

#include <stdint.h>

#include "stm32l152xc.h"

#include "config.h"

#include "hdr/hdr_rcc.h"

#include "rcc.h"
#include "helper.h"
#include "exti.h"
#include "rtc.h"
#include "nvic.h"
#include "st_rcc.h"
#include "pwr.h"

/*---------------------------------------------------------------------------------------------------------------------+
| global variables
+---------------------------------------------------------------------------------------------------------------------*/

volatile uint16_t tim6OverflowCount;

/*---------------------------------------------------------------------------------------------------------------------+
| global functions
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief Configures TIM6 for runtime stats.
 *
 * Configures TIM6 for runtime stats.
 */

void configureTimerForRuntimestats(void)
{
//	RCC_APB1ENR_TIM6EN_bb = 1;				// enable timer
//
//	DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM6_STOP;
//
//	TIM6->CNT = 0;							// clear the timer
//	TIM6->PSC = (rccGetCoreFrequency() / 10000) - 1;	// 100us resolution
//	TIM6->ARR = 0xFFFF;						// max autoreload
//	TIM6->DIER = TIM_DIER_UIE;				// enable update interrupt
//	TIM6->CR1 = TIM_CR1_CEN;				// enable ARR buffering, enable timer
//
//	NVIC_SetPriority(TIM6_IRQn, TIM6_IRQ_PRIORITY);
//	NVIC_EnableIRQ(TIM6_IRQn);
	//TODO: tutaj implementacja timera
}

// konfiguracja przerwania RTC_WKUP
void _RTC_WKUP_Config()
{
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;

	/* Enable the PWR clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	/* Allow access to RTC */
	PWR_RTCAccessCmd(ENABLE);

	/* Enable the LSI OSC */
	RCC_LSICmd(ENABLE);	// The RTC Clock may varies due to LSI frequency dispersion

	/* Wait till LSI is ready */
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
		;

	/* Select the RTC Clock Source */
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);

	/* Enable the RTC Clock */
	RCC_RTCCLKCmd(ENABLE);

	/* Wait for RTC APB registers synchronisation */
	RTC_WaitForSynchro();

	/* EXTI configuration *******************************************************/
	EXTI_ClearITPendingBit(EXTI_Line20);
	EXTI_InitStructure.EXTI_Line = EXTI_Line20;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	// Configuring RTC_WakeUp interrupt
	NVIC_InitStructure.NVIC_IRQChannel = RTC_WKUP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

//  /* RTC Wakeup Interrupt Generation: Clock Source: RTCDiv_16, Wakeup Time Base: 4s , RTCDiv_4 WTB: 1s*/
//    RTC_WakeUpClockConfig(RTC_WakeUpClock_RTCCLK_Div4);
//    RTC_SetWakeUpCounter(0x640); // 0.2s
//    //(0x1FFF); // -> 8100 Div4 = 1s
//    //(0x320) ; // -> 800 Div4 = 0.1s

	// RTC wake up counter disable
	RTC_WakeUpCmd(DISABLE);

	// RTC Wakeup Configuration
	RTC_WakeUpClockConfig(RTC_WakeUpClock_RTCCLK_Div16);

	// RTC Set WakeUp Counter
	RTC_SetWakeUpCounter(19);

	// Enabling RTC_WakeUp interrupt
	RTC_ITConfig(RTC_IT_WUT, ENABLE);

	// Disabling Alarm Flags
	RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
	RTC_AlarmCmd(RTC_Alarm_B, DISABLE);

	// RTC Enable the Wakeup Function
	RTC_WakeUpCmd(ENABLE);

	// Clear flags
	RTC_ClearITPendingBit(RTC_IT_WUT);
	EXTI_ClearITPendingBit(EXTI_Line20);
}

/*---------------------------------------------------------------------------------------------------------------------+
| ISRs
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief TIM6_IRQHandler
 *
 * TIM6_IRQHandler
 */

//extern "C" void TIM6_IRQHandler(void) __attribute__ ((interrupt));
//void TIM6_IRQHandler(void)
//{
//	tim6OverflowCount++;
//
//	TIM6->SR = 0;							// clear UIF which is only bit in this register
//}
