#include "esp32_comm.h"

#if APP_ENABLE_SELF_TEST
#include "self_test.h"
#endif

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
#if APP_ENABLE_SELF_TEST
  SelfTest_OnSpiTxRxComplete(hspi);
#endif
  Esp32Comm_OnTxRxComplete(hspi);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
#if APP_ENABLE_SELF_TEST
  SelfTest_OnSpiError(hspi);
#endif
  Esp32Comm_OnError(hspi);
}
