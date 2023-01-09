/*
===============================================================================
 Name        : TP_Final
 Description : Barrera para Autos.
===============================================================================
*/


/*
 *  Librerias a Incluir para el
 *  proyecto.
 *  Compilar con C Build -> Managed Linker Script -> Library -> Redlib(nohost-nf)
 */

#include <stdio.h>
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_systick.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_rtc.h"


/*
 *  Macros utiles
 */

// MACROS PARA LOS PINES
#define OUTPUT 			1
#define INPUT 			0
#define FALLING			1
#define RISING			0
#define UPPER			1
#define LOWER			0
#define PORT_ZERO		0
#define PORT_ONE		1
#define PORT_TWO 		2
#define PIN_18 			(uint32_t)(1<<18)
#define PIN_20 			(uint32_t)(1<<20)
#define PIN_21 			(uint32_t)(1<<21)
#define PIN_22 			(uint32_t)(1<<22)
#define PIN_23 			(uint32_t)(1<<23)
#define PIN_24 			(uint32_t)(1<<24)
#define PIN_25 			(uint32_t)(1<<25)
#define PIN_26 			(uint32_t)(1<<26)
#define PIN_28 			(uint32_t)(1<<28)
// MACROS CANALES
#define ADC_CHANNEL_0 		(uint8_t)0
#define MATCH_CHANNEL_0 	(uint8_t)0
#define CAPTURE_CHANNEL_0 	(uint8_t)0
// MACROS PARA EL TECLADO
#define KEY_COL 		4
#define KEY_ROW 		4
#define KEYBOARD_SIZE 	(KEY_COL * KEY_ROW)
#define CODE_SIZE		4
#define LETTER_A		10
#define LETTER_B		11
#define LETTER_C		12
#define LETTER_D		13
// MACROS PARA ADMINISTRACION
#define EMPLOYEES_NUM	10
// MACROS PARA EL MOTOR
#define BARRIER_OPEN 	1
#define BARRIER_CLOSE	0
// MACROS PARA DMA
#define DMA_CHANNEL_0		(uint32_t)0
#define ADC_DMA_RESULT(n)	((n>>4)&0xFFF)
// MACROS PARA EL SENSOR
#define DISTANCE_TO_WALL	40 // En centimetros

// Bit para leer el estado de interrupcion por transmit holding register empty (THRE)
#define THRE (1<<5)
// Caracteres de salto y retorno de linea
#define LINE_FEED 0x0A
#define CARRIAGE_RETURN 0x0D


/*
 *  Definicion de funciones
 */

// Funciones de configuracion
void configPin(void);
void configTimerLEDs(void);
// Funciones para el teclado
void bounceDelay(void);
uint8_t getKey(void);
// Funciones para el motor paso a paso
void configMotorPins(void);
void configMotorTimer(void);
void motorControl(uint8_t ctrl);
void motorOpen(void);
void motorClose(void);
void clearMotorPins(void);
// Funciones para el ADC
void configADCPins(void);
void configADC(void);
void checkBat(void);
void configDMA(void);
void ADCtoDMA(void);
// Funciones para el Sensor
void configSensorPins(void);
void EchoConfig(void);
void delay(uint32_t time);
void turnOnPin(uint8_t captureValue);
uint8_t stopTimer1(void);
void sensingBarrier(void);
// Funciones para Comunicacion Serie
void configUART(void);
void configUARTpins(void);
// Funciones Administrativas
void configPassword(uint8_t *code);
void registerEmployee(uint8_t *code);
char checkArea(uint8_t ref);
void enterEmployee(uint8_t *code);
void getLog(uint8_t *code);
uint8_t checkPassword(uint8_t *code);
void turnLED(uint8_t port, uint32_t pin);
// Funciones del RTC
void configRTC(void);
void RTC_AlarmHandler(void);
void RTC_IncreaseHandler(void);


/*
 * 	Estructura que representa a un empleado.
 * 	Sus campos:
 * 		1. codigo: Codigo de identificacion del empleado.
 * 		2. area: Area donde trabaja el empleado.
 * 		3. presentismo: Campo que representa el presentismo. Cuenta los dias de ausente.
 * 		4. ausente: Campo que representa si el empleado estuvo ausente en caso de ser TRUE.
 */
typedef struct Employee {
	uint8_t codigo[4];
	char area;
	uint8_t presentismo;
	uint8_t ausente;
} Employee;

/*
 * 	Estructura usada para redefinir __FILE
 * 	de la libreria stdio
 */
struct __FILE
{
	int dummy;
};

// Redefinimos el standard output e input
FILE __stdout;
FILE __stdin;

/*
 *  Variables Globales
 */

uint32_t key_aux = 0;									// Variable auxiliar para guardar una tecla
uint8_t keysHex[KEYBOARD_SIZE] = 						// Array con las teclas en hexadecimal
{
	0x06, 0x5B, 0x4F, 0x77,	//	1	2	3	A
	0x66, 0x6D, 0x7D, 0x7C,	//	4	5	6	B
	0x07, 0x7F, 0x67, 0x39,	//	7	8	9	C
	0x79, 0x3F, 0x71, 0x5E	//	E	0	F	D
};
uint8_t keysDec[KEYBOARD_SIZE] =						// Array con las teclas en decimal
{
	1, 2, 3, 10,	//	1	2	3	A
	4, 5, 6, 11,	//	4	5	6	B
	7, 8, 9, 12,	//	7	8	9	C
	0, 0, 0, 13		//	0	0	0	D
};
uint8_t admin_password[CODE_SIZE] = { NULL };		// Contraseña de administrador
uint8_t code_buffer[CODE_SIZE];						// Buffer para el codigo ingresado
uint8_t code_buffer_cfg[(CODE_SIZE*2)-1];			// Buffer para codigo y contraseña
uint8_t unlock = 1;									// Variable para configuraciones de admin
Employee employees[EMPLOYEES_NUM];					// Array con informacion de empleados
uint8_t emp_index = 0;								// Numero de empleados registrados
uint8_t flagLed = 0;								// Flag para prender LEDs de control

uint8_t pin_flag = 0; 								// flag para saber si es flanco de subida o bajada en capture
uint8_t sensor_flag = 0; 							// 1 cuandose esta midiendo
uint8_t timeout_barrier_counter = 0;
uint8_t echoTime = 0;								// Tiempo que tarda en volver la onda ultrasonica
float distance = 0;									// Distancia medida por el sensor

uint8_t motorCtrl = 0;								// Variable para controlar el movimiento del motor
uint8_t flagTimerMotor = 0;							// Variable para controlar los pines del motor
uint8_t barrierDone = 0;							// Variable para controlar cuando la barrera este completamente abierta
uint16_t pasos = 0;									// Variable para contar los pasos del motor

__IO uint32_t ADCValue;								// Variable para ver el valor de la bateria
volatile uint8_t DMA_ADC_Flag = 0;					// Variable para marcar cuando el ADC termina

/*
 * 	Enum para los diferentes estados del dispositivo
 */
typedef enum {
	WAITING = 0, // esperando a que suceda algo, no se debe hacer nada de logica durante este tiempo
	BARRIER_UP,
	SENSING, // sensando
	BARRIER_DOWN,
	CONVERTING
} PROGRAM_STATUS;

PROGRAM_STATUS STATUS = WAITING; //variable con la que controlamos el estado del programa

/*
 * 	Funcion Principal.
 */
int main(void) {

	/*
	 * 	CONFIGURACIONES INICIALES
	 */

	// Limpiamos pines de control
	GPIO_ClearValue(PORT_ONE,PIN_23);
	GPIO_ClearValue(PORT_ONE,PIN_20);
	GPIO_ClearValue(PORT_ONE,PIN_21);
	// Configuraciones para el teclado
	configPin();
	// Configuraciones para el sensor
	configSensorPins();
	EchoConfig();
	GPIO_ClearValue(PORT_ONE, PIN_26);
	// Configuraciones para el motor
	configMotorPins();
	configMotorTimer();
	// Configuraciones para el ADC
	configADCPins();
	configDMA();
	configADC();
	// Configuraciones para UART
	configUARTpins();
	configUART();

	configRTC();

	/*
	 * 									MAQUINA DE ESTADOS
	 */

    while(1) {

		switch(STATUS){
			/* -------------------------------- Sensor -------------------------------- */
			case SENSING:
				sensingBarrier();
				break;

			/* ------------------------------------------------------------------------ */
			/* ------------------------------- Esperando ------------------------------ */
			case WAITING:
				break;
			/* -------------------------------- Bajando Barrera -------------------------------- */
			case BARRIER_DOWN:
				motorControl(BARRIER_CLOSE);
				break;
			/* ---------------------------------------------------------------------------------- */
			/* -------------------------------- Subiendo Barrera -------------------------------- */
			case BARRIER_UP:
				motorControl(BARRIER_OPEN);
				break;
			/* ------------------------------------------------------------------------------- */
			/* -------------------------------- Convirtiendo -------------------------------- */
			case CONVERTING:
				ADCtoDMA();
				break;
			/* ------------------------------------------------------------------------------ */
		}
	}

    return 0 ;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//										TECLADO
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Funcion que configura todos los pines que se utilizan en el proyecto, incluidas sus
 * 	funciones alternas.
 */
void configPin(void) {
	PINSEL_CFG_Type pin_config;

	/*
	 * 	PUERTO 0 :
	 * 	Salidas para Display 7-segmentos
	 */
	pin_config.Portnum = PINSEL_PORT_0;
	pin_config.Funcnum = PINSEL_FUNC_0;
	pin_config.OpenDrain = PINSEL_PINMODE_NORMAL;
	pin_config.Pinmode = PINSEL_PINMODE_TRISTATE;
	for(uint8_t i = 0; i < 8; i++){
		pin_config.Pinnum = i;
		PINSEL_ConfigPin(&pin_config);
	}
	GPIO_SetDir(PORT_ZERO,0xFF,OUTPUT);

	/*
	 * 	PUERTO 2 :
	 * 	Teclado Matricial
	 */
	pin_config.Portnum = PINSEL_PORT_2;
	pin_config.Funcnum = PINSEL_FUNC_0;
	pin_config.OpenDrain = PINSEL_PINMODE_NORMAL;
	pin_config.Pinmode = PINSEL_PINMODE_TRISTATE;

	// Configuramos las filas = SALIDAS
	for(uint8_t i = 0; i < KEY_ROW; i++) {
		pin_config.Pinnum = i;
		PINSEL_ConfigPin(&pin_config);
	}
	// Configuramos las columnas = ENTRADAS
	pin_config.Pinmode = PINSEL_PINMODE_PULLUP;
	for(uint8_t i = KEY_ROW; i < (KEY_ROW + KEY_COL); i++) {
		pin_config.Pinnum = i;
		PINSEL_ConfigPin(&pin_config);
	}

	// Configuramos columnas y filas GPIO
	GPIO_SetDir(PORT_TWO, 0xF, OUTPUT);
	GPIO_SetDir(PORT_TWO, (0xF << 4), INPUT);

	// Configuramos interrupciones por GPIO del puerto 2
	FIO_IntCmd(PORT_TWO,(0xF << 4),FALLING);
	FIO_ClearInt(PORT_TWO,(0xF << 4));

	// Configuracione para LEDs
	GPIO_SetDir(PORT_ONE,PIN_23,OUTPUT);
	GPIO_SetDir(PORT_ONE,PIN_20,OUTPUT);
	GPIO_SetDir(PORT_ONE,PIN_21,OUTPUT);

	// Habilitamos las interrupciones
	NVIC_EnableIRQ(EINT3_IRQn);
}


/*
 * 	El teclado funciona por interrupciones GPIO. Esta funcion se encarga de analizar las teclas
 * 	presionadas y hacer algo dependiendo de las teclas que hayan sido presionadas.
 * 	Si la primer tecla presionada es una letra (A,B,C o D) sabemos que se quiere realizar una
 * 	accion de admin (configPassword, registerEmployee, getLog).
 * 	En caso contrario, es un empleado intentando ingresar (enterEmployee).
 */
void EINT3_IRQHandler(void) {
	// Desactivamos la interrupcion por EINT3
	NVIC_DisableIRQ(EINT3_IRQn);

	// Variable auxiliar para contar los digitos del codigo
	static uint8_t dig_aux = 0;
	// Variable auxiliar para la tecla de configuracion
	static uint8_t conf_key = 0;

	// Guardamos el valor de tecla en un aux
	key_aux = (GPIO_ReadValue(PORT_TWO) & 0xF0);

	// Hacemos un delay anti-rebote
	bounceDelay();

	/*/////////////////////////////////////////////////////////////////////////////////////////////
	 *								Analisis del teclado
	 *////////////////////////////////////////////////////////////////////////////////////////////

	// Chequeamos que el valor de la tecla sea el mismo despues del delay
	if(((GPIO_ReadValue(PORT_TWO) & 0xF0) - key_aux) == 0) {
		// Obtenemos el valor de la tecla presionada
		uint8_t key = getKey();
		// Valor en hexadecimal para mostrarlo por display
		uint8_t keyHex = keysHex[key];
		// Valor decimal para realizar operaciones
		uint8_t keyDec = keysDec[key];

		LPC_GPIO0->FIOPIN0 = keyHex;

		// Si la tecla presionada es una letra:
		if(keyDec > 9){
			// Reiniciamos el dig_aux
			dig_aux = 0;
			// Guardamos el valor en la variable auxiliar para configuracion
			conf_key = keyDec;
		} else {	// En caso contrario:
			// Guardamos el valor de la tecla en el buffer
			code_buffer[dig_aux] = keyDec;
			// Aumentamos la variable auxiliar para digitos
			dig_aux++;
		}

	}

	/*
	 * 		CERRAMOS BARRERA
	 */
	if(conf_key == LETTER_D) {


		if(motorCtrl && barrierDone) {	// Solo se puede cerrar si esta completamente abierta
			STATUS = BARRIER_DOWN;
		} else {
			turnLED(PORT_ONE,PIN_20);
			// estado WAITING
			STATUS = WAITING;
		}
		// Reiniciamos auxiliar
		dig_aux = 0;
		conf_key = 0;

	}

	/*////////////////////////////////////////////////////////////////////////////////////////////
	 * 								Analisis del codigo
	 *////////////////////////////////////////////////////////////////////////////////////////////

	// Cuando tengamos el codigo completo
	if((dig_aux == CODE_SIZE)){

		// Si se quiere configurar algo y el dispositivo esta bloqueado
		if((!unlock) && (conf_key != 0)) {
			// Chequeamos la contraseña
			if(!checkPassword(code_buffer)) {
				// Prendemos un led
				turnLED(PORT_ONE,PIN_20);
				//STATUS = WAITING;
			}
			// Reiniciamos auxiliar
			dig_aux = 0;
			// Limpiamos banderas
			FIO_ClearInt(PORT_TWO, (0xF << 4));
			// Volvemos a habilitar las interrupciones
			NVIC_EnableIRQ(EINT3_IRQn);
			// Salimos de la funcion
			return;
		}

		// Si no se presiono tecla de configuracion, hay un empleado ingresando
		if(conf_key == 0) {

			/*
			 * 		EMPLEADO INGRESANDO
			 */
			enterEmployee(code_buffer);
			conf_key = 0;

		} else if(conf_key == LETTER_A) {

			/*
			 * 		CONFIGURACION DE CONTRASEÑA
			 */
			if(unlock) {
				configPassword(code_buffer);
				conf_key = 0;
				unlock = 0;
			} else {
				turnLED(PORT_ONE,PIN_20);
				// Estado WAITING
				STATUS = WAITING;
			}

		} else if(conf_key == LETTER_B) {

			/*
			 * 		REGISTRO DE EMPLEADO
			 */
			if(unlock) {
				registerEmployee(code_buffer);
				conf_key = 0;
				unlock = 0;
			} else {
				turnLED(PORT_ONE,PIN_20);
				// estado WAITING
				STATUS = WAITING;
			}

		} else if(conf_key == LETTER_C) {

			/*
			 * 		OBTENER REGISTROS
			 */
			if(unlock) {
				getLog(code_buffer);
				conf_key = 0;
				unlock = 0;
			} else {
				turnLED(PORT_ONE,PIN_20);
				STATUS = WAITING;
			}

		} else {
			turnLED(PORT_ONE,PIN_20);
			STATUS = WAITING;
		}

		// Reiniciamos auxiliar
		dig_aux = 0;
	}

	// Limpiamos banderas
	FIO_ClearInt(PORT_TWO, (0xF << 4));
	// Volvemos a habilitar las interrupciones
	NVIC_EnableIRQ(EINT3_IRQn);
	// Retornamos
	return;
}

/*
 * 	Funcion que se encarga de realizar un retardo anti-rebote
 * 	para las teclas del teclado.
 */
void bounceDelay(void) {
	for(uint32_t tmp = 0; tmp < 600000; tmp++);
}

/*
 * 	Funcion que se encarga de escanear el teclado y obtener la tecla
 * 	que fue presionada.
 */
uint8_t getKey(void) {
	uint8_t row = 0;
	uint8_t col = 0;

	/*
	 *  FILAS:
	 *	Se envia un 1 por fila, y se analiza el primer byte
	 *	del Puerto 2 (P2.0 a 7). Si se presiono una tecla en
	 *	la fila "i", todas las columnas deberian ser 1, por lo
	 *	que el nibble superior deberia ser 0x7.
	 */
	for(uint8_t i = 0; i < KEY_ROW; i++) {
		// Setea en 1 el pin P2.i
		GPIO_SetValue(PORT_TWO,((uint32_t)(1<<i)));

		// Analizo si estan en 1 las columnas
		if((FIO_ByteReadValue(PORT_TWO, 0) & 0xF0) == 0xF0) {
			// Guardo el valor de la fila
			row = i;
			// Setea en 0 el pin P2.i
			GPIO_ClearValue(PORT_TWO,((uint32_t)(1<<i)));
			// Sale del bucle
			break;
		}

		// Setea en 0 el pin P2.i
		GPIO_ClearValue(PORT_TWO,((uint32_t)(1<<i)));
	}

	/*
	 * COLUMNAS:
	 * Se analiza el auxiliar de tecla hasta encontrar un 0,
	 * en cuyo caso se guarda la columna.
	 */
	for(uint8_t i = 0; i < KEY_COL; i++) {
		if( !(key_aux & ((uint32_t)(1<<(4+i)))) ){
			// Guarda el valor de la columna
			col = i;
			// Sale del bucle
			break;
		}
	}

	// Retorno el valor de la tecla
	return ((4 * row) + col);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//										  BARRERA
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Configuracion para los pines asociados al motor paso a paso.
 */
void configMotorPins(void) {
	// Pine P0.23 a P0.26 como salida
	GPIO_SetDir(PORT_ZERO,PIN_22,OUTPUT);
	GPIO_SetDir(PORT_ZERO,PIN_23,OUTPUT);
	GPIO_SetDir(PORT_ZERO,PIN_24,OUTPUT);
	GPIO_SetDir(PORT_ZERO,PIN_25,OUTPUT);
}

/*
 * 	Configuracion para los timers asociados al motor paso a paso.
 */
void configMotorTimer(void) {
	TIM_TIMERCFG_Type timer_config;
	TIM_MATCHCFG_Type match_config;

	/*
	 * 	Timer 2: Control de bobinas del motor
	 */

	// Configuramos timer
	timer_config.PrescaleOption = TIM_PRESCALE_TICKVAL;
	timer_config.PrescaleValue = 100;

	TIM_Init(LPC_TIM2,TIM_TIMER_MODE,&timer_config);

	// Configuramos match 0
	match_config.MatchChannel = 0;
	match_config.IntOnMatch = ENABLE;
	match_config.ResetOnMatch = ENABLE;
	match_config.StopOnMatch = DISABLE;
	match_config.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	match_config.MatchValue = 1250;// 5ms

	TIM_ConfigMatch(LPC_TIM2,&match_config);

	// Seteamos prioridades
	NVIC_SetPriority(TIMER2_IRQn,5);

	// Habilitamos interrupciones
	NVIC_EnableIRQ(TIMER2_IRQn);
}

/*
 * 	Funcion que controla el movimiento del motor dependiendo del
 * 	parametro motorCtrl. Si este es 1, la barrera se abre, y si es
 * 	0, la barrera se cierra.
 */
void motorControl(uint8_t ctrl) {
	// Modificamos la variable de control del motor
	//		1 = motorOpen
	//		0 = motorClose
	motorCtrl = ctrl;

	// El dispositivo queda en loop, funcionando por interrupciones
	STATUS = WAITING;

	// Activamos Timer 2 para movilizar el motor MATCH 0 PARA TEMP EL MOTOR M1
	TIM_Cmd(LPC_TIM2,ENABLE);

}

/*
 * 	Funcion que moviliza el motor para abrir la barrera.
 */
void motorOpen(void) {
	// Interrupcion por match 0
	if(TIM_GetIntStatus(LPC_TIM2,TIM_MR0_INT)) {
		switch(flagTimerMotor){
			case 0:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_22);
				flagTimerMotor++;
				pasos++;
				break;

			case 1:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_23);
				flagTimerMotor++;
				pasos++;
				break;

			case 2:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_24);
				flagTimerMotor++;
				pasos++;
				break;

			case 3:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_25);
				flagTimerMotor = 0;
				pasos++;
				break;
		}
	}
	return;

}

/*
 * 	Funcion que moviliza el motor para cerrar la barrera.
 */
void motorClose(void) {
	// Interrupcion por match 0
	if(TIM_GetIntStatus(LPC_TIM2,TIM_MR0_INT)) {
		switch(flagTimerMotor){
			case 0:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_25);
				flagTimerMotor++;
				pasos++;
				break;

			case 1:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_24);
				flagTimerMotor++;
				pasos++;
				break;

			case 2:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_23);
				flagTimerMotor++;
				pasos++;
				break;

			case 3:
				clearMotorPins();
				GPIO_SetValue(PORT_ZERO,PIN_22);
				flagTimerMotor = 0;
				pasos++;
				break;
		}
	}
	return;
}

/*
 * 	Funcion que se encarga de movilizar el motor para un lado o para
 * 	el otro dependiendo de si queremos abrir o cerrar la barrera.
 */
void TIMER2_IRQHandler(void) {

	if(pasos == 512) {
		//terminó de abrir o cerrar
		pasos = 0;
		// Desactivamos Timer 2 para parar el motor
		TIM_Cmd(LPC_TIM2,DISABLE);
		clearMotorPins();

		// Reiniciamos y desactivamos Timer
		TIM_ResetCounter(LPC_TIM2);

		// Limpiamos banderas
		TIM_ClearIntPending(LPC_TIM2,TIM_MR0_INT);
		TIM_ClearIntPending(LPC_TIM2,TIM_MR1_INT);

		// Chequeamos el estado de la bateria cuando se cierra la barrera
		if(!motorCtrl) {
			barrierDone = 0;
			/* CON DMA */
			STATUS = CONVERTING;
			return;
		} else {
			// Se termino de abrir la barrera
			barrierDone = 1;
			// Empezamos a sensar
			STATUS = SENSING;
			return;
		}


	} else {

		if(motorCtrl) {
			motorOpen();
		} else {
			motorClose();
		}

		// Limpiamos bandera
		TIM_ClearIntPending(LPC_TIM2,TIM_MR0_INT);
	}
}

/*
 * 	Funcion que pone a convertir el ADC y luego espera a
 * 	que el DMA lea el valor convertido para chequear el
 * 	estado de la bateria con este.
 */
void ADCtoDMA(void) {
	// Configuramos DMA
	configDMA();

	// Habilitamos Interrupcion por DMA
	NVIC_EnableIRQ(DMA_IRQn);
	// La interrupcion por ADC debe estar desactivada
	NVIC_DisableIRQ(ADC_IRQn);

	// La interrupcion por canal del ADC esta activada para levantar su flag
	ADC_IntConfig(LPC_ADC,ADC_ADINTEN3,ENABLE);

	STATUS = WAITING;

	// Mandamos a convertir
	ADC_StartCmd(LPC_ADC, ADC_START_NOW);

	// Habilitamos el canal DMA
	GPDMA_ChannelCmd(DMA_CHANNEL_0,ENABLE);
}

/*
 * 	Funcion auxiliar para el control del motor.
 */
void clearMotorPins(void) {
	GPIO_ClearValue(PORT_ZERO,PIN_22);
	GPIO_ClearValue(PORT_ZERO,PIN_23);
	GPIO_ClearValue(PORT_ZERO,PIN_24);
	GPIO_ClearValue(PORT_ZERO,PIN_25);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//										  SENSOR
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Funcion que sensa la presencia de un vehiculo entrando.
 * 	Si la distancia medida es menor a DISTANCE_TO_WALL / 2,
 * 	entonces se considera que hay un vehiculo.
 */
void sensingBarrier(void) {

	while(1) {
		// Nos aseguramos de que Trig este en 0
		GPIO_ClearValue(PORT_ONE, PIN_26);

		/* enviamos un 1 durante 10us a Trig */
		GPIO_SetValue(PORT_ONE, PIN_26);
		delay(10);
		GPIO_ClearValue(PORT_ONE, PIN_26);

		// Esperamos por un 1 en ECHO
		while(!(GPIO_ReadValue(PORT_ONE) & (PIN_18)));

		TIM_ResetCounter(LPC_TIM1);
		TIM_Cmd(LPC_TIM1, ENABLE);

		// Esperamos por un 0 en ECHO
		while(GPIO_ReadValue(PORT_ONE) & (PIN_18));

		// Tiempo que tardo en llegar la onda
		echoTime = stopTimer1();

		// Distancia sensada
		distance = ((0.0343 * echoTime)/2)*100;

		// Suponemos que no hay vehiculo
		if(distance > DISTANCE_TO_WALL/2) {
			timeout_barrier_counter++;
			GPIO_ClearValue(PORT_ONE,PIN_21);
		} else {	// Suponemos que hay vehiculo
			timeout_barrier_counter = 0;
			GPIO_SetValue(PORT_ONE,PIN_21);
		}

		// Nos aseguramos de que no haya nada antes de cerrar la barrera
		if(timeout_barrier_counter == 10) {
			break;
		}

		// Mide cada 0.5 segundos
		delay(50000);
	}

	// Reiniciamos la variables
	distance = 0;
	timeout_barrier_counter = 0;
	GPIO_ClearValue(PORT_ONE,PIN_21);
	// Cerramos la barrera
	STATUS = BARRIER_DOWN;
}

/*
 * 	Metodo que genera un delay en us
 */
void delay(uint32_t time){

	for(uint32_t i = 0; i < (time*100); i++){}

}

/*
 * 	Configuracion de los pines necesarios para el HC-SR04
 *
 * 	Trigger: P1.28
 * 	Echo: P1.18 - CAP1.0
 */
void configSensorPins(){
	PINSEL_CFG_Type SR04;

	/* Trigger */
	SR04.Portnum = PINSEL_PORT_1;
	SR04.Pinnum = PINSEL_PIN_26;
	SR04.Funcnum = PINSEL_FUNC_0;
	SR04.OpenDrain = PINSEL_PINMODE_NORMAL;
	SR04.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&SR04);

	GPIO_SetDir(PORT_ONE, PIN_26, OUTPUT);

	/* Echo */
	SR04.Portnum = PINSEL_PORT_1;
	SR04.Pinnum = PINSEL_PIN_18;
	SR04.Funcnum = PINSEL_FUNC_0;
	SR04.OpenDrain = PINSEL_PINMODE_NORMAL;
	SR04.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&SR04);

	GPIO_SetDir(PORT_ONE, PIN_18, INPUT);

	/* LED pin */
	PINSEL_CFG_Type SensorLed;

	SensorLed.Portnum = PINSEL_PORT_1;
	SensorLed.Pinnum = PINSEL_PIN_21;
	SensorLed.Funcnum = PINSEL_FUNC_0;
	SensorLed.OpenDrain = PINSEL_PINMODE_NORMAL;
	SensorLed.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&SensorLed);

	GPIO_SetDir(PORT_ONE, PIN_21, OUTPUT);

}

/*
 * 	Metodo que configura el timer 1 para que funcione como CAP1.0
 */
void EchoConfig(){

	/* Capture en flanco de bajada y subida para capturar la respuesta del sensor */

	TIM_TIMERCFG_Type timer_1;
	timer_1.PrescaleOption = TIM_PRESCALE_USVAL;
	timer_1.PrescaleValue = 100;
	TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &timer_1);

	TIM_MATCHCFG_Type match_conf;
	match_conf.MatchChannel = 0;
	match_conf.IntOnMatch = DISABLE;
	match_conf.ResetOnMatch = DISABLE;
	match_conf.StopOnMatch = DISABLE;
	match_conf.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	match_conf.MatchValue = 10;
	TIM_ConfigMatch(LPC_TIM1, &match_conf);

	NVIC_SetPriority(TIMER1_IRQn,1); // prioridad mas baja que el systick
	NVIC_EnableIRQ(TIMER1_IRQn);
}

/*
 * 	Funcion que detiene el timer y devuelve el valor del
 * 	tiempo transcurrido.
 */
uint8_t stopTimer1(void) {
	TIM_Cmd(LPC_TIM1,DISABLE);
	return LPC_TIM1->TC;
}

/*
 * Metodo que prende o apaga el led
 */
void turnOnPin(uint8_t captureValue){
	if(0 < captureValue && captureValue < 2048)
		GPIO_SetValue(PORT_ONE, PIN_21);
	else
		GPIO_ClearValue(PORT_ONE, PIN_21);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//											ADC
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Configuracion para los pines del ADC.
 * 	Pin 0.26 como ADC channel 3.
 */
void configADCPins(void) {
	PINSEL_CFG_Type adcPin_config;

	adcPin_config.Portnum = PINSEL_PORT_0;
	adcPin_config.Pinnum = PINSEL_PIN_26;
	adcPin_config.Pinmode = PINSEL_PINMODE_TRISTATE;
	adcPin_config.OpenDrain = PINSEL_PINMODE_NORMAL;
	adcPin_config.Funcnum = PINSEL_FUNC_1;

	PINSEL_ConfigPin(&adcPin_config);
}


/*
 * 	Funcion donde se configura el ADC
 * 	Canal 0
 * 	Frecuencia de muestreo = 200KHz
 * 	Al finalizar la conversion setea sus banderas pero
 * 	no interrumpe directamente.
 */
void configADC(void) {
	/*
	 * Configuracion del ADC - canal 3
	 * Frecuencia de muestreo = 200KHz
	 * Interrumpe al finalizar la conversion
	 */

	LPC_SC->PCLKSEL0 |= (3<<24);	// 13Mhz aprox

	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_ADINTEN3,ENABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_3,ENABLE);

	// Las interrupciones deben estar descativadas para usar ADC con DMA
	NVIC_DisableIRQ(ADC_IRQn);
}

/*
 * 	Funcion de configuracion del DMA.
 */
void configDMA(void) {
	NVIC_DisableIRQ(DMA_IRQn);

	GPDMA_Init();

	// Configuramos el canal del DMA
	GPDMA_Channel_CFG_Type DMA;
	DMA.ChannelNum = DMA_CHANNEL_0;
	DMA.TransferSize = 1;
	DMA.TransferWidth = GPDMA_WIDTH_WORD;
	DMA.SrcMemAddr = 0;
	DMA.DstMemAddr = (uint32_t)&ADCValue;
	DMA.TransferType = GPDMA_TRANSFERTYPE_P2M;
	DMA.SrcConn = GPDMA_CONN_ADC;
	DMA.DstConn = 0;
	DMA.DMALLI = 0;

	GPDMA_Setup(&DMA);

	NVIC_SetPriority(DMA_IRQn,6);
}

/*
 * 	Funcion rutina de interrupcion del DMA que aumenta en 1
 * 	la variable DMA_ADC_Flag, la cual es usada como el
 * 	terminal counter del DMA.
 */
void DMA_IRQHandler(void){
	// Reiniciamos bandera DONE
	ADC_GlobalGetData(LPC_ADC);

	// Desactivamos canal DMA
	GPDMA_ChannelCmd(DMA_CHANNEL_0,DISABLE);

	// Reiniciamos bandera para marcar que el DMA realizo la transferencia
	DMA_ADC_Flag = 1;

	// Chequeamos la bateria
	checkBat();

	// Volvemos al estado waiting
	STATUS = WAITING;

	// Limpiamos bandera
	GPDMA_ClearIntPending(GPDMA_STATCLR_INTTC, DMA_CHANNEL_0);
}

/*
 * Funcion donde se compara el valor de la bateria
 * Si es menor a 25% prendemos un led
 */
void checkBat(void) {
	/*
	 * 12 bits -> 4096
	 * 25% de 4096: 1024
	 */

	if(ADC_DMA_RESULT(ADCValue) <= (uint32_t)1024)
		GPIO_SetValue(PORT_ONE, PIN_23);
	else
		GPIO_ClearValue(PORT_ONE, PIN_23);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//									 COMUNICACION SERIE
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Funcion para configurar los pines del UART para
 * 	transmitir.
 */
void configUARTpins(void) {
	PINSEL_CFG_Type pin_config;

	// Pines P0.10 como TXD2
	pin_config.Portnum = PINSEL_PORT_0;
	pin_config.Pinnum = PINSEL_PIN_10;
	pin_config.Funcnum = PINSEL_FUNC_1;
	pin_config.Pinmode = PINSEL_PINMODE_PULLUP;
	pin_config.OpenDrain = PINSEL_PINMODE_NORMAL;

	PINSEL_ConfigPin(&pin_config);
}

/*
 * 	Funcion para configurar el UART.
 */
void configUART(void) {
	UART_CFG_Type UART_config;
	UART_FIFO_CFG_Type UART_FIFO_config;

	// Configuramos el UART
	UART_ConfigStructInit(&UART_config);
	UART_Init(LPC_UART2, &UART_config);

	// Configuramos la FIFO
	UART_FIFOConfigStructInit(&UART_FIFO_config);
	UART_FIFOConfig(LPC_UART2, &UART_FIFO_config);

	// Habilitamos la transmision
	UART_TxCmd(LPC_UART2, ENABLE);
}

/*
 * 	Funcion para transmitir caracteres por UART.
 */
void UART_Write(char c) {
	// Esperamos a que THR este vacio
	while( !(LPC_UART2->LSR & THRE) );

	// En caso de ser \n, hacemos un salto y return
	if(c == '\n') {
		LPC_UART2->THR = CARRIAGE_RETURN;
		while( !(LPC_UART2->LSR & THRE) );
		LPC_UART2->THR = LINE_FEED;
	} else {
		LPC_UART2->THR = c;
	}

}

/*
 * 	Funcion para trasmitir un caracter.
 * 	Devuelve el caracter transmitido en caso exitoso.
 * 	Se redefine para poder utilizar printf().
 */

int __sys_write(int iFileHandle, char *pcBuffer, int iLength) {
	unsigned int i;
	for(i = 0;  i < iLength; i++) {
		UART_Write(pcBuffer[i]);
	}
	return iLength;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//										ADMINISTACION
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 	Funcion que se encarga de configurar la contraseña de admin, si se presiono primeramente la
 * 	*tecla A*. La contraseña consta de 4 numeros y es guardada en la variable global admin_password.
 * 	Si se vuelve a presionar la *tecla A* luego de haber una contraseña configurada, se necesitara que
 * 	se ingrese la misma antes de reconfigurar la contraseña.
 */
void configPassword(uint8_t *code) {

	for(uint8_t i = 0; i < CODE_SIZE; i++) {
		admin_password[i] = code[i];
	}

}

/*
 *	Funcion que se encarga de registrar el codigo de un empleado, si se presiono primeralmente la
 *	*tecla B*. Luego de presionar esa tecla, el admin debe ingresar la contraseña para luego poder
 *	ingresar el numero de identificacion del empleado. Este codigo consta de 4 numeros.
 *	Cada empleado esta representado por la estructura "Employee". Al regsitrar un nuevo empleado
 *	se crea una instancia de esta estructura con el campo codigo siendo el ingresado.
 */
void registerEmployee(uint8_t *code) {

	// Cargamos la informacion del empleado
	for(uint8_t i = 0; i < CODE_SIZE; i++) {
		employees[emp_index].codigo[i] = code[i];
	}
	employees[emp_index].area = checkArea(code[0]);
	employees[emp_index].presentismo = 0;
	employees[emp_index].ausente = 0;

	// Aumentamos el numero de empleados registrados
	emp_index++;

	return;
}

/*
 * 	Funcion auxiliar de registerEmployee que analiza el primer numero del codigo para completar
 * 	el campo "area" del empleado.
 */
char checkArea(uint8_t ref) {
	switch(ref) {
		case 1:
			return 'M';
		case 2:
			return 'V';
		case 3:
			return 'R';
		case 4:
			return 'P';
		case 5:
			return 'O';
		case 6:
			return 'S';
		case 7:
			return 'T';
		case 8:
			return 'L';
		case 9:
			return 'C';
		case 0:
			return 'x';
		default:
			return 'X';
	}
}

/*
 * 	Funcion que compara el codigo ingresado con los que hay en la lista de empleados registrados.
 * 	Si el codigo coincide, se abre la barrera (motorOpen) y se pone en 0 el campo "presentismo"
 * 	del empleado.
 */
void enterEmployee(uint8_t *code) {
	uint8_t entering = EMPLOYEES_NUM;

	for(uint8_t i = 0; i < EMPLOYEES_NUM; i++) {
		for(uint8_t j = 0; j < CODE_SIZE; j++) {
			if(employees[i].codigo[j] != code[j]) {
				break;
			} else {
				if(j == (CODE_SIZE-1)) {
					// Guardamos el numero de empleado
					entering = i;
				}
			}
		}
	}

	// Modificamos la info del empleado
	if(entering < EMPLOYEES_NUM) {
		employees[entering].presentismo = 0;
		// Si el empleado existe el estado cambia a BARRIER_UP
		STATUS = BARRIER_UP;
	} else { // Sino encontro al empleado o el codigo esta mal, muestra un mensaje de error
		turnLED(PORT_ONE,PIN_20);
		// Si no existe, muestre error y pasa a WAITING
		STATUS = WAITING;
	}

}

/*
 * 	Funcion que se encarga de enviar por transmision serie, el array con
 *	todos los empleados y sus respectivos estados de presentismo. Esto ocurre
 *	si se presiono primeramente la *tecla C* y luego el admin ingreso su
 *	contraseña.
 */
void getLog(uint8_t *code) {
	uint8_t ausentes = 0;

	printf("Informacion de los empleados:\n");

	// Imprimimos la informacion de todos los empleados
	for(uint8_t i = 0; i < EMPLOYEES_NUM ; i++) {
		printf("\tEmpleado: %d ; Area: %c ; ID: ",i ,employees[i].area);

		if(employees[i].presentismo >= 1) {
			ausentes++;
		}

		for(uint8_t j = 0; j < CODE_SIZE; j++) {
			printf("%d", employees[i].codigo[j]);
		}

		printf("\n");
	}

	// Calculamos el porcentaje de empleados ausentes
	printf("Cantidad de empleados ausentes: \n");
	printf("\t %d/%d \n", ausentes,emp_index);

}

/*
 * 	Funcion que verifica que la contraseña ingresada sea correcta y
 * 	desbloquea el dispositivo en caso de serlo
 * 	En caso de serlo devuelve 1 y hace que la variable unlock sea 1.
 * 	En caso contrario devuelve 0.
 */
uint8_t checkPassword(uint8_t *code) {
	// Si no hay contraseña se devuelve 1
	if(admin_password == NULL) {
		unlock = 1;
		return 1;
	}

	// Seteamos la contraseña
	for(uint8_t i = 0; i < CODE_SIZE; i++) {
		if(code[i] != admin_password[i]) {
			unlock = 0;
			return 0;
		}
	}

	unlock = 1;
	return 1;
}

/*
 * 	Funcion que prende un LED de un pin especifico por
 * 	un pequeño tiempo.
 */
void turnLED(uint8_t port, uint32_t pin) {
	GPIO_SetValue(port,pin);
	delay(100000); // delay de 1seg
	GPIO_ClearValue(port,pin);
}

/*
 * 	Funcion para configurar el reloj de tiempo real
 */
void configRTC(void) {
	RTC_TIME_Type RTC_Config;
	RTC_Init (LPC_RTC);

	/*
	 * 	Configuracion de la fecha inicial.
	 * 	Normalmente se pondria el dia y hora en que
	 * 	se instala el dispositivo.
	 */
	RTC_Config.YEAR = 2022;
	RTC_Config.MONTH = 11;
	RTC_Config.DOM = 3;
	RTC_Config.HOUR = 14;
	RTC_Config.MIN = 30;

	RTC_SetFullTime(LPC_RTC,&RTC_Config);

	/*
	 * 	Configuramos la alarma.
	 * 	Normalmente se configuraria a las 00:00
	 */
	RTC_SetAlarmTime(LPC_RTC,RTC_TIMETYPE_HOUR, 14);
	RTC_SetAlarmTime(LPC_RTC,RTC_TIMETYPE_MINUTE, 40);

	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_HOUR,ENABLE);
	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_MINUTE,ENABLE);

	// Iniciamos el RTC
	RTC_Cmd(LPC_RTC, ENABLE);
	// Habilitamos interrupciones
	NVIC_EnableIRQ(RTC_IRQn);
}

void RTC_IRQHandler(void) {

	if(RTC_GetIntPending(LPC_RTC,RTC_INT_ALARM)) {
		RTC_AlarmHandler();
	} else if (RTC_GetIntPending(LPC_RTC,RTC_INT_COUNTER_INCREASE)) {
		RTC_IncreaseHandler();
	}

}

/*
 * 	Funcion que, a la hora determinada de alarma, incrementa en 1
 * 	todos los valores de presencia de los empleados.
 */
void RTC_AlarmHandler(void) {
	// Revisamos que sea la hora exacta
	uint32_t horaAl = RTC_GetAlarmTime(LPC_RTC, RTC_TIMETYPE_HOUR);
	uint32_t minAl = RTC_GetAlarmTime(LPC_RTC, RTC_TIMETYPE_MINUTE);
	uint32_t horaAct = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_HOUR);
	uint32_t minAct = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_MINUTE);

	if(horaAl != horaAct || minAl != minAct) {
		// Limpiamos bandera
		RTC_ClearIntPending(LPC_RTC,RTC_INT_ALARM);
		return;
	}

	// Aumentamos el indice de presencia de los empleados
	for(uint8_t i = 0; i < EMPLOYEES_NUM; i++) {
		employees[i].presentismo++;
	}

	// Desactivamos interrupcion por alarma
	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_HOUR,DISABLE);
	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_MINUTE,DISABLE);
	// Activamos interrupcion por incremento
	RTC_CntIncrIntConfig(LPC_RTC,RTC_TIMETYPE_MINUTE,ENABLE);
	// Limpiamos bandera
	RTC_ClearIntPending(LPC_RTC,RTC_INT_ALARM);
}

/*
 * 	Funcion auxiliar de RTC_AlarmHandler utilizada para que esta
 * 	solo se corra una vez por dia.
 */
void RTC_IncreaseHandler(void) {
	// Volvemos a activar las interrupciones por alarma
	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_HOUR,DISABLE);
	RTC_AlarmIntConfig(LPC_RTC,RTC_TIMETYPE_MINUTE,DISABLE);
	// Desactivamos interrupcion por incremento
	RTC_CntIncrIntConfig(LPC_RTC,RTC_TIMETYPE_MINUTE,ENABLE);
	// Limpiamos bandera
	RTC_ClearIntPending(LPC_RTC,RTC_INT_COUNTER_INCREASE);
}

