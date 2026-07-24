/* Includes ------------------------------------------------------------------*/
#include "main.h"
//sys
#include "stdio.h"

//gui
#include "lvgl.h"
#include "ui.h"

#include "ui_HomePage.h"
#include "ui_MenuPage.h"
#include "ui_SetPage.h"
#include "ui_OffTimePage.h"
#include "ui_DateTimeSetPage.h"


uint8_t Sensor_LSM303_Erro=1;
uint8_t Sensor_AHT21_Erro=1;
uint8_t Sensor_SPL_Erro=1;
uint8_t Sensor_EM_Erro=1;
uint8_t Sensor_MPU_Erro=1;


#define LVGL_TICK 5

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

	
	//LVGL init
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
	ui_init();

  int tickets = 0;
  while (1) {
    // 先调用 lv_tick_inc 再调用 lv_task_handler
    // if (tickets % (TICK_RATE * LVGL_TICK)==0) {
    //   lv_tick_inc(LVGL_TICK);
    //   tickets = 0;
    // }
    lv_tick_inc(LVGL_TICK);
    lv_task_handler();
    usleep(1000);//5 ms
    // delay_ms(LVGL_TICK);
    tickets++;
  }
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
