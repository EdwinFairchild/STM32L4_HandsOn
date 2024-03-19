#include "user_app.h"
#include "math.h"
#define USE_LL_DRIVER 0
uCShell_type cli;
extern LPTIM_HandleTypeDef hlptim1;
extern UART_HandleTypeDef huart2;
extern DAC_HandleTypeDef hdac1;
extern DMA_HandleTypeDef hdma_dac_ch1;
// flag used to indicate we printing to console via DMA
// not the shell/cli printf
bool DMA_TX_complete = true;

#define DWT_BASE        0xE0001000
#define DWT_CONTROL     *(volatile unsigned long *)(DWT_BASE + 0x00)
#define DWT_CYCCNT      *(volatile unsigned long *)(DWT_BASE + 0x04)
uint32_t start_dma;
uint32_t end_dma;
uint32_t start;
uint32_t end;
#define DWT_CONTROL_CYCCNTENA_BIT (1UL << 0)

void enableDWT(void) {
    // Enable the use of DWT
    if (!(DWT_CONTROL & DWT_CONTROL_CYCCNTENA_BIT)) {
        DWT_CONTROL |= DWT_CONTROL_CYCCNTENA_BIT; // Enable cycle counter
    }
}

uint8_t paragraph[] = "\
1.This is a paragraph of text that will be printed to the terminal.\r\n \
2.This is a paragraph of text that will be printed to the terminal.\r\n \
3.This is a paragraph of text that will be printed to the terminal.\r\n \
4.This is a paragraph of text that will be printed to the terminal.\r\n \
5.This is a paragraph of text that will be printed to the terminal.\r\n \
6.This is a paragraph of text that will be printed to the terminal.\r\n \
7.This is a paragraph of text that will be printed to the terminal.\r\n \
8.This is a paragraph of text that will be printed to the terminal.\r\n \
9.This is a paragraph of text that will be printed to the terminal.\r\n \
10.This is a paragraph of text that will be printed to the terminal.\r\n \
11.This is a paragraph of text that will be printed to the terminal.\r\n \
12.This is a paragraph of text that will be printed to the terminal.\r\n \
13.This is a paragraph of text that will be printed to the terminal.\r\n \
14.This is a paragraph of text that will be printed to the terminal.\r\n \
15.This is a paragraph of text that will be printed to the terminal.\r\n \
16.This is a paragraph of text that will be printed to the terminal.\r\n \
17.This is a paragraph of text that will be printed to the terminal.\r\n \
18.This is a paragraph of text that will be printed to the terminal.\r\n \
19.This is a paragraph of text that will be printed to the terminal.\r\n \
20.This is a paragraph of text that will be printed to the terminal.\r\n \
21.This is a paragraph of text that will be printed to the terminal.\r\n \
22.This is a paragraph of text that will be printed to the terminal.\r\n \
23.This is a paragraph of text that will be printed to the terminal.\r\n \
24.This is a paragraph of text that will be printed to the terminal.\r\n \
25.This is a paragraph of text that will be printed to the terminal.\r\n \
26.This is a paragraph of text that will be printed to the terminal.\r\n \
27.This is a paragraph of text that will be printed to the terminal.\r\n \
28.This is a paragraph of text that will be printed to the terminal.\r\n \
29.This is a paragraph of text that will be printed to the terminal.\r\n \
30.This is a paragraph of text that will be printed to the terminal.\r\n \
31.This is a paragraph of text that will be printed to the terminal.\r\n \
32.This is a paragraph of text that will be printed to the terminal.\r\n \
33.This is a paragraph of text that will be printed to the terminal.\r\n \
34.This is a paragraph of text that will be printed to the terminal.\r\n \
35.This is a paragraph of text that will be printed to the terminal.\r\n \
36.This is a paragraph of text that will be printed to the terminal.\r\n \
37.This is a paragraph of text that will be printed to the terminal.\r\n \
38.This is a paragraph of text that will be printed to the terminal.\r\n \
39.This is a paragraph of text that will be printed to the terminal.\r\n \
40.This is a paragraph of text that will be printed to the terminal.\r\n ";

#define WAVE_SIZE 1024
uint8_t sinceWave[WAVE_SIZE];
#define DAC_MAX_VALUE 4095 // digital to analog converter max value
#define TWO_PI  (2 * M_PI)

uint16_t sineWave[WAVE_SIZE]; // Changed to uint16_t to accommodate larger values

void fillSinWave(void){
    for(int i = 0; i < WAVE_SIZE; i++){
        sineWave[i] = (uint16_t)((DAC_MAX_VALUE / 2) * (1 + sin(TWO_PI * i / WAVE_SIZE)));
    }
}
//----------------------------------------------------------------
void cmd_ok_handler(uint8_t num, char *values[])
{
    printf("System ok!\r\n");
}
//---------------------------------------------------------------- 
void wakeFromSleep(void)
{
    // have to reinit things after waking up from Stop modes 
    // because upon wake up the clock is HSI
    HAL_RCC_DeInit(); // reset RCC to known state
    SystemClock_Config(); // reconfig RCC
    MX_GPIO_Init(); //reinit GPIO
    MX_USART2_UART_Init(); // reinit UART
    HAL_ResumeTick();
    HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);
    printf(" Woke up from sleep\r\n");
}
//----------------------------------------------------------------
void cmd_SleepStop_1(uint8_t num, char *values[]){
    uint32_t mode = (uint32_t)atoi(values[0]);
    uint32_t sleep = (uint32_t)atoi(values[1]);
    printf("Sleeping for %d seconds\r\n", sleep);
    // go to sleep mode
    HAL_SuspendTick();
    NVIC_ClearPendingIRQ(SysTick_IRQn); //just to be safe
        HAL_LPTIM_TimeOut_Start_IT(&hlptim1, 65535, sleep * 8000);
    if(mode == 1)
        HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);
    else
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    wakeFromSleep();
} 
//----------------------------------------------------------------
void rtc_handler(void){
    // RTC interrupt handler
    printf("!!! RTC : Interrupt !!!\r\n");
}
//----------------------------------------------------------------
void lp_handler(void){
    // RTC interrupt handler
    printf("!!! LP : Interrupt !!!\r\n");
}
//----------------------------------------------------------------
void cmd_startRTC(uint8_t num, char *values[]){
    // TODO: set an RTC alarm for n seconds
    uint32_t sleep = atoi(values[0]);
    printf("Alarm set for %d seconds\r\n", sleep);
}
//----------------------------------------------------------------
int _write(int file, char *ptr, int len){
    //
    for(int i = 0 ; i < len ; i++){
        #if USE_LL_DRIVER == 1
        LL_USART_TransmitData8(USART2, ptr[i]);
        while(!LL_USART_IsActiveFlag_TXE(USART2));
        #else
        HAL_UART_Transmit(&huart2, (uint8_t*)&ptr[i], 1, 0xFFFF);
        #endif
    }
    return len;
}
//----------------------------------------------------------------
void cmd_moveData(uint8_t num, char *values[]){
    // output sine wave to UART via DMA
    DMA_TX_complete = false;

    #if USE_LL_DRIVER == 1
    LL_USART_EnableDMAReq_TX(USART2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)sineWave);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, WAVE_SIZE);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_7);
    LL_USART_EnableDMAReq_TX(USART2);
    #else
    HAL_UART_Transmit_DMA(&huart2, (uint8_t*)sineWave, WAVE_SIZE);
    #endif
}
//----------------------------------------------------------------
void cmd_printParagraph(uint8_t num, char *values[]){
    // start the dwt in the core

    start = DWT->CYCCNT;
    printf("%s", paragraph);
    end = DWT->CYCCNT;
    
}
//----------------------------------------------------------------
void cmd_printParagraphDMA(uint8_t num, char *values[]){
    // get dwt time stamp to measure time
    
    // output sine wave to UART via DMA
    DMA_TX_complete = false;
    start_dma = DWT->CYCCNT;
    #if USE_LL_DRIVER == 1
    LL_USART_EnableDMAReq_TX(USART2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)paragraph);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, sizeof(paragraph));
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_7);
    LL_USART_EnableDMAReq_TX(USART2);
    #else
    HAL_UART_Transmit_DMA(&huart2, (uint8_t*)paragraph, sizeof(paragraph));
    #endif
    // get dwt time stamp to measure time
    
}
//----------------------------------------------------------------
void cmd_timeToPrint(uint8_t num, char *values[]){
    uint32_t difference = end - start;
    uint32_t timeInMsInt = (difference * 1000) / 80000000; // Integer part
    uint32_t timeInMsFrac = ((difference * 1000) % 80000000) * 1000 / 80000000; // Fractional part, scaled to 3 decimal places

    uint32_t difference_dma = end_dma - start_dma;
    uint32_t timeInMsInt_dma = (difference_dma * 1000) / 80000000; // Integer part
    uint32_t timeInMsFrac_dma = ((difference_dma * 1000) % 80000000) * 1000 / 80000000; // Fractional part, scaled to 3 decimal places

    printf("Time to print paragraph slow: %u cycles, %u.%03u ms\r\n", difference, timeInMsInt, timeInMsFrac);
    printf("Time to print paragraph fast: %u cycles, %u.%03u ms\r\n", difference_dma, timeInMsInt_dma, timeInMsFrac_dma);

}
//----------------------------------------------------------------
void cmd_sinWaveStart(uint8_t num, char *values[]){
    // output sine wave to DAC (configured to be circular)
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)sineWave, WAVE_SIZE, DAC_ALIGN_12B_R);
}
//----------------------------------------------------------------
void user_main(void){
    //compute sine wave
    fillSinWave();
    enableDWT();
    CL_cli_init(&cli, "uCShell:>", printf);
    #if USE_LL_DRIVER == 1
    LL_USART_EnableIT_RXNE(USART2);
    #else
    //kick off RX and enable interrupt
    HAL_UART_Receive_IT(&huart2, &cli.charReceived, 1);
    #endif
    cli.registerCommand("ok", ' ', cmd_ok_handler, "Prints \"ok\" if cli is ok", false);
    cli.registerCommand("sleep", ' ', cmd_SleepStop_1, "(sleep m n)Enter sleep mode m for n amount of seconds", false);
    cli.registerCommand("alarm", ' ', cmd_startRTC, "(alarm n)Set an RTC alarm n amount of seconds", false);
    cli.registerCommand("movedata", ' ', cmd_moveData, "Move data from mem to UART", false);
    cli.registerCommand("printslow", ' ', cmd_printParagraph, "Prints a paragraph of text", false);
    cli.registerCommand("printfast", ' ', cmd_printParagraphDMA, "Prints a paragraph of text via DMA", false);
    cli.registerCommand("stats", ' ', cmd_timeToPrint, "Prints time it took to print the paragraph", false);
    cli.registerCommand("sinwave", ' ', cmd_sinWaveStart, "Starts the sine wave", false);
    while(1){
        uCShell_run(&cli);
    }
}