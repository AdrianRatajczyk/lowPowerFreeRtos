/** \file main.cpp
 * \brief Main code block
 *
 * Main code block
 *
 * project: mg-stm32l_acquisition_supervisor; chip: STM32L152RB
 *
 * \author Mazeryt Freager
 * \author Adrian Ratajczyk
 * \date 2012-09-06
 */

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "stm32l152xc.h"
#include "stm32l1xx_hal.h"

//Third party software
#include "FreeRTOS.h"
#include "task.h"

#include "ff.h"

//configuration package
#include "power_management.h"
#include "command.h"
#include "config.h"
#include "error.h"
#include "bsp.h"

//hardware and peripherals
#include "hdr/hdr_rcc.h"
#include "hdr/hdr_gpio.h"

#include "gpio.h"
#include "spi.h"
#include "rcc.h"
#include "helper.h"
#include "usart.h"
#include "rtc.h"
#include "nvic.h"
#include "exti.h"
#include "st_rcc.h"
#include "pwr.h"
#include "stm32l1xx_gpio.h"

#include "mma955x_drv.h"

/* Private variables ---------------------------------------------------------*/


/* Private function prototypes -----------------------------------------------*/
static void _sysInit(void);
static void _setVCore(void);
static void _configureHSE(void);
static void _configureHSI(void);
static void _startPLL(void);

/*---------------------------------------------------------------------------------------------------------------------+
| local functions' declarations
+---------------------------------------------------------------------------------------------------------------------*/

static enum Error _dirHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer,
		size_t output_buffer_length);
static void _heartbeatTask(void *parameters);
static enum Error _initializeHeartbeatTask(void);
static enum Error _initializePowerSaveTask(void);
static enum Error _runtimestatsHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer,
		size_t output_buffer_length);
static enum Error _tasklistHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer,
		size_t output_buffer_length);

/*---------------------------------------------------------------------------------------------------------------------+
| local variables
+---------------------------------------------------------------------------------------------------------------------*/

static const struct CommandDefinition _dirCommandDefinition =
{
		"dir",						// command string
		1,							// maximum number of arguments
		_dirHandler,				// callback function
		"dir: prints contents of selected directory on SD card\r\n"
				"\t\tusage: dir [path]\r\n",	// string displayed by help function
};

static const struct CommandDefinition _runtimestatsCommandDefinition =
{
		"runtimestats",						// command string
		0,									// maximum number of arguments
		_runtimestatsHandler,				// callback function
		"runtimestats: lists all tasks with their runtime stats\r\n",	// string displayed by help function
};

static const struct CommandDefinition _tasklistCommandDefinition =
{
		"tasklist",							// command string
		0,									// maximum number of arguments
		_tasklistHandler,					// callback function
		"tasklist: lists all tasks with their info\r\n",	// string displayed by help function
};

static FATFS _fileSystem;

static uint8_t buffer [28];


void __rtcConfig(void);
void __RTC_Config();
void LED_Init();
//void vPortSetupTimerInterrupt( void );

/*---------------------------------------------------------------------------------------------------------------------+
| root task
+---------------------------------------------------------------------------------------------------------------------*/
int main(void)
{
	/* Configure the system clock and pll*/
	_sysInit();

	gpioInitialize();
	spiInitialize();

	LED_Init();

	enum Error error = usartInitialize();

	FRESULT fresult = f_mount(0, &_fileSystem);	// try mounting the filesystem on SD card
	ASSERT("f_mount()", fresult == FR_OK);

	error = _initializeHeartbeatTask();
	ASSERT("_initializeHeartbeatTask()", error == ERROR_NONE);

	gpioInitialize();

	/*	Special delay for debugging because after scheduler start
	 * 	it may be hard to catch core in run mode to connect to debugger
	 */
	for(unsigned long i=0; i<30; i++)
	{
		for(unsigned long j=0; j<1000000; j++){}
		LED1_bb ^= 1;
	}

	commandRegister(&_dirCommandDefinition);
	commandRegister(&_runtimestatsCommandDefinition);
	commandRegister(&_tasklistCommandDefinition);

	vTaskStartScheduler();

	while (1)
	{ }

}

// function to configure a timer to generate a periodic interrupt
//void vPortSetupTimerInterrupt( void )
//{
//	__RTC_Config();
//}

void GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN |
			RCC_AHBENR_GPIOEEN | RCC_AHBENR_GPIOHEN;

}

void LED_Init(void)
{
	gpioConfigurePin(LED_GPIO, LED_pin, GPIO_OUT_PP_40MHz);
	gpioConfigurePin(LED_GPIO, LED_pin_1, GPIO_OUT_PP_40MHz);
}

static void _sysInit(void)
{
	_setVCore();

	/* System Clock Configuration */
	_configureHSI();

	rccStartPll(RCC_PLL_INPUT_HSI, HSI_VALUE, FREQUENCY);

//	rtcConfig();
}

static void _setVCore(void)
{
	RCC_APB1ENR_PWREN_bb = 1;
	PWR->CR = (PWR->CR & (~PWR_CR_VOS)) | PWR_CR_VOS_0;	// set VCORE voltage range 1 (1.8V)
	while((PWR->CSR & PWR_CSR_VOSF) != 0);				// wait for regulator ready
}

static void _configureHSE(void)
{
	RCC_CR_HSEON_bb = 1;					// enable HSE oscillator
	while (RCC_CR_HSERDY_bb == 0);			// wait till READY
}

static void _configureHSI(void)
{
	RCC_CR_HSION_bb = 1;					// enable HSI oscillator
	while (RCC_CR_HSIRDY_bb == 0);			// wait till READY
}

static void _startPLL(void)
{
	if(CLOCK_SOURCE == USING_HSI){
		rccStartPll(RCC_PLL_INPUT_HSI, HSI_VALUE, FREQUENCY);
	}
	else if(CLOCK_SOURCE == USING_HSE) {
		rccStartPll(RCC_PLL_INPUT_HSE, HSE_VALUE, FREQUENCY);
	}

}

/*---------------------------------------------------------------------------------------------------------------------+
| local functions
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief Handler of 'dir' command.
 *
 * Handler of 'dir' command. Displays contents of selected directory.
 *
 * \param [in] arguments_array is the array with arguments, first elements is the command identification string - "dir"
 * \param [in] arguments_count is the number of arguments in arguments_array
 * \param [out] output_buffer is the pointer to output buffer
 * \param [in] output_buffer_length is the size of output buffer
 *
 * \return ERROR_NONE on success, otherwise an error code defined in the file error.h
 */

static enum Error _dirHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer, size_t output_buffer_length)
{
	static const char *root_path = "/";
	const char *path;

	if (arguments_count > 1)				// was argument passed to command?
		path = arguments_array[1];			// yes - use it as a path
	else
		path = root_path;					// no - use root path

	DIR dir;

	FRESULT fresult = f_opendir(&dir, path);
	enum Error error = errorConvert_FRESULT(fresult);

	if (error != ERROR_NONE)
		return error;

	FILINFO filinfo;

	while (error == ERROR_NONE)
	{
		fresult = f_readdir(&dir, &filinfo);
		error = errorConvert_FRESULT(fresult);

		if (error != ERROR_NONE)
			return error;

		size_t length = strlen(filinfo.fname);

		if (length == 0)					// end of directory?
			return error;

		 length += 2 + 1;					// include size of newline (2) and size of optional directory marker '/' (1)

		if (length > output_buffer_length - 1)	// will it fit into buffer (leave space for '\0')?
			return ERROR_BUFFER_OVERFLOW;

		output_buffer_length -= length;

		output_buffer = stpcpy(output_buffer, filinfo.fname);

		if (filinfo.fattrib & AM_DIR)		// is this a directory?
			output_buffer = stpcpy(output_buffer, "/");	// append trailing slash to indicate that

		output_buffer = stpcpy(output_buffer, "\r\n");
	}

	return error;
}

/**
 * \brief Heartbeat task that simulate real system behavior
 *
 * This is a main task in this device
 */

static void _heartbeatTask(void *parameters)
{

	uint32_t licznik;

	(void)parameters;						// suppress warning

	portTickType xLastHeartBeat;

	xLastHeartBeat = xTaskGetTickCount();



	//struct acc_t * accelerometer = new_acc();

	//accelerometer->acc_init(accelerometer);

	//rtcFastStart();

	//__RTC_Config();

	//__rtcConfig();

	for(;;){

		//accelerometer->acc_getAcceleration(accelerometer, 0);

		//accelerometer->acc_checkVersion(accelerometer, buffer);

		LED_bb ^= 1;

		//licznik = RTC_GetWakeUpCounter();

		vTaskDelay(500/portTICK_RATE_MS);	//Then go sleep
	}

}

/**
 * \brief Initialization of heartbeat task - setup GPIO, create task.
 *
 * Initialization of heartbeat task - setup GPIO, create task.
 *
 * \return ERROR_NONE if the task was successfully created and added to a ready list, otherwise an error code defined in
 * the file error.h
 */

static enum Error _initializeHeartbeatTask(void)
{
	portBASE_TYPE ret = xTaskCreate(_heartbeatTask, (signed char*)"heartbeat", HEARTBEAT_STACK_SIZE, NULL,
			HEARTBEAT_TASK_PRIORITY, NULL);

	return errorConvert_portBASE_TYPE(ret);
}

/**
 * \brief Initialization of power save task
 *
 * \return ERROR_NONE if the task was successfully created and added to a ready list, otherwise an error code defined in
 * the file error.h
 */

static enum Error _initializePowerSaveTask(void)
{
	portBASE_TYPE ret = xTaskCreate(system_idle_task, (signed char*)"system_idle_task", POWER_OFF_TASK_STACK_SIZE, NULL,
			POWER_OFF_TASK_PRIORITY, NULL);

	return errorConvert_portBASE_TYPE(ret);
}

/**
 * \brief Handler of 'runtimestats' command.
 *
 * Handler of 'runtimestats' command. Displays all tasks with their info.
 *
 * \param [out] output_buffer is the pointer to output buffer
 * \param [in] output_buffer_length is the size of output buffer
 *
 * \return ERROR_NONE on success, otherwise an error code defined in the file error.h
 */

static enum Error _runtimestatsHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer, size_t output_buffer_length)
{
	static const char header[] = "Task\t\tAbs Time\t% Time\r\n--------------------------------------";

	(void)arguments_array;					// suppress warning
	(void)arguments_count;					// suppress warning

	char *buffer = (char*)pvPortMalloc(1024);

	if (buffer == NULL)
		return ERROR_FreeRTOS_errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

	vTaskGetRunTimeStats((signed char*)buffer);

	size_t length = strlen(buffer);

	enum Error error = ERROR_NONE;

	if ((length + sizeof (header)) < output_buffer_length)
	{
		memcpy(output_buffer, header, sizeof(header) - 1);
		memcpy(output_buffer + sizeof(header) - 1, buffer, length + 1);
		error = ERROR_NONE;
	}
	else
		error = ERROR_BUFFER_OVERFLOW;

	vPortFree(buffer);

	return error;
}

/**
 * \brief Handler of 'tasklist' command.
 * Handler of 'tasklist' command. Displays all tasks with their info.
 *
 * \param [out] output_buffer is the pointer to output buffer
 * \param [in] output_buffer_length is the size of output buffer
 *
 * \return ERROR_NONE on success, otherwise an error code defined in the file error.h
 */

static enum Error _tasklistHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer, size_t output_buffer_length)
{
	static const char header[] = "Task\t\tState\tPri.\tStack\t##\r\n------------------------------------------";

	(void)arguments_array;					// suppress warning
	(void)arguments_count;					// suppress warning

	char *buffer = (char*)pvPortMalloc(1024);

	if (buffer == NULL)
		return ERROR_FreeRTOS_errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

	vTaskList((signed char*)buffer);

	size_t length = strlen(buffer);

	enum Error error = ERROR_NONE;

	if ((length + sizeof (header)) < output_buffer_length)
	{
		memcpy(output_buffer, header, sizeof(header) - 1);
		memcpy(output_buffer + sizeof(header) - 1, buffer, length + 1);
		error = ERROR_NONE;
	}
	else
		error = ERROR_BUFFER_OVERFLOW;

	vPortFree(buffer);

	return error;
}

/**
  * @brief  Configures the RTC Wakeup.
  * @param  None
  * @retval None
  */
void __rtcConfig(void)  //TODO: ta funkcja nie działa
{
  NVIC_InitTypeDef  NVIC_InitStructure;
  EXTI_InitTypeDef  EXTI_InitStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

  /*!< Allow access to RTC */
  PWR_RTCAccessCmd(ENABLE);

  /*!< Reset RTC Domain */
  RCC_RTCResetCmd(ENABLE);
  RCC_RTCResetCmd(DISABLE);

	/* The RTC Clock may varies due to LSI frequency dispersion. */
	/* Enable the LSI OSC */
	RCC_LSICmd(ENABLE);

	/* Wait till LSI is ready */
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET) {
	}

	/* Select the RTC Clock Source */
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);


  /*!< Enable the RTC Clock */
  RCC_RTCCLKCmd(ENABLE);

  /*!< Wait for RTC APB registers synchronisation */
  RTC_WaitForSynchro();

  /* EXTI configuration *******************************************************/
  EXTI_ClearITPendingBit(EXTI_Line20);
  EXTI_InitStructure.EXTI_Line = EXTI_Line20;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  /* Enable the RTC Wakeup Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = RTC_WKUP_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* RTC Wakeup Interrupt Generation: Clock Source: RTCDiv_16, Wakeup Time Base: 4s , RTCDiv_4 WTB: 1s*/
  RTC_WakeUpClockConfig(RTC_WakeUpClock_RTCCLK_Div4);
  RTC_SetWakeUpCounter(0x640); // 0.2s
  //(0x1FFF); // -> 8100 Div4 = 1s
  //(0x320) ; // -> 800 Div4 = 0.1s

  /* Enable the Wakeup Interrupt */
  RTC_ITConfig(RTC_IT_WUT, ENABLE);
}

void __RTC_Config()
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
	RTC_SetWakeUpCounter(18);

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

// RTC Wakeup through EXTI line interrupt
//extern "C" void RTC_WKUP_IRQHandler(void) __attribute((interrupt));
//void RTC_WKUP_IRQHandler(void)
//{
////	unsigned long ulDummy;
////
////	//	/* If using preemption, also force a context switch. */
////	#if configUSE_PREEMPTION == 1
////		*(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;
////	#endif
////
////	ulDummy = portSET_INTERRUPT_MASK_FROM_ISR();
////	{
////		vTaskIncrementTick();
////	}
////	portCLEAR_INTERRUPT_MASK_FROM_ISR( ulDummy );
//
//
//	if(RTC_GetITStatus(RTC_IT_WUT) != RESET)
//	{
//		RTC_ClearITPendingBit(RTC_IT_WUT);
//		EXTI_ClearITPendingBit(EXTI_Line20);
//	}
//}
