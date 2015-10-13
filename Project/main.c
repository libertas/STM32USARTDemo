#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_it.h"
#include "stm32f10x_tim.h"
#include <stdio.h>

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
	
	// Define Tx pin
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// Define Rx pin
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// Define output pin
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
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
	
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
	TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
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

uint8_t PWMState = 0;
uint16_t PWMHighTime = 5000;
uint16_t PWMLowTime = 5000;
uint16_t PWMTotal = 10000;

void USART1_IRQHandler(void)
{
	uint8_t data;
	double freq, duty;
	char buf[100] = {0};
	
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
			data = USART_ReceiveData(USART1);
			
			switch(data)
			{
				case 'w':
					PWMHighTime += 1000;
					PWMLowTime = PWMTotal - PWMHighTime;
					break;
				case 's':
					PWMHighTime -= 1000;
					PWMLowTime = PWMTotal - PWMHighTime;
					break;
				case 'e':
					PWMTotal /= 2;
					PWMHighTime /= 2;
					PWMLowTime /= 2;
					break;
				case 'd':
					PWMTotal *= 2;
					PWMHighTime *= 2;
					PWMLowTime *= 2;
					break;
				default:
					break;
			}
			
			USART_SendData(USART1, data);
			
			freq = 72000000UL / ((uint32_t) PWMHighTime + (uint32_t) PWMLowTime);
			duty = (double) PWMHighTime / ((double) PWMHighTime + (double) PWMLowTime);
			
			sprintf(buf, "\nFrequency:%lf Hz\n", freq);
			USART_Print(buf);
			
			sprintf(buf, "\nDucy ratio:%lf\n", duty);
			USART_Print(buf);
	}
}

void TIM2_IRQHandler(void)
{
	vu16 capture;
	capture = TIM_GetCapture1(TIM2);
	
	if(TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET)
	{
		if(PWMHighTime == PWMTotal)
		{
			GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
			TIM_SetCompare1(TIM2, capture - 1);
			PWMState = 1;
			goto end;
		}
		
		if(PWMLowTime == PWMTotal)
		{
			GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
			TIM_SetCompare1(TIM2, capture - 1);
			PWMState = 0;
			goto end;
		}
		
		if(PWMState)
		{
			GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
			TIM_SetCompare1(TIM2, capture + PWMHighTime);
		}
		else
		{
			GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_RESET);
			TIM_SetCompare1(TIM2, capture + PWMLowTime);
		}
		
		end:
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
		PWMState = !PWMState;
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
	}
	return 0;
}
