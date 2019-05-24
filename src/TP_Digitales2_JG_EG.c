//  Trabajo Regularizador Electrónica Digital II - UNSAM
//  Segundo cuatrimestre 2017
//  Profesor: Nicolás Alvarez
//  Alumnos: Joaquin Gonzalez, Erik Gomez


//******************************************************
// Declaraciones de constantes, variables y librerias
//******************************************************
#include "funciones.h"
#include <math.h>

#define WAVE_SAMPLE_NUM	256					// Cantidad de puntos de la senal a generar
#define PI				(float)(3.14159265)
#define AMPLITUD		128				    // Amplificacion de la senal a generar (500 = por debajo de media escala (VALUE/1024)*VDD)
#define FRECUENCIA		2000				// Frecuencia de la senal a generar [HZ] - Base para el resto
#define GPDMA_CLK		204e6				// Frecuencia a la que opera DMA
//#define GPIO_PIN_INT_MASK		(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)

unsigned int tono_2_khz[WAVE_SAMPLE_NUM];	
unsigned int tono_4_khz[WAVE_SAMPLE_NUM];
unsigned int tono_8_khz[WAVE_SAMPLE_NUM];
unsigned int tono_16_khz[WAVE_SAMPLE_NUM];

unsigned int tono_buffer_1[WAVE_SAMPLE_NUM];
unsigned int tono_buffer_2[WAVE_SAMPLE_NUM];
unsigned int tono_suma[WAVE_SAMPLE_NUM];

static char debug[256];	                   // para debugging (usada por printf)
struct LLI_T LLI0;
struct LLI_T LLI1;

// Toggle para TEC_[1-4]
static bool TEC_1;
static bool TEC_2;
static bool TEC_3;
static bool TEC_4;
//static bool init;

//******************************************************
// Comienzo programa principal
//******************************************************

int main(void) {
	
	// Inicio de la UART para debugging
	UART_Init();
    // Configuración de LEDs y Teclas
    _Configuracion_IO();

// Calculo los valores de las señales y los guardo en sus respectivos vectores
    int i=0;

    for(i=0;i<WAVE_SAMPLE_NUM;i++){
        //Inicio Buffers
        tono_buffer_1[i]=0;
        tono_buffer_2[i]=0;
        tono_suma[i]=0;
        //Se genera tono de 2Khz
        tono_2_khz[i]=0;
        tono_2_khz[i]=((((unsigned int)(AMPLITUD + AMPLITUD*sin(2*PI*i/WAVE_SAMPLE_NUM)) & 0x3FF) << 6) | DAC_BIAS_1MHz);
        //Se genera tono de 4Khz
        tono_4_khz[i]=0;
        tono_4_khz[i]=((((unsigned int)(AMPLITUD + AMPLITUD*sin(4*PI*i/WAVE_SAMPLE_NUM)) & 0x3FF) << 6) | DAC_BIAS_1MHz);
        //Se genera tono de 8Khz
        tono_8_khz[i]=0;
        tono_8_khz[i]=((((unsigned int)(AMPLITUD + AMPLITUD*sin(8*PI*i/WAVE_SAMPLE_NUM)) & 0x3FF) << 6) | DAC_BIAS_1MHz);
        //Se genera tono de 16Khz
        tono_16_khz[i]=0;
        tono_16_khz[i]=((((unsigned int)(AMPLITUD + AMPLITUD*sin(16*PI*i/WAVE_SAMPLE_NUM)) & 0x3FF) << 6) | DAC_BIAS_1MHz);

    }
    
 	// Configuracion del DMA
	_Configuracion_DMA();

    //Inicializo Toggle para las Teclas
    //Entran en 1 al Handler y recorren las instrucción para cargar la señal
    //En Handler, luego, pone TEC_[i]=0 y la proxima vez que ingresen a
    //la estructura entonces removeran la señal del vector que hara que salga del DAC
    TEC_1=1;
    TEC_2=1;
    TEC_3=1;
    TEC_4=1;
    
    //Bucle que mantiene el programa corriendo
	while (1) {
	}

	return 0;
}

//******************************************************
// Fin principal
//******************************************************


//******************************************************
// Comienzo definición funciones
//******************************************************

void _Configuracion_IO(void){

    //Configuro pines (LEDs y teclas) como salida sin pull-up/pull-down con buffer habilitado
	Config_LEDS(SCU_MODE_DES | SCU_MODE_EZI);
	Config_Botones(SCU_MODE_DES | SCU_MODE_EZI);

	//Deshabilitación las resistencias de pull‐up/pull‐down del pin P4_4 y habilitación del DAC
	SCU->SFSP[4][4] = (SCU_MODE_DES | SCU_MODE_FUNC7);
	SCU->ENAIO[2] = 1;

    //Inicializo el PINTSEL (pag. 423 ARM NXP)
	SCU->PINTSEL[0]=0x0;
	
    //Configutro Interrupción para puertos GPIO
	//INTERRUPCION, PUERTO GPIO, Bit del PIN
	SCU_GPIOIntPinSel(0, 0, 4);	// TEC_1
	SCU_GPIOIntPinSel(1, 0, 8);	// TEC_2
	SCU_GPIOIntPinSel(2, 0, 9);	// TEC_3
	SCU_GPIOIntPinSel(3, 1, 9);	// TEC_4
	
	//Configuro interrupción por flanco descendente y la habilito
	GPIO_PIN_INT->ISEL=0x0;  //Edge Sensitive (pag. 457)
	GPIO_PIN_INT->IENF=0xF;  //Enable Falling Edge or Level interrupt (pag. 457)
	GPIO_PIN_INT->SIENF=0xF; //Enable Falling Edge or Level interrupt (pag. 458)
    
	//Configuro prioridad de la IRQ para GPIO
	//Estas interrupciones serán generadas por los GPIO y serán enviadas y gestionadas por NVIC
	//PIN Number IRQ + Prioridad
	NVIC_SetPri(PIN_INT0_IRQn,15);
	NVIC_SetPri(PIN_INT1_IRQn,15);
	NVIC_SetPri(PIN_INT2_IRQn,15);
	NVIC_SetPri(PIN_INT3_IRQn,15);
	//Se habilita IRQ en NVIC
	NVIC_EnaIRQ(PIN_INT0_IRQn);
	NVIC_EnaIRQ(PIN_INT1_IRQn);
	NVIC_EnaIRQ(PIN_INT2_IRQn);
	NVIC_EnaIRQ(PIN_INT3_IRQn);
}

void _Configuracion_DMA(void) {

    //Defino LLI0 - Conexión entre Datos-DMA-DAC
    LLI0.SRC     = (unsigned int) &tono_buffer_1[0];
    LLI0.DEST 	 = (unsigned int) &(_DAC->CR);
    LLI0.NEXT    = (unsigned int) &LLI1; // Apunta a LLI1
    LLI0.CONTROL = LLI_CTRL_MASK;

    //Defino LLI1 - Conexión entre Datos-DMA-DAC
    LLI1.SRC     = (unsigned int) &tono_buffer_2[0];
    LLI1.DEST 	 = (unsigned int) &(_DAC->CR);
    LLI1.NEXT    = (unsigned int) &LLI0; // Apunta a LLI0
    LLI1.CONTROL = LLI_CTRL_MASK;

    // Habilitacion del DMA (1 = Enabled / 0 = Disabled)
	_GPDMA->CONFIG = 1;
	
	_GPDMA->CH[0].LLI = LLI0;

    //Configuración Channel 0
	_GPDMA->CH[0].CONFIG = CH_CTRL_MASK;

	// DMA = 204 MHz (GPDMA_CLOCK)
    // Frecuencia funcionamiento = (counts / 20.4E6) * WAVE_SAMPLE_NUM
    int counts; // indican la cantidad de muestras/ciclo que van a entrar
	counts = GPDMA_CLK / (FRECUENCIA * WAVE_SAMPLE_NUM);
	_DAC->CNTVAL = counts & 0xFFFF;

	//Configuración del DAC
	_DAC->CTRL = (CNT_ENA | DMA_ENA | DBLBUF_ENA);
	_DAC->CR = (0x0 << 6);
    //Configuración DAC IRQ < IO IRQ
    NVIC_EnaIRQ(DMA_IRQn);
    NVIC_SetPri(DMA_IRQn, 3);

	sprintf_mio(debug,"\r\n  DAC CNTVAL = %d\r\n", _DAC->CNTVAL);
	DEBUGSTR(debug);
	sprintf_mio(debug,"\r\n  Frecuencia = %d [Khz]\r\n", ((counts / 20.4E6) * WAVE_SAMPLE_NUM)/1000);
	DEBUGSTR(debug);
	sprintf_mio(debug,"\r\n--- Reloj = %d [KHz]---\r\n", Chip_Clock_GetRate(CLK_APB3_DAC)/1000);
	DEBUGSTR(debug);

}
//******************************************************
// Fin definición funciones
//******************************************************



//******************************************************
// Comienzo Handlers de Interrupciones
//******************************************************

//Handler TEC_1 
void GPIO0_IRQHandler(void){
	int i;

    //Debug
    sprintf_mio(debug, "GPIO0_IRQHandler - Valor de IST = 0x%X\r\n", GPIO_PIN_INT->IST);
	DEBUGSTR(debug);
	
    // Se limpia IST porque si no quedaría la interrupción activa y no ejecutaría el código del handler
	GPIO_PIN_INT->IST = (1 << 0); 

    if(TEC_1 == 1){
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
            tono_suma[i] = tono_suma[i] + tono_2_khz[i]; 
            //sprintf_mio(debug,"\r\nvalor seno: %d\n", tono_suma[i]);
	        //DEBUGSTR(debug);

      }  
      Secuencia_Tecla(TEC_1,0);
      TEC_1 = 0;
    }else{
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
          tono_suma[i] = tono_suma[i] - tono_2_khz[i]; 
      }
      Secuencia_Tecla(TEC_1,0);
      TEC_1 = 1;
      //LED_OFF(1);
    }

}

//Handler TEC_2
void GPIO1_IRQHandler(void){
	int i;

    //Debug
    sprintf_mio(debug, "GPIO1_IRQHandler - Valor de IST = 0x%X\r\n", GPIO_PIN_INT->IST);
	DEBUGSTR(debug);
	
    // Se limpia IST porque si no quedaría la interrupción activa y no ejecutaría el código del handler
	GPIO_PIN_INT->IST = (1 << 1); 

    if(TEC_2 == 1){
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
            tono_suma[i] = tono_suma[i] + tono_4_khz[i]; 
      }  
      Secuencia_Tecla(TEC_2,1);
      TEC_2 = 0;
    }else{
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
          tono_suma[i] = tono_suma[i] - tono_4_khz[i]; 
      }
      Secuencia_Tecla(TEC_2,1);
      TEC_2 = 1;
    }

}

//Handler TEC_3
void GPIO2_IRQHandler(void){
	int i;

    //Debug
    sprintf_mio(debug, "GPIO2_IRQHandler - Valor de IST = 0x%X\r\n", GPIO_PIN_INT->IST);
	DEBUGSTR(debug);
	
    // Se limpia IST porque si no quedaría la interrupción activa y no ejecutaría el código del handler
	GPIO_PIN_INT->IST = (1 << 2); 

    if(TEC_3 == 1){
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
            tono_suma[i] = tono_suma[i] + tono_8_khz[i]; 
      } 
      Secuencia_Tecla(TEC_3,2); 
      TEC_3 = 0;
    }else{
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
          tono_suma[i] = tono_suma[i] - tono_8_khz[i]; 
      }
      Secuencia_Tecla(TEC_3,2);
      TEC_3 = 1;
    }

}

//Handler TEC_4 
void GPIO3_IRQHandler(void){
	int i;

    //Debug
    sprintf_mio(debug, "GPIO3_IRQHandler - Valor de IST = 0x%X\r\n", GPIO_PIN_INT->IST);
	DEBUGSTR(debug);
	
    // Se limpia IST porque si no quedaría la interrupción activa y no ejecutaría el código del handler
	GPIO_PIN_INT->IST = (1 << 3); 

    if(TEC_4 == 1){
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
            tono_suma[i] = tono_suma[i] + tono_16_khz[i]; 
      }  
      Secuencia_Tecla(TEC_4,3);
      TEC_4 = 0;
    }else{
      for(i=0;i<WAVE_SAMPLE_NUM;i++){
          tono_suma[i] = tono_suma[i] - tono_16_khz[i]; 
      }
      Secuencia_Tecla(TEC_4,3);
      TEC_4 = 1;
    }

}

//Interrupción del DMA
void DMA_IRQHandler(void) {
	_GPDMA->INTTCCLEAR = 1;
    int i;
    if (_GPDMA->CH[0].LLI.NEXT == (unsigned int)&LLI0) {
        for(i=0; i < WAVE_SAMPLE_NUM; i++) {
            tono_buffer_1[i] = tono_suma[i];
        }
    }
    else {
        for(i=0; i < WAVE_SAMPLE_NUM; i++) {
            tono_buffer_2[i] = tono_suma[i];
        }
    }
}
//******************************************************
// Fin Handlers de Interrupciones
//******************************************************
