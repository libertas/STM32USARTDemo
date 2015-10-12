#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_it.h"
#include "stm32f10x_tim.h"

void delay(void)
{
	uint8_t n = 0;
	while(--n);
}

void RCC_Config(void)
{
	RCC_DeInit();
	RCC_HSEConfig(RCC_HSE_ON);
	if(RCC_WaitForHSEStartUp() == SUCCESS)
	{
		RCC_HCLKConfig(RCC_SYSCLK_Div1);
		RCC_PCLK2Config(RCC_HCLK_Div1);
		RCC_PCLK1Config(RCC_HCLK_Div2);
		
		FLASH_SetLatency(FLASH_Latency_2);
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		RCC_PLLCmd(ENABLE);
		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		while(RCC_GetSYSCLKSource() != 0x08);
	}
	
	// Enable Timer2 clock
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	// Enable USART1 and GPIOA clock
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
}

void GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	// Define Tx pin and output pin
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// Define Rx pin
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void USART_Config(void)
{
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

void NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	/*
	#ifdef VECT_TAB_RAM
		NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0);
	#else
		NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
	#endif
	*/
	
	// Set up USART interrupt
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	// Set up timer2 interrupt
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void Timer_Config(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	
	TIM_TimeBaseStructure.TIM_Period = 65535;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse = 40000;
	
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
}

void USART_Print(char str[])
{
	while(*str != '\0')
	{
		USART_SendData(USART1, *str);
		while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
		str++;
	}
}

void USART1_IRQHandler(void)
{
	uint8_t data;
	
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
			data = USART_ReceiveData(USART1);
			
			if(data == '\n')
				USART_Print("\nReceiving Process Completed!\n");
			else if(data)
				USART_SendData(USART1, data);
	}
}

void TIM2_IRQHandler(void)
{
	vu16 capture = 0;
	if(TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET)
	{
		GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_4)));
		capture = TIM_GetCapture1(TIM2);
		TIM_SetCompare1(TIM2, capture + 40000);
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
	}
}

int main(void)
{
	RCC_Config();
	GPIO_Config();
	NVIC_Config();
	USART_Config();
	Timer_Config();
	
	while(1)
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_0);
	}
	return 0;
}
