// STANDARD INCLUDES
#include <stdio.h>
#include <string.h>

// KERNEL INCLUDES
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

// HARDWARE SIMULATOR UTILITY FUNCTIONS  
#include "HW_access.h"

// SERIAL SIMULATOR CHANNEL TO USE 
#define COM_CH_0 (0)
#define COM_CH_1 (1)
#define COM_CH_2 (2)

// TASK PRIORITIES 
#define DISPLAY_PRI				( tskIDLE_PRIORITY + (UBaseType_t)1 )
#define	SEND_MESSAGE_PRI		( tskIDLE_PRIORITY + (UBaseType_t)2 )
#define	SEND_INFO_PRI			( tskIDLE_PRIORITY + (UBaseType_t)3 )
#define PROCESSING_PRI			( tskIDLE_PRIORITY + (UBaseType_t)4 )
#define RECEIVE_COMMAND_PRI		( tskIDLE_PRIORITY + (UBaseType_t)5 )
#define LED_PRI					( tskIDLE_PRIORITY + (UBaseType_t)6 )
#define RECEIVE_VALUE_PRI		( tskIDLE_PRIORITY + (UBaseType_t)7 )

// TASKS: FORWARD DECLARATIONS 
static void ReceiveValueTask(void* pvParameters);
static void LedBarTask(void* pvParameters);
static void ReceiveCommandTask(void* pvParameters);
static void ProcessingDataTask(void* pvParameters);
static void SendInfoTask(void* pvParameters);
static void SendOKTask(void* pvParameters);
static void SendImpossibleTask(void* pvParameters);
static void SendMessageTask(void* pvParameters);
static void DisplayTask(void* pvParameters);
static void TimerCallback(TimerHandle_t tmH);
extern void main_program(void);

// GLOBAL OS-HANDLES 
static SemaphoreHandle_t LED_BinarySemaphore;
static SemaphoreHandle_t Display_BinarySemaphore;
static SemaphoreHandle_t RXC_BinarySemaphore0;
static SemaphoreHandle_t RXC_BinarySemaphore1;
static SemaphoreHandle_t SendInfo_BinarySemaphore;
static SemaphoreHandle_t SendOK_BinarySemaphore;
static SemaphoreHandle_t SendLocked_BinarySemaphore;
static SemaphoreHandle_t SendMessage_BinarySemaphore;
static QueueHandle_t Queue1;
static QueueHandle_t Queue2;
static QueueHandle_t Queue3;

typedef float float_t;

#define BR_ODBIRAKA_BRZINE (10)
#define MAX_CHARACTERS (30)
#define ASCII_CR (13)
#define LENGTH_AUTOMATSKI (10)
#define LENGTH_MANUELNO (8)
#define LENGTH_BRZINA (6)
#define LENGTH_NIVO (4)
#define POSITION_FLAG_VALUE (16)
#define NUM_OF_USEFUL_LED (7)

static uint8_t prednjiLevi;
static uint8_t prednjiDesni;
static uint8_t zadnjiLevi;
static uint8_t zadnjiDesni;
static uint16_t trenutnaBrzina;
static uint16_t minBrzina = (uint16_t)50000;
static uint16_t maxBrzina = (uint16_t)0;
static uint8_t rezimRada = (uint8_t)0;
static uint8_t enableSendPC = (uint8_t)0;
static uint8_t enableImpossible = (uint8_t)0;
static uint8_t lockedWindows = (uint8_t)0;
static uint8_t tasterMinMaxBrzina = (uint8_t)0;

// INTERRUPTS //

static uint32_t prvProcessRXCInterrupt(void) {
	BaseType_t xHigherPTW = pdFALSE;
	if (get_RXC_status(0) != 0) {
		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &xHigherPTW) != pdPASS) {
			printf("Error RXC_SEM0_GIVE\n");
		}
	}
	if (get_RXC_status(1) != 0) {
		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &xHigherPTW) != pdPASS) {
			printf("Error RXC_SEM1_GIVE\n");
		}
	}
	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

static uint32_t OnLED_ChangeInterrupt() 
{
	BaseType_t xHigherPTW = pdFALSE;
	if (xSemaphoreGiveFromISR(LED_BinarySemaphore, &xHigherPTW) != pdPASS) {
		printf("Error LED_SEM_GIVE\n");
	}
	portYIELD_FROM_ISR((uint32_t)xHigherPTW);  
}

// MAIN - SYSTEM STARTUP POINT 
void main_program(void)
{
	// INITIALIZATION //
	if (init_serial_uplink(COM_CH_0) != 0) {
		printf("Error TX_COM0_INIT\n");
	}
	if (init_serial_downlink(COM_CH_0) != 0) {
		printf("Error RX_COM0_INIT\n");
	}
	if (init_serial_uplink(COM_CH_1) != 0) {
		printf("Error TX_COM1_INIT\n");
	}
	if (init_serial_downlink(COM_CH_1) != 0) {
		printf("Error RX_COM1_INIT\n");
	}
	if (init_serial_uplink(COM_CH_2) != 0) {
		printf("Error TX_COM2_INIT\n");
	}
	if (init_LED_comm() != 0) {
		printf("Error LED_INIT \n");
	}
	if (init_7seg_comm() != 0) {
		printf("Error DISPLAY_INIT \n");
	}

	if (set_LED_BAR((uint8_t)1, (uint8_t)0x00) != 0) {
		printf("Error SET_LED \n");
	}

	// SERIAL RECEPTION INTERRUPT HANDLER //
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);

	// SEMAPHORES //
	RXC_BinarySemaphore0 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore0 == NULL) {
		printf("Error RX_SEM0_CREATE\n");
	}
	
	RXC_BinarySemaphore1 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore1 == NULL) {
		printf("Error RX_SEM1_CREATE\n");
	}

	SendInfo_BinarySemaphore = xSemaphoreCreateBinary();
	if (SendInfo_BinarySemaphore == NULL) {
		printf("Error SEND_INFO_SEM_CREATE\n");
	}
	SendOK_BinarySemaphore = xSemaphoreCreateBinary();
	if (SendOK_BinarySemaphore == NULL) {
		printf("Error SEND_OK_SEM_CREATE\n");
	}
	SendLocked_BinarySemaphore = xSemaphoreCreateBinary();
	if (SendLocked_BinarySemaphore == NULL) {
		printf("Error SEND_LOCKED_SEM_CREATE\n");
	}
	SendMessage_BinarySemaphore = xSemaphoreCreateCounting(10,0);
	if (SendMessage_BinarySemaphore == NULL) {
		printf("Error SEND_LOCKED_SEM_CREATE\n");
	}
	LED_BinarySemaphore = xSemaphoreCreateBinary();
	if (LED_BinarySemaphore == NULL) {
		printf("Error LED_SEM_CREATE\n");
	}
	Display_BinarySemaphore = xSemaphoreCreateCounting(10, 0);
	if (Display_BinarySemaphore == NULL) {
		printf("Error DISPLAY_SEM_CREATE\n");
	}
	
	// QUEUES //
	Queue1 = xQueueCreate((uint8_t)10, (uint8_t)MAX_CHARACTERS * (uint8_t)sizeof(char));
	if (Queue1 == NULL) {
		printf("Error QUEUE1_CREATE\n");
	}

	Queue2 = xQueueCreate((uint8_t)10, (uint8_t)2 * (uint8_t)sizeof(uint8_t));
	if (Queue2 == NULL) {
		printf("Error QUEUE2_CREATE\n");
	}

	Queue3 = xQueueCreate((uint8_t)10, (uint8_t)sizeof(uint8_t));
	if (Queue3 == NULL) {
		printf("Error QUEUE3_CREATE\n");
	}
	
	// TIMERS //
	TimerHandle_t Timer200ms = xTimerCreate(
		NULL,
		pdMS_TO_TICKS(200),
		pdTRUE,
		NULL,
		TimerCallback);
	if (Timer200ms == NULL) {
		printf("Error TIMER_CREATE\n");
	}
	if (xTimerStart(Timer200ms, 0) != pdPASS) {
		printf("Error TIMER_START\n");
	}

	// TASKS //
	BaseType_t status;
	status = xTaskCreate(
		ReceiveValueTask,
		"prijem podataka sa senzora",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)RECEIVE_VALUE_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error RECEIVE_VALUE_TASK_CREATE\n");
	}
	
	status = xTaskCreate(
		ReceiveCommandTask,
		"prijem komande",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)RECEIVE_COMMAND_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error RECEIVE_COMMAND_TASK_CREATE\n");
	}

	status = xTaskCreate(
		ProcessingDataTask,
		"obrada podataka",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)PROCESSING_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error PROCESSING_DATA_TASK_CREATE\n");
	}

	status = xTaskCreate(
		SendInfoTask,
		"slanje trenutnog stanja ka PC-ju",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)SEND_INFO_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error SEND_INFO_TASK_CREATE\n");
	}

	status = xTaskCreate(
		SendOKTask,
		"slanje OK ka PC-ju",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)SEND_INFO_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error SEND_OK_TASK_CREATE\n");
	}

	status = xTaskCreate(
		LedBarTask,
		"LED bar",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)LED_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error LED_TASK_CREATE\n");
	}
	
	status = xTaskCreate(
		SendImpossibleTask,
		"send impossible task",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)SEND_MESSAGE_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error SENDIMPOSSIBLE_TASK_CREATE\n");
	}

	status = xTaskCreate(
		SendMessageTask,
		"send message task",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)SEND_MESSAGE_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error SENDMESSAGE_TASK_CREATE\n");
	}

	status = xTaskCreate(
		DisplayTask,
		"display task",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)DISPLAY_PRI,
		NULL);
	if (status != pdPASS) {
		printf("Error DISPLAY_TASK_CREATE\n");
	}

	vTaskStartScheduler();
	for (;;) {
		// da bi se ispostovalo pravilo misre
	}
}

static void TimerCallback(TimerHandle_t tmH) {
	static uint8_t cnt = 0;
	static uint8_t cnt2 = 0;
	cnt++;
	cnt2++;
	if (send_serial_character((uint8_t)COM_CH_0, (uint8_t)'S') != 0) {
		printf("Error SEND_TRIGGER\n");
	}
	vTaskDelay(pdMS_TO_TICKS(100));
	if (cnt == (uint8_t)25) {
		cnt = (uint8_t)0;
		if (xSemaphoreGive(SendInfo_BinarySemaphore) != pdTRUE) {
			printf("Error SENDINFO_SEM_GIVE\n");
		}
	}
	if (cnt2 == (uint8_t)5) {
		cnt2 = (uint8_t)0;
		if (xSemaphoreGive(Display_BinarySemaphore) != pdTRUE) {
			printf("Error DISPLAY_SEM_GIVE\n");
		}
	}
}

static void ReceiveValueTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t position = 0;
	uint8_t cc = 0;
	for (;;) {
		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {
			printf("Error RXC_SEM0_TAKE\n");
		}
		if (get_serial_character(COM_CH_0, &cc) != 0) {
			printf("Error GET_CHARACTER0\n");
		}
		if (cc == (uint8_t)ASCII_CR) {
			tmpString[position] = '\0'; 
			position++;
			tmpString[position] = 'i';
			position = 0;
			if (xQueueSend(Queue1, tmpString, 0) != pdTRUE) {
				printf("Error QUEUE1_SEND\n");
			}
		}
		else if (position < (uint8_t)MAX_CHARACTERS) {			
			tmpString[position] = (char)cc;
			position++;
		}
		else {
			//da bi se ispostovalo pravilo misre
		}
	}
}

static void ReceiveCommandTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t position = 0;
	uint8_t cc = 0;
	for (;;) {
		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
			printf("Error RXC_SEM1_TAKE\n");
		}
		if (get_serial_character(COM_CH_1, &cc) != 0) {
			printf("Error GET_CHARACTER1\n");
		}
		if (cc == (uint8_t)ASCII_CR) {
			tmpString[position] = '\0';
			position = 0;
			if (xQueueSend(Queue1, tmpString, 0) != pdTRUE) {
				printf("Error QUEUE1_SEND\n");
			}
		}
		else if (position < (uint8_t)MAX_CHARACTERS) {
			tmpString[position] = (char)cc;
			position++;
		}
		else {
			// da bi se ispostovalo pravilo misre
		}
	}
}

static void ProcessingDataTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t state = (uint8_t)0;
	static uint8_t setovanNivoKomandom = (uint8_t)0;
	static float_t srednjaBrzina = (float_t)0;
	static uint16_t granicaBrzine = (uint16_t)100;
	static uint8_t nivoProzora = (uint8_t)0;
	static uint16_t nizBrzina[BR_ODBIRAKA_BRZINE];
	memset(nizBrzina, 0, sizeof(uint16_t));
	static uint16_t sumaBrzina = (uint16_t)0;
	static uint8_t cnt = (uint8_t)0;
	static uint8_t oldDiodes = (uint8_t)0;
	uint8_t newDiodes = (uint8_t)0;
	static uint8_t diodes = (uint8_t)0;
	uint8_t changeDiodes = (uint8_t)0;
	static char old_diode0 = '0';
	static char old_diode1 = '0';
	uint8_t tmpString2[2];
	for (;;) {
		if (xQueueReceive(Queue1, tmpString, portMAX_DELAY) != pdTRUE) {
			printf("Error QUEUE1_RECEIVE\n");
		}
		if (memcmp(tmpString, "AUTOMATSKI", LENGTH_AUTOMATSKI) == 0) {
			state = (uint8_t)0;
		}
		if (memcmp(tmpString, "MANUELNO", LENGTH_MANUELNO) == 0) {
			state = (uint8_t)1;
		}
		if (memcmp(tmpString, "BRZINA", LENGTH_BRZINA) == 0) {
			for (uint8_t j = (uint8_t)LENGTH_BRZINA; j < ((uint8_t)LENGTH_BRZINA + 3U); j++) {
				for (uint8_t i = (uint8_t)0; i <= (uint8_t)9; i++) {
					if (tmpString[j] == ('0' + i)) {
						tmpString[j] = (char)i;
						break;
					}
				}
			}
			state = (uint8_t)2;
		}
		if (memcmp(tmpString, "NIVO", LENGTH_NIVO) == 0) {
			for (uint8_t j = (uint8_t)LENGTH_NIVO; j < ((uint8_t)LENGTH_NIVO + 3U); j++) {
				for (uint8_t i = (uint8_t)0; i <= (uint8_t)9; i++) {
					if (tmpString[j] == ('0' + i)) {
						tmpString[j] = (char)i;
						break;
					}
				}
			}
			state = (uint8_t)3;
		}
		if (tmpString[POSITION_FLAG_VALUE] == 'i') {
			state = (uint8_t)4;
		}
		if (tmpString[NUM_OF_USEFUL_LED + 1] == 'l') {
			state = (uint8_t)5;
		}
		if ((state == (uint8_t)0) || (state == (uint8_t)1)) {
			if (xSemaphoreGive(SendOK_BinarySemaphore) != pdTRUE) {
				printf("Error SENDOK_SEM_GIVE\n");
			}
		}
		switch (state) {
		case (uint8_t)0:
			rezimRada = (uint8_t)0;
			setovanNivoKomandom = (uint8_t)0;
			printf("Rezim je: %s\n", tmpString);
			break;
		case (uint8_t)1:
			rezimRada = (uint8_t)1;
			printf("Rezim je: %s\n", tmpString);
			break;
		case (uint8_t)2:
			granicaBrzine = (100U * (uint16_t)tmpString[6]) + (10U * (uint16_t)tmpString[7]) + (uint16_t)tmpString[8];
			printf("Nova granica brzine je: %d\n", granicaBrzine);
			break;
		case (uint8_t)3:
			if ((rezimRada == (uint8_t)0) && (srednjaBrzina <= (float_t)granicaBrzine)) {
				nivoProzora = (100U * (uint8_t)tmpString[4]) + (10U * (uint8_t)tmpString[5]) + (uint8_t)tmpString[6];
				if (nivoProzora <= (uint8_t)100) {
					printf("Nov nivo prozora je: %d\n", nivoProzora);
					prednjiLevi = nivoProzora;
					prednjiDesni = nivoProzora;
					zadnjiLevi = nivoProzora;
					zadnjiDesni = nivoProzora;
					setovanNivoKomandom = (uint8_t)1;
				}
			}
			break;
		case (uint8_t)4:
			for (uint8_t j = (uint8_t)0; j < ((uint8_t)POSITION_FLAG_VALUE - 1U); j++) {
				for (uint8_t i = (uint8_t)0; i <= (uint8_t)9; i++) {
					if (tmpString[j] == ('0' + i)) {
						tmpString[j] = (char)i;
						break;
					}
				}
			}
			if ((rezimRada == (uint8_t)0) && ((srednjaBrzina <= (float_t)granicaBrzine) && (setovanNivoKomandom != (uint8_t)1))) {
				uint8_t prozor1 = (100U * (uint8_t)tmpString[0]) + (10U * (uint8_t)tmpString[1]) + (uint8_t)tmpString[2];
				uint8_t prozor2 = (100U * (uint8_t)tmpString[3]) + (10U * (uint8_t)tmpString[4]) + (uint8_t)tmpString[5];
				uint8_t prozor3 = (100U * (uint8_t)tmpString[6]) + (10U * (uint8_t)tmpString[7]) + (uint8_t)tmpString[8];
				uint8_t prozor4 = (100U * (uint8_t)tmpString[9]) + (10U * (uint8_t)tmpString[10]) + (uint8_t)tmpString[11];
				if ((prozor1 <= (uint8_t)100) && (prozor2 <= (uint8_t)100) && (prozor3 <= (uint8_t)100) && (prozor4 <= (uint8_t)100)) {
					prednjiLevi = prozor1;
					prednjiDesni = prozor2;
					zadnjiLevi = prozor3;
					zadnjiDesni = prozor4;
				}
			}
			trenutnaBrzina = (100U * (uint16_t)tmpString[12]) + (10U * (uint16_t)tmpString[13]) + (uint16_t)tmpString[14];
			if (minBrzina > trenutnaBrzina) {
				minBrzina = trenutnaBrzina;
			}
			if (maxBrzina < trenutnaBrzina) {
				maxBrzina = trenutnaBrzina;
			}
			nizBrzina[cnt] = trenutnaBrzina;
			cnt++;
			if (cnt == (uint8_t)BR_ODBIRAKA_BRZINE) {
				cnt = (uint8_t)0;
			}
			for (uint8_t k = (uint8_t)0; k < (uint8_t)BR_ODBIRAKA_BRZINE; k++) {
				sumaBrzina = sumaBrzina + nizBrzina[k];
			}
			srednjaBrzina = (float_t)sumaBrzina / (float_t)BR_ODBIRAKA_BRZINE;
			sumaBrzina = (uint16_t)0;

			if ((rezimRada == (uint8_t)0) && (srednjaBrzina > (float_t)granicaBrzine)) {
				prednjiLevi = (uint8_t)100;
				prednjiDesni = (uint8_t)100;
				zadnjiLevi = (uint8_t)100;
				zadnjiDesni = (uint8_t)100;
				setovanNivoKomandom = (uint8_t)0;
			}
			break;
		case (uint8_t)5:
			if (xQueueReceive(Queue3, &newDiodes, portMAX_DELAY) != pdTRUE) {
				printf("Error QUEUE3_RECEIVE\n");
			}
			if (rezimRada == (uint8_t)1) {
				if (tmpString[4] == '1') {				//LOCKED
					lockedWindows = (uint8_t)1;
					diodes |= (uint8_t)0x03;
					zadnjiLevi = (uint8_t)100;
					zadnjiDesni = (uint8_t)100;
					if (((tmpString[0] == '0') && (old_diode0 == '1')) || ((tmpString[1] == '0') && (old_diode1 == '1'))) {
						if (xSemaphoreGive(SendLocked_BinarySemaphore) != pdTRUE) {
							printf("Error SENDLOCKED_SEM_GIVE\n");
						}
					}
				}
				else {
					lockedWindows = (uint8_t)0;
					if (tmpString[0] == '1') {				// ZD
						diodes |= (uint8_t)0x01;
						zadnjiDesni = (uint8_t)100;
					}
					else {
						diodes &= (uint8_t)~(1U);
						zadnjiDesni = (uint8_t)0;
					}
					if (tmpString[1] == '1') {				// ZL
						diodes |= (uint8_t)0x02;
						zadnjiLevi = (uint8_t)100;
					}
					else {
						diodes &= (uint8_t)~(2U);
						zadnjiLevi = (uint8_t)0;
					}
				}
				if (tmpString[2] == '1') {				// PD
					diodes |= (uint8_t)0x04;
					prednjiDesni = (uint8_t)100;
				}
				else {
					diodes &= (uint8_t)~(4U);
					prednjiDesni = (uint8_t)0;
				}
				if (tmpString[3] == '1') {				// PL
					diodes |= (uint8_t)0x08;
					prednjiLevi = (uint8_t)100;
				}
				else {
					diodes &= (uint8_t)~(8U);
					prednjiLevi = (uint8_t)0;
				}
				if (tmpString[5] == '1') {				//MIN
					tasterMinMaxBrzina = (uint8_t)0;
				}
				if (tmpString[6] == '1') {				//MAX
					tasterMinMaxBrzina = (uint8_t)1;
				}
				if(set_LED_BAR((uint8_t)1, diodes) != 0) {
					printf("Error SET_LED \n");
				}
				old_diode0 = tmpString[0];
				old_diode1 = tmpString[1];
				changeDiodes = oldDiodes ^ newDiodes;
				oldDiodes = newDiodes;
				tmpString2[0] = (uint8_t)13;
				tmpString2[1] = (uint8_t)13;
				for (uint8_t i = 0; i < (uint8_t)4; i++) {
					if ((changeDiodes & ((uint8_t)(1U << i))) != (uint8_t)0) {
						tmpString2[0] = i;
						if ((newDiodes & ((uint8_t)(1U << i))) != (uint8_t)0) {
							tmpString2[1] = (uint8_t)1;
						}
						else {
							tmpString2[1] = (uint8_t)0;
						}
					}
				}
				if (xQueueSend(Queue2, tmpString2, 0) != pdTRUE) {
					printf("Error QUEUE2_SEND\n");
				}
				if (xSemaphoreGive(SendMessage_BinarySemaphore) != pdTRUE) {
					printf("Error SENDMESSAGE_SEM_GIVE\n");
				}
			}
			break;
		default:
			// da bi se ispostovalo pravilo misre
			break;
		}
	}
}

static void SendInfoTask(void* pvParameters)
{
	static uint8_t tmpString[MAX_CHARACTERS];
	for (;;) {
		if (xSemaphoreTake(SendInfo_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Error SEND_INFO_SEM_TAKE\n");
		}
		tmpString[0] = prednjiLevi / 100U;
		tmpString[1] = (prednjiLevi % 100U) / 10U;
		tmpString[2] = prednjiLevi % 10U;
		tmpString[3] = prednjiDesni / 100U;
		tmpString[4] = (prednjiDesni % 100U) / 10U;
		tmpString[5] = prednjiDesni % 10U;
		tmpString[6] = zadnjiLevi / 100U;
		tmpString[7] = (zadnjiLevi % 100U) / 10U;
		tmpString[8] = zadnjiLevi % 10U;
		tmpString[9] = zadnjiDesni / 100U;
		tmpString[10] = (zadnjiDesni % 100U) / 10U;
		tmpString[11] = zadnjiDesni % 10U;
		tmpString[12] = (uint8_t)(trenutnaBrzina / 100U);
		tmpString[13] = (uint8_t)((trenutnaBrzina % 100U) / 10U);
		tmpString[14] = (uint8_t)(trenutnaBrzina % 10U);

		for (uint8_t i = (uint8_t)0; i < (uint8_t)15; i++) {
			if (tmpString[i] <= (uint8_t)9) {
				tmpString[i] = tmpString[i] + (uint8_t)48;
			}
		}
		enableSendPC = (uint8_t)1;
		for (uint8_t i = (uint8_t)0; i <= (uint8_t)2; i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, tmpString[i])) != (uint8_t)0) {
				printf("Error SEND_PL \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)' ')) != (uint8_t)0) {
			printf("Error SEND_SPACE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		for (uint8_t i = (uint8_t)3; i <= (uint8_t)5; i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, tmpString[i])) != (uint8_t)0) {
				printf("Error SEND_PD \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)' ')) != (uint8_t)0) {
			printf("Error SEND_SPACE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		for (uint8_t i = (uint8_t)6; i <= (uint8_t)8; i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, tmpString[i])) != (uint8_t)0) {
				printf("Error SEND_ZL \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)' ')) != (uint8_t)0) {
			printf("Error SEND_SPACE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		for (uint8_t i = (uint8_t)9; i <= (uint8_t)11; i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, tmpString[i])) != (uint8_t)0) {
				printf("Error SEND_ZD \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)' ')) != (uint8_t)0) {
			printf("Error SEND_SPACE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		for (uint8_t i = (uint8_t)12; i <= (uint8_t)14; i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, tmpString[i])) != (uint8_t)0) {
				printf("Error SEND_BRZINA \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)' ')) != (uint8_t)0) {
			printf("Error SEND_SPACE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		if (rezimRada == (uint8_t)0) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)'a')) != (uint8_t)0) {
				printf("Error SEND_A \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else if (rezimRada == (uint8_t)1) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)'m')) != (uint8_t)0) {
				printf("Error SEND_M \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else {
			// da bi se ispostovalo pravilo misre
		}
		if ((uint8_t)(send_serial_character((uint8_t)COM_CH_1, (uint8_t)'\n')) != (uint8_t)0) {
			printf("Error SEND_NL \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));
		enableSendPC = (uint8_t)0;
	}
}

static void SendOKTask(void* pvParameters)
{
	for (;;) {
		if (xSemaphoreTake(SendOK_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Error SEND_OK_SEM_TAKE\n");
		}
		while (enableSendPC != (uint8_t)0) {
			// da bi se ispostovalo pravilo misre
		}
		if (((uint8_t)send_serial_character((uint8_t)COM_CH_1, (uint8_t)'O')) != (uint8_t)0) {
			printf("Error SEND_O_FROM_OK \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));
		if (((uint8_t)send_serial_character((uint8_t)COM_CH_1, (uint8_t)'K')) != (uint8_t)0) {
			printf("Error SEND_K_FROM_OK \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));
		if (((uint8_t)send_serial_character((uint8_t)COM_CH_1, (uint8_t)'\n')) != (uint8_t)0) {
			printf("Error SEND_NEW_LINE \n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

static void LedBarTask(void* pvParameters)
{
	uint8_t ledTmp, cifraTmp, ledTmpQueue;
	char tmpString[NUM_OF_USEFUL_LED + 2];
	for (;;) {
		if (xSemaphoreTake(LED_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Error LED_SEM_TAKE \n");
		}
		if (get_LED_BAR((uint8_t)0, &ledTmp) != 0) {
			printf("Error GET_LED \n");
		}
		ledTmpQueue = ledTmp;
		for (uint8_t i = (uint8_t)0; i < (uint8_t)NUM_OF_USEFUL_LED; i++) {  
			cifraTmp = (ledTmp % (uint8_t)2) + (uint8_t)48;
			ledTmp = ledTmp / (uint8_t)2;
			tmpString[i] = (char)cifraTmp;
		}
		tmpString[NUM_OF_USEFUL_LED] = '\0';
		tmpString[NUM_OF_USEFUL_LED + 1] = 'l';
		if (xQueueSend(Queue1, tmpString, 0) != pdTRUE) {
			printf("Error QUEUE1_SEND\n");
		}
		if (xQueueSend(Queue3, &ledTmpQueue, 0) != pdTRUE) {
			printf("Error QUEUE3_SEND\n");
		}
	}
}

static void SendImpossibleTask(void* pvParameters) {
	const char stringImpossible[] = "impossible\n";
	for (;;) {
		if (xSemaphoreTake(SendLocked_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Error SEND_LOCKED_SEM_TAKE\n");
		}
		while (enableImpossible != (uint8_t)0) {
			// da bi se ispostovalo pravilo misre
		}
		enableImpossible = (uint8_t)1;
		for (uint8_t i = 0; i < (sizeof(stringImpossible) - 1U); i++) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)stringImpossible[i])) != (uint8_t)0) {
				printf("Error SEND_CHAR \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		enableImpossible = (uint8_t)0;
	}
}

static void SendMessageTask(void* pvParameters) {
	uint8_t tmpString[2];
	const char stringRise[] = "rise";
	const char stringFall[] = "fall";
	for (;;) {
		if (xSemaphoreTake(SendMessage_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
			printf("Error SEND_MESSAGE_SEM_TAKE\n");
		}
		if (xQueueReceive(Queue2, tmpString, portMAX_DELAY) != pdTRUE) {
			printf("Error QUEUE2_RECEIVE\n");
		}
		while (enableImpossible != (uint8_t)0) {
			// da bi se ispostovalo pravilo misre
		}
		enableImpossible = (uint8_t)1;
		if (tmpString[1] == (uint8_t)1) {
			for (uint8_t i = (uint8_t)0; i < sizeof(stringRise); i++) {
				if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)stringRise[i])) != (uint8_t)0) {
					printf("Error SEND_CHAR2 \n");
				}
				vTaskDelay(pdMS_TO_TICKS(100));
			}
		}
		else if (tmpString[1] == (uint8_t)0) {
			if ((lockedWindows == (uint8_t)0) || ((lockedWindows == (uint8_t)1) && ((tmpString[0] == (uint8_t)2) || (tmpString[0] == (uint8_t)3)))) {
				for (uint8_t i = (uint8_t)0; i < sizeof(stringFall); i++) {
					if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)stringFall[i])) != (uint8_t)0) {
						printf("Error SEND_CHAR2 \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}
			}
		}
		else {
			// da bi se ispostovalo pravilo misre
		}
		if ((tmpString[0] == (uint8_t)0) && ((lockedWindows == (uint8_t)0) || ((lockedWindows == (uint8_t)1) && (tmpString[1] != (uint8_t)0)))) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'Z')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'D')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'\n')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else if ((tmpString[0] == (uint8_t)1) && ((lockedWindows == (uint8_t)0) || ((lockedWindows == (uint8_t)1) && (tmpString[1] != (uint8_t)0)))) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'Z')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'L')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'\n')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else if (tmpString[0] == (uint8_t)2) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'P')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'D')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'\n')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else if (tmpString[0] == (uint8_t)3) {
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'P')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'L')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			if ((uint8_t)(send_serial_character((uint8_t)COM_CH_2, (uint8_t)'\n')) != (uint8_t)0) {
				printf("Error SEND_CHAR2 \n");
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		else {
			// da bi se ispostovalo pravilo misre
		}
		enableImpossible = (uint8_t)0;
	}
}

static void DisplayTask(void* pvParameters)
{
	// 7-SEG NUMBER DATABASE - ALL DEC DIGITS [ 0 1 2 3 4 5 6 7 8 9 ]
	const uint8_t character[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
	uint16_t tmpBroj, minBrzinaTmp, maxBrzinaTmp;
	uint8_t jT, dT, sT, jMAX, dMAX, sMAX, jMIN, dMIN, sMIN;
	for (;;) {
		if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) { 
			printf("Error Error DISPLAY_SEM_TAKE\n");
		}
		if ((uint8_t)select_7seg_digit(0) != (uint8_t)0) {
			printf("Error SELECT_DISPLAY0\n");
		}
		if ((uint8_t)set_7seg_digit(character[rezimRada]) != (uint8_t)0) {
			printf("Error SET_DISPLAY0 \n");
		}

		tmpBroj = trenutnaBrzina; 
		jT = (uint8_t)(tmpBroj % 10U);
		dT = (uint8_t)((tmpBroj / 10U) % 10U);
		sT = (uint8_t)(tmpBroj / 100U);
		if ((uint8_t)select_7seg_digit((uint8_t)3) != (uint8_t)0) {
			printf("Error SELECT_DISPLAY3\n");
		}
		if ((uint8_t)set_7seg_digit(character[jT]) != (uint8_t)0) {
			printf("Error SET_DISPLAY3 \n");
		}
		if ((uint8_t)select_7seg_digit((uint8_t)2) != (uint8_t)0) {
			printf("Error SELECT_DISPLAY2\n");
		}
		if ((uint8_t)set_7seg_digit(character[dT]) != (uint8_t)0) {
			printf("Error SET_DISPLAY2 \n");
		}
		if ((uint8_t)select_7seg_digit((uint8_t)1) != (uint8_t)0) {
			printf("Error SELECT_DISPLAY1\n");
		}
		if ((uint8_t)set_7seg_digit(character[sT]) != (uint8_t)0) {
			printf("Error SET_DISPLAY1 \n");
		}
		if (tasterMinMaxBrzina == (uint8_t)0) {
			minBrzinaTmp = minBrzina;
			jMIN = (uint8_t)(minBrzinaTmp % 10U);
			dMIN = (uint8_t)((minBrzinaTmp / 10U) % 10U);
			sMIN = (uint8_t)(minBrzinaTmp / 100U);
			if ((uint8_t)select_7seg_digit((uint8_t)6) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY3\n");
			}
			if ((uint8_t)set_7seg_digit(character[jMIN]) != (uint8_t)0) {
				printf("Error SET_DISPLAY3 \n");
			}
			if ((uint8_t)select_7seg_digit((uint8_t)5) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY2\n");
			}
			if ((uint8_t)set_7seg_digit(character[dMIN]) != (uint8_t)0) {
				printf("Error SET_DISPLAY2 \n");
			}
			if ((uint8_t)select_7seg_digit((uint8_t)4) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY1\n");
			}
			if ((uint8_t)set_7seg_digit(character[sMIN]) != (uint8_t)0) {
				printf("Error SET_DISPLAY1 \n");
			}
		}
		else {
			maxBrzinaTmp = maxBrzina;
			jMAX = (uint8_t)(maxBrzinaTmp % 10U);
			dMAX = (uint8_t)((maxBrzinaTmp / 10U) % 10U);
			sMAX = (uint8_t)(maxBrzinaTmp / 100U);
			if ((uint8_t)select_7seg_digit((uint8_t)6) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY3\n");
			}
			if ((uint8_t)set_7seg_digit(character[jMAX]) != (uint8_t)0) {
				printf("Error SET_DISPLAY3 \n");
			}
			if ((uint8_t)select_7seg_digit((uint8_t)5) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY2\n");
			}
			if ((uint8_t)set_7seg_digit(character[dMAX]) != (uint8_t)0) {
				printf("Error SET_DISPLAY2 \n");
			}
			if ((uint8_t)select_7seg_digit((uint8_t)4) != (uint8_t)0) {
				printf("Error SELECT_DISPLAY1\n");
			}
			if ((uint8_t)set_7seg_digit(character[sMAX]) != (uint8_t)0) {
				printf("Error SET_DISPLAY1 \n");
			}
		}


	}
}