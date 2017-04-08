#include "stm32l4xx.h"
#include "main.h"
#include "bootloader.h"
#include "bsp_driver_sd.h"
#include "fatfs.h"

/* Variables -----------------------------------------------------------------*/
SD_HandleTypeDef        hsd1;
HAL_SD_CardInfoTypedef  SDCardInfo1;

/* Function prototypes -------------------------------------------------------*/
uint8_t VerifyChecksum(void);
uint8_t SDMMC1_Init(void);
void SDMMC1_DeInit(void);
void GPIO_Init(void);
void GPIO_DeInit(void);
void SystemClock_Config(void);
void Error_Handler(void);

int main(void)
{    
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    
    HAL_Delay(1000);
    
    if(IS_BTN_PRESSED())
    {
        puts("Entering Bootloader...\r\n");
        if(!SDMMC1_Init())
        {
            FIL fil;
            FRESULT fr;
            
            puts("SD found\r\n");
            FATFS_Init();
            mountSD();
            puts("SD mounted\r\n");
            
            fr = f_open(&fil, "image.bin", FA_READ);
            if(fr == FR_OK)
            {
                puts("Software found.\r\n");
                UINT num;
                uint64_t data;
                
                if(Bootloader_CheckSize( f_size(&fil) ) == BL_OK)
                {
                    uint32_t cntr = 0;
                    puts("App size OK.\r\n");
                    
                    puts("Flash erase starts...\r\n");
                    Bootloader_Erase();
                    puts("Flash erase finished.\r\n");
                    
                    puts("Start flashing...\r\n");
                    Bootloader_FlashInit();
                    
                    do
                    {
                        fr = f_read(&fil, &data, 8, &num);
                        if(num)
                        {
                            if(Bootloader_FlashNext(data) == BL_OK)
                            {
                                cntr++;
                            }
                        }
                    } while((fr == FR_OK) && (num > 0));                    
                    
                    Bootloader_FlashEnd();
                    puts("End.\r\n");
                }
                
                f_close(&fil);
            }
            else
            {
                fr = FR_OK;
            }
            
            unmountSD();
            puts("SD dismounted\r\n");
        }
    }
    
    /*if(!VerifyChecksum())
    {
        LED_Y_ON();
        Error_Handler();
    }*/
    
    if(Bootloader_CheckForApplication() == BL_OK)
    {
        puts("Application found, preparing for jump...\r\n");
        LED_G_ON();
        HAL_Delay(1000);
        LED_G_OFF();
        
        SDMMC1_DeInit();
        GPIO_DeInit();
        HAL_DeInit();
        
        Bootloader_JumpToApplication();        
    }

    while(1)
    {
        LED_R_TG();
        HAL_Delay(500);
    }
}

/*** SDIO *********************************************************************/
uint8_t SDMMC1_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 2;

    return (BSP_SD_Init());
}
void SDMMC1_DeInit(void)
{
    BSP_SD_DeInit();
}

void HAL_SD_MspInit(SD_HandleTypeDef* hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    if(hsd->Instance==SDMMC1)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_SDMMC1_CLK_ENABLE();

        /* SDMMC1 GPIO Configuration    
        PC8     ---> SDMMC1_D0
        PC9     ---> SDMMC1_D1
        PC10    ---> SDMMC1_D2
        PC11    ---> SDMMC1_D3
        PC12    ---> SDMMC1_CK
        PD2     ---> SDMMC1_CMD 
        */
        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef* hsd)
{
    if(hsd->Instance==SDMMC1)
    {
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    }
}

/*** Verify Application Checksum ***/
uint8_t VerifyChecksum(void)
{
#if USE_CHECKSUM
    CRC_HandleTypeDef CrcHandle;
    volatile uint32_t calculatedCrc = 0;
    
    __HAL_RCC_CRC_CLK_ENABLE();
    CrcHandle.Instance = CRC;
    CrcHandle.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
    CrcHandle.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
    CrcHandle.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_NONE;
    CrcHandle.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    CrcHandle.InputDataFormat              = CRC_INPUTDATA_FORMAT_WORDS;
    if(HAL_CRC_Init(&CrcHandle) != HAL_OK)
    {    
        return 0;
    }
    
    calculatedCrc = HAL_CRC_Calculate(&CrcHandle, (uint32_t*)APP_ADDRESS, APP_SIZE);
    
    __HAL_RCC_CRC_FORCE_RESET();
    __HAL_RCC_CRC_RELEASE_RESET();
    
    if( (*(uint32_t*)CRC_ADDRESS) != calculatedCrc)
    {
        return 0;
    }
#endif
    return 1;
}

/*** GPIO Configuration ***/
void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /*Configure GPIO pin output levels */
    HAL_GPIO_WritePin(LED_G_Port, LED_G_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_Y_Port, LED_Y_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_R_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BTN_Port, BTN_Pin, GPIO_PIN_SET);
    
    /* LED_G_Pin, LED_Y_Pin, LED_R_Pin */
    GPIO_InitStruct.Pin = LED_G_Pin | LED_Y_Pin | LED_R_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* User Button */
    GPIO_InitStruct.Pin = BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BTN_Port, &GPIO_InitStruct);
}
void GPIO_DeInit(void)
{
    HAL_GPIO_DeInit(LED_G_Port, LED_G_Pin);
    HAL_GPIO_DeInit(LED_Y_Port, LED_Y_Pin);
    HAL_GPIO_DeInit(LED_R_Port, LED_R_Pin);
    HAL_GPIO_DeInit(BTN_Port, BTN_Pin);
    
    __HAL_RCC_GPIOD_CLK_DISABLE();
    __HAL_RCC_GPIOE_CLK_DISABLE();
}

/*** System Clock Configuration ***/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 24;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USB
                              |RCC_PERIPHCLK_SDMMC1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLLSAI1;
  PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_MSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 24;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the main internal regulator output voltage 
    */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/*** HAL MSP init ***/
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
    HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
    HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
    HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
    HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
    HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
    while(1) 
    {
        LED_R_ON();
    }
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
    
}

#endif