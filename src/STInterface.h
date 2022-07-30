/*
 * STInterface.h
 *
 *  Created on: Jun 27, 2022
 */
#include "main.h"
#include<string.h>
#define MAX_SIZE 0x1000

/*STI Result States*/
#define R_NULL (STIResult)0x0;
#define R_FAILED (STIResult)0x1;
#define R_NOT_INITIALIZED (STIResult)0x2;
#define R_SUCCESS (STIResult)0xBEEF;

/*STI Command States*/
#define S_UPDATE (STICommand)0x8000
#define S_STRING (STICommand)0x4000
#define S_NULL (STICommand)0x0
/*STI MANAGER STATES*/
#define NOT_INITIALIZED 0
#define INITIALIZED 1
#define HAL_TIMEOUT 1000

#ifndef STINTERFACE_H
#define STINTERFACE_H


UART_HandleTypeDef *bluetoothInterface;
extern DMA_HandleTypeDef hdma_usart2_rx;
typedef unsigned short STICommand;
typedef unsigned short STIResult;
typedef unsigned char BYTE;
typedef unsigned char BOOL;
#define TRUE (BOOL)0x1
#define FALSE (BOOL)0x0
BYTE STI_STATE = NOT_INITIALIZED;
BOOL isSync = FALSE;
BYTE dataBuffer[MAX_SIZE];
uint32_t dcursor = 0x0;
void (*CMD_STRING_CALLBACK)(char* buffer,size_t length) = {0x0};

void sti_Setup_UART(UART_HandleTypeDef *bf);

uint32_t sti_used();
uint32_t sti_available();
STICommand sti_resolveCommand();

STIResult _sti_receiveACK();
STICommand _sti_receiveCommand();
size_t _sti_receiveString();
STIResult _sti_sendCommand(STICommand command);
STIResult _sti_sendString_(const char *strbuffer,size_t length);
STIResult _sti_sendString(const char *strbuffer);

STICommand lastCommand = R_NULL;
void sti_Setup_UART(UART_HandleTypeDef *bf){
	bluetoothInterface = bf;
	HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, RESET);
	HAL_UART_Receive_DMA(bluetoothInterface, dataBuffer, MAX_SIZE);
	STI_STATE= INITIALIZED;

}
void sti_SetCallback_STRING(void (*cs)(char* buffer,size_t length)){
	CMD_STRING_CALLBACK = cs;
}
BOOL fastSearch(void* needle,size_t needle_len,void* bale,size_t bale_len,size_t *offsetResult){
	size_t idx = 0;
	BYTE fs = *((BYTE*)(needle));
	BYTE ts = 0x00;
	while(idx < bale_len){
		while(ts != fs && idx < bale_len){
			ts = *(BYTE*)((uint32_t)bale+idx);
			idx++;
		}
		BYTE EqRes = 0x1;
		for (size_t j = 0; j < needle_len; ++j) {
			EqRes &= *(BYTE*)((uint32_t)bale+idx +j-1) == *(BYTE*)((uint32_t)needle+j);
		}
		if(EqRes > 0){
			idx = idx-1;
			break;
		}
		else{
			idx++;
		}
	}
	if(idx>=bale_len){
		return 0;
	}else{
		*offsetResult = idx;
		return 1;
	}
}
STIResult sti_syncSession(){
	if(isSync>0){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, RESET);
		return R_SUCCESS;
	};
	BYTE sign[4] =  {0xBE,0xEF,0xBE,0xEF};
	size_t offset = 0;
	isSync= fastSearch(&sign, 4, &dataBuffer, MAX_SIZE,&offset);
	if(!isSync){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		return R_FAILED;
		
	};
	dcursor = offset+4;
	HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, RESET);

}
STIResult sti_resolveCommand(){
	if(STI_STATE == NOT_INITIALIZED || isSync == 0){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		sti_syncSession();
		return S_NULL;
	}
	sti_syncSession();
	if(sti_available()>=2){
		lastCommand = *(STICommand *)((uint32_t)&dataBuffer+dcursor);
		dcursor +=2;
		if(lastCommand == S_STRING){
			size_t stringLength = *(size_t *)((uint32_t)&dataBuffer+dcursor);
			dcursor+=4;
			char *tmpbuffer;
			tmpbuffer = (char *)((uint32_t)&dataBuffer+dcursor);
			dcursor+=stringLength;
			
			if(CMD_STRING_CALLBACK!= 0x0){
				(*CMD_STRING_CALLBACK)(tmpbuffer,stringLength);
			}
			

		}
	}
	return S_NULL;
}
/*
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_GPIO_WritePin(BLUELED_GPIO_Port, BLUELED_Pin,SET);
	uint32_t sr = READ_REG(bluetoothInterface->Instance->SR),
			 cr = READ_REG(bluetoothInterface->Instance->CR1);
	if((sr & USART_SR_RXNE) != RESET && (cr & USART_CR1_RXNEIE) != RESET){
		if(lastCommand==S_STRING){
			size_t length = sti_receiveString();
			sti_sendString_(dataBuffer, length);
		}
	}

}*/
uint32_t sti_used(){
	return ((uint32_t)MAX_SIZE-(uint32_t)hdma_usart2_rx.Instance->NDTR);
}
uint32_t sti_available(){
	return sti_used()-dcursor;
}

STIResult _sti_receiveACK(){
	if(STI_STATE == NOT_INITIALIZED){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		return R_NOT_INITIALIZED;
	}
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,SET);
	STIResult result = 0x0;
	HAL_UART_Receive(bluetoothInterface, (uint8_t*)(&result), sizeof(STIResult), HAL_TIMEOUT);
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,RESET);
	return result;
}
STICommand _sti_receiveCommand(){
	if(STI_STATE == NOT_INITIALIZED){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		return R_NULL;
	}
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,SET);
	STICommand result = 0x0;
	HAL_UART_Receive(bluetoothInterface, (uint8_t*)&result, sizeof(STICommand), HAL_TIMEOUT);
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,RESET);
	return result;
}
size_t _sti_receiveString(){
	if(STI_STATE == NOT_INITIALIZED){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		return (size_t)0;
	}
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,SET);
	size_t length = 0x0;
	HAL_UART_Receive(bluetoothInterface, (uint8_t*)&length , sizeof(size_t), HAL_TIMEOUT);
	if(length<=MAX_SIZE){
		HAL_UART_Receive(bluetoothInterface,dataBuffer,length,HAL_TIMEOUT);
		HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,RESET);
		return length;
	}
	HAL_GPIO_WritePin(GREENLED_GPIO_Port,GREENLED_Pin,RESET);
	return (size_t)0;
}


STIResult _sti_sendCommand(STICommand command){
	if(STI_STATE == NOT_INITIALIZED){
		HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
		return R_NOT_INITIALIZED;
	}
	HAL_GPIO_WritePin(BLUELED_GPIO_Port,BLUELED_Pin,SET);
	HAL_UART_Transmit(bluetoothInterface, (uint8_t*)&command, sizeof(command), HAL_TIMEOUT);
	HAL_GPIO_WritePin(BLUELED_GPIO_Port,BLUELED_Pin,RESET);
	/*STIResult ACK = sti_receiveACK();*/
	return R_SUCCESS;
}
STIResult _sti_sendString_(const char *strbuffer,size_t length){
	if(STI_STATE == NOT_INITIALIZED){
			HAL_GPIO_WritePin(REDLED_GPIO_Port, REDLED_Pin, SET);
			return R_NOT_INITIALIZED;
	}
	HAL_GPIO_WritePin(ORANGELED_GPIO_Port,ORANGELED_Pin,SET);
	STICommand cmd = S_STRING;
	HAL_UART_Transmit(bluetoothInterface, (uint8_t*)&cmd, sizeof(STICommand), HAL_TIMEOUT);
	HAL_UART_Transmit(bluetoothInterface, (uint8_t*)&length, sizeof(size_t), HAL_TIMEOUT);
	HAL_UART_Transmit(bluetoothInterface, (uint8_t*)strbuffer, length, HAL_TIMEOUT);
	HAL_GPIO_WritePin(ORANGELED_GPIO_Port,ORANGELED_Pin,RESET);
	/*STIResult ACK = sti_receiveACK();*/
	return R_SUCCESS;
}
STIResult _sti_sendString(const char *strbuffer){
	return _sti_sendString_(strbuffer,strlen(strbuffer));
}


#endif /* STINTERFACE_H_ */
