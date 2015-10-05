#include "adc.h"
#include "config.h"
#include "gpio.h"
#include "stm32l152xc.h"

void ADC_Init()
{
	// enable clock for ADC
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	// enable clock for GPIOA
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

	// configuring GPIO
	gpioConfigurePin(ADC_GPIO, ADC_PIN, ADC_CONFIGURATION);

	// bank selection (bank A)
	ADC1->CR2 &= ~ADC_CR2_CFG;

	// channel selection (channel 1)
	ADC1->SQR5 |= ADC_SQR5_SQ1_0;
}

uint16_t ADC_SingleMeas()
{
	uint16_t value;

	// if ADC is enabled
	if(ADC1->SR & ADC_SR_ADONS)
	{
		ADC1->CR2 &= ~ADC_CR2_ADON;
		while(ADC1->SR & ADC_SR_ADONS);
	}

	// enabling ADC
	ADC1->CR2 |= ADC_CR2_ADON;

	// waiting for ADC enable
	while(!(ADC1->SR & ADC_SR_ADONS));

	// starting single convwersion
	ADC1->CR2 |= ADC_CR2_SWSTART;

	// waiting for conversion end
	while(!(ADC1->SR & ADC_SR_EOC));

	value = (uint16_t)(ADC1->DR & ADC_DR_DATA);

	return value;
}
