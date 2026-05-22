/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : RFID + teclado + servo + botao - Nucleo-F303RE
  *                   Versao com maquina de estados
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PD */

#define PN532_I2C_ADDR        (0x24 << 1)

#define PN532_CMD_GETFIRMWAREVERSION   0x02
#define PN532_CMD_SAMCONFIGURATION     0x14
#define PN532_CMD_INLISTPASSIVETARGET  0x4A

#define LED_VERDE_PORT GPIOA
#define LED_VERDE_PIN  GPIO_PIN_5

#define LED_VERMELHO_PORT GPIOA
#define LED_VERMELHO_PIN  GPIO_PIN_6

#define BOTAO_PORT GPIOC
#define BOTAO_PIN  GPIO_PIN_13

#define SENHA_TAMANHO 4
#define TEMPO_SENHA_MS 10000

/*
 * Pinagem do teclado:
 *
 * Linha 1 -> PA0
 * Linha 2 -> PA1
 * Linha 3 -> PA4
 * Linha 4 -> PB0
 *
 * Coluna 1 -> PC0
 * Coluna 2 -> PC1
 * Coluna 3 -> PC2
 * Coluna 4 -> PC3
 */

#define LINHA1_PORT GPIOA
#define LINHA1_PIN  GPIO_PIN_0

#define LINHA2_PORT GPIOA
#define LINHA2_PIN  GPIO_PIN_1

#define LINHA3_PORT GPIOA
#define LINHA3_PIN  GPIO_PIN_4

#define LINHA4_PORT GPIOB
#define LINHA4_PIN  GPIO_PIN_0

#define COLUNA1_PORT GPIOC
#define COLUNA1_PIN  GPIO_PIN_0

#define COLUNA2_PORT GPIOC
#define COLUNA2_PIN  GPIO_PIN_1

#define COLUNA3_PORT GPIOC
#define COLUNA3_PIN  GPIO_PIN_2

#define COLUNA4_PORT GPIOC
#define COLUNA4_PIN  GPIO_PIN_3

/* USER CODE END PD */

/* USER CODE BEGIN PV */

uint8_t pn532_ok = 0;

uint8_t uid[10];
uint8_t uid_len = 0;

char senha_digitada[SENHA_TAMANHO + 1];

/*
 * Estado do servo:
 * 0 = fechado em 0 graus
 * 1 = aberto em 180 graus
 */
uint8_t servo_aberto = 0;

uint8_t uid_1[4] = {0xE2, 0xDA, 0x86, 0x06};
uint8_t uid_2[4] = {0xC2, 0x4F, 0x7B, 0x06};

char senha_uid_1[SENHA_TAMANHO + 1] = "1234";
char senha_uid_2[SENHA_TAMANHO + 1] = "4321";

/*
 * Maquina de estados do sistema.
 */
typedef enum
{
  ESTADO_ERRO_PN532 = 0,
  ESTADO_AGUARDANDO_CARTAO,
  ESTADO_DIGITANDO_SENHA,
  ESTADO_ACESSO_LIBERADO,
  ESTADO_ACESSO_NEGADO
} EstadoSistema;

EstadoSistema estado_atual = ESTADO_AGUARDANDO_CARTAO;

char *senha_esperada_atual = NULL;
uint8_t indice_senha = 0;
uint32_t tempo_inicio_senha = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);

/* USER CODE BEGIN PFP */

uint8_t PN532_CheckI2C(void);
uint8_t PN532_WaitReady(uint32_t timeout);
uint8_t PN532_ReadAck(void);
uint8_t PN532_SendCommand(uint8_t *cmd, uint8_t cmd_len);
uint8_t PN532_GetFirmwareVersion(void);
uint8_t PN532_SAMConfig(void);
uint8_t PN532_ReadPassiveTarget(uint8_t *uid_buffer, uint8_t *uid_length);

uint8_t CompararUID(uint8_t *uid_lido, uint8_t uid_lido_len, uint8_t *uid_cadastrado);
char* ObterSenhaDoUID(uint8_t *uid_lido, uint8_t uid_lido_len);

void LED_Verde_Blink(void);
void LED_Vermelho_Blink(void);
void LED_Ambos_Blink(void);

void Teclado_SetColunasHigh(void);
char Teclado_LerTecla(void);

void Servo_Iniciar(void);
void Servo_SetPulseUs(uint16_t pulse_us);
void Servo_SetAngle(uint8_t angle);

uint8_t Botao_Pressionado(void);
void AbrirPeloBotao(void);

void AcessoLiberado(void);
void AcessoNegado(void);

void MaquinaEstados_Executar(void);
void Estado_AguardandoCartao(void);
void Estado_DigitandoSenha(void);
void Estado_AcessoLiberado(void);
void Estado_AcessoNegado(void);
void Estado_ErroPN532(void);

/* USER CODE END PFP */

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();

  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_RESET);

  Servo_Iniciar();
  Servo_SetAngle(0);
  servo_aberto = 0;

  HAL_Delay(1000);

  if (PN532_CheckI2C())
  {
    if (PN532_GetFirmwareVersion())
    {
      if (PN532_SAMConfig())
      {
        pn532_ok = 1;

        for (int i = 0; i < 3; i++)
        {
          LED_Verde_Blink();
          HAL_Delay(200);
        }

        estado_atual = ESTADO_AGUARDANDO_CARTAO;
      }
    }
  }

  if (!pn532_ok)
  {
    for (int i = 0; i < 5; i++)
    {
      LED_Vermelho_Blink();
      HAL_Delay(200);
    }

    estado_atual = ESTADO_ERRO_PN532;
  }

  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */

    /*
     * Botao pode ser usado em qualquer estado.
     * Se a porta estiver fechada, abre.
     * Se a porta estiver aberta, fecha.
     */
    if (Botao_Pressionado())
    {
      AbrirPeloBotao();
    }

    MaquinaEstados_Executar();

    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_TIM1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 79;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 100;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;

  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);

  /*
   * PC0, PC1, PC2, PC3 -> colunas do teclado.
   */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*
   * PA5 -> LED verde
   * PA6 -> LED vermelho
   */
  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*
   * PA0, PA1, PA4 -> linhas 1, 2 e 3 do teclado.
   */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*
   * PB0 -> linha 4 do teclado.
   */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*
   * PC13 -> botão interno.
   * Pull-up:
   * solto = HIGH
   * pressionado = LOW
   */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

void MaquinaEstados_Executar(void)
{
  switch (estado_atual)
  {
    case ESTADO_ERRO_PN532:
      Estado_ErroPN532();
      break;

    case ESTADO_AGUARDANDO_CARTAO:
      Estado_AguardandoCartao();
      break;

    case ESTADO_DIGITANDO_SENHA:
      Estado_DigitandoSenha();
      break;

    case ESTADO_ACESSO_LIBERADO:
      Estado_AcessoLiberado();
      break;

    case ESTADO_ACESSO_NEGADO:
      Estado_AcessoNegado();
      break;

    default:
      estado_atual = ESTADO_AGUARDANDO_CARTAO;
      break;
  }
}

void Estado_ErroPN532(void)
{
  LED_Vermelho_Blink();
  HAL_Delay(500);
}

void Estado_AguardandoCartao(void)
{
  /*
   * Se a porta estiver aberta, nao permite:
   * - ler cartao;
   * - iniciar digitacao de senha;
   * - manter senha antiga salva.
   *
   * Enquanto a porta estiver aberta, somente o botao fecha.
   */
  if (servo_aberto == 1)
  {
    uid_len = 0;
    memset(uid, 0, sizeof(uid));

    memset(senha_digitada, 0, sizeof(senha_digitada));
    indice_senha = 0;
    senha_esperada_atual = NULL;

    HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_RESET);

    HAL_Delay(100);
    return;
  }

  uid_len = 0;
  memset(uid, 0, sizeof(uid));

  if (PN532_ReadPassiveTarget(uid, &uid_len))
  {
    senha_esperada_atual = ObterSenhaDoUID(uid, uid_len);

    if (senha_esperada_atual != NULL)
    {
      LED_Verde_Blink();

      memset(senha_digitada, 0, sizeof(senha_digitada));
      indice_senha = 0;
      tempo_inicio_senha = HAL_GetTick();

      estado_atual = ESTADO_DIGITANDO_SENHA;
    }
    else
    {
      estado_atual = ESTADO_ACESSO_NEGADO;
    }

    HAL_Delay(500);
  }
  else
  {
    HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
  }
}

void Estado_DigitandoSenha(void)
{
  char tecla;

  /*
   * Protecao extra:
   * se a porta abriu por algum motivo enquanto estava digitando senha,
   * cancela a digitacao e volta para aguardar cartao.
   */
  if (servo_aberto == 1)
  {
    memset(senha_digitada, 0, sizeof(senha_digitada));
    indice_senha = 0;
    senha_esperada_atual = NULL;

    estado_atual = ESTADO_AGUARDANDO_CARTAO;
    return;
  }

  if ((HAL_GetTick() - tempo_inicio_senha) > TEMPO_SENHA_MS)
  {
    estado_atual = ESTADO_ACESSO_NEGADO;
    return;
  }

  tecla = Teclado_LerTecla();

  if (tecla == 0)
  {
    HAL_Delay(20);
    return;
  }

  if (tecla >= '0' && tecla <= '9')
  {
    if (indice_senha < SENHA_TAMANHO)
    {
      senha_digitada[indice_senha] = tecla;
      indice_senha++;

      LED_Verde_Blink();
    }

    if (indice_senha == SENHA_TAMANHO)
    {
      senha_digitada[SENHA_TAMANHO] = '\0';

      if (strcmp(senha_digitada, senha_esperada_atual) == 0)
      {
        estado_atual = ESTADO_ACESSO_LIBERADO;
      }
      else
      {
        estado_atual = ESTADO_ACESSO_NEGADO;
      }
    }
  }
  else if (tecla == '*')
  {
    indice_senha = 0;
    memset(senha_digitada, 0, sizeof(senha_digitada));

    LED_Vermelho_Blink();
  }
  else if (tecla == '#')
  {
    senha_digitada[indice_senha] = '\0';

    if (strcmp(senha_digitada, senha_esperada_atual) == 0)
    {
      estado_atual = ESTADO_ACESSO_LIBERADO;
    }
    else
    {
      estado_atual = ESTADO_ACESSO_NEGADO;
    }
  }

  HAL_Delay(20);
}

void Estado_AcessoLiberado(void)
{
  AcessoLiberado();

  memset(senha_digitada, 0, sizeof(senha_digitada));
  indice_senha = 0;
  senha_esperada_atual = NULL;

  estado_atual = ESTADO_AGUARDANDO_CARTAO;

  HAL_Delay(1000);
}

void Estado_AcessoNegado(void)
{
  AcessoNegado();

  memset(senha_digitada, 0, sizeof(senha_digitada));
  indice_senha = 0;
  senha_esperada_atual = NULL;

  estado_atual = ESTADO_AGUARDANDO_CARTAO;

  HAL_Delay(1000);
}

uint8_t PN532_CheckI2C(void)
{
  if (HAL_I2C_IsDeviceReady(&hi2c1, PN532_I2C_ADDR, 3, 1000) == HAL_OK)
  {
    return 1;
  }

  return 0;
}

uint8_t PN532_WaitReady(uint32_t timeout)
{
  uint8_t status = 0;
  uint32_t start = HAL_GetTick();

  while ((HAL_GetTick() - start) < timeout)
  {
    if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, &status, 1, 100) == HAL_OK)
    {
      if (status == 0x01)
      {
        return 1;
      }
    }

    HAL_Delay(10);
  }

  return 0;
}

uint8_t PN532_ReadAck(void)
{
  uint8_t ack_buffer[7];

  if (!PN532_WaitReady(1000))
  {
    return 0;
  }

  if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, ack_buffer, 7, 1000) != HAL_OK)
  {
    return 0;
  }

  if (ack_buffer[1] == 0x00 &&
      ack_buffer[2] == 0x00 &&
      ack_buffer[3] == 0xFF &&
      ack_buffer[4] == 0x00 &&
      ack_buffer[5] == 0xFF &&
      ack_buffer[6] == 0x00)
  {
    return 1;
  }

  return 0;
}

uint8_t PN532_SendCommand(uint8_t *cmd, uint8_t cmd_len)
{
  uint8_t frame[32];
  uint8_t checksum = 0;
  uint8_t len = cmd_len + 1;

  if (len > 24)
  {
    return 0;
  }

  frame[0] = 0x00;
  frame[1] = 0x00;
  frame[2] = 0xFF;
  frame[3] = len;
  frame[4] = (uint8_t)(~len + 1);
  frame[5] = 0xD4;

  checksum += 0xD4;

  for (uint8_t i = 0; i < cmd_len; i++)
  {
    frame[6 + i] = cmd[i];
    checksum += cmd[i];
  }

  frame[6 + cmd_len] = (uint8_t)(~checksum + 1);
  frame[7 + cmd_len] = 0x00;

  if (HAL_I2C_Master_Transmit(&hi2c1, PN532_I2C_ADDR, frame, 8 + cmd_len, 1000) != HAL_OK)
  {
    return 0;
  }

  if (!PN532_ReadAck())
  {
    return 0;
  }

  return 1;
}

uint8_t PN532_GetFirmwareVersion(void)
{
  uint8_t cmd[1];
  uint8_t response[20];

  cmd[0] = PN532_CMD_GETFIRMWAREVERSION;

  if (!PN532_SendCommand(cmd, 1))
  {
    return 0;
  }

  if (!PN532_WaitReady(1000))
  {
    return 0;
  }

  if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, response, 20, 1000) != HAL_OK)
  {
    return 0;
  }

  for (uint8_t i = 0; i < 18; i++)
  {
    if (response[i] == 0xD5 && response[i + 1] == 0x03)
    {
      return 1;
    }
  }

  return 0;
}

uint8_t PN532_SAMConfig(void)
{
  uint8_t cmd[4];
  uint8_t response[16];

  cmd[0] = PN532_CMD_SAMCONFIGURATION;
  cmd[1] = 0x01;
  cmd[2] = 0x14;
  cmd[3] = 0x01;

  if (!PN532_SendCommand(cmd, 4))
  {
    return 0;
  }

  if (!PN532_WaitReady(1000))
  {
    return 0;
  }

  if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, response, 16, 1000) != HAL_OK)
  {
    return 0;
  }

  for (uint8_t i = 0; i < 14; i++)
  {
    if (response[i] == 0xD5 && response[i + 1] == 0x15)
    {
      return 1;
    }
  }

  return 0;
}

uint8_t PN532_ReadPassiveTarget(uint8_t *uid_buffer, uint8_t *uid_length)
{
  uint8_t cmd[3];
  uint8_t response[32];

  cmd[0] = PN532_CMD_INLISTPASSIVETARGET;
  cmd[1] = 0x01;
  cmd[2] = 0x00;

  if (!PN532_SendCommand(cmd, 3))
  {
    return 0;
  }

  if (!PN532_WaitReady(1000))
  {
    return 0;
  }

  if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, response, 32, 1000) != HAL_OK)
  {
    return 0;
  }

  for (uint8_t i = 0; i < 24; i++)
  {
    if (response[i] == 0xD5 && response[i + 1] == 0x4B)
    {
      uint8_t quantidade_targets = response[i + 2];

      if (quantidade_targets < 1)
      {
        return 0;
      }

      uint8_t tamanho_uid = response[i + 7];

      if (tamanho_uid > 10)
      {
        return 0;
      }

      *uid_length = tamanho_uid;

      for (uint8_t j = 0; j < tamanho_uid; j++)
      {
        uid_buffer[j] = response[i + 8 + j];
      }

      return 1;
    }
  }

  return 0;
}

uint8_t CompararUID(uint8_t *uid_lido, uint8_t uid_lido_len, uint8_t *uid_cadastrado)
{
  if (uid_lido_len != 4)
  {
    return 0;
  }

  for (uint8_t i = 0; i < 4; i++)
  {
    if (uid_lido[i] != uid_cadastrado[i])
    {
      return 0;
    }
  }

  return 1;
}

char* ObterSenhaDoUID(uint8_t *uid_lido, uint8_t uid_lido_len)
{
  if (CompararUID(uid_lido, uid_lido_len, uid_1))
  {
    return senha_uid_1;
  }

  if (CompararUID(uid_lido, uid_lido_len, uid_2))
  {
    return senha_uid_2;
  }

  return NULL;
}

void LED_Verde_Blink(void)
{
  HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_SET);
  HAL_Delay(150);
  HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_RESET);
}

void LED_Vermelho_Blink(void)
{
  HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_SET);
  HAL_Delay(150);
  HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_RESET);
}

void LED_Ambos_Blink(void)
{
  HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_SET);

  HAL_Delay(200);

  HAL_GPIO_WritePin(LED_VERDE_PORT, LED_VERDE_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_VERMELHO_PORT, LED_VERMELHO_PIN, GPIO_PIN_RESET);
}

void Teclado_SetColunasHigh(void)
{
  HAL_GPIO_WritePin(COLUNA1_PORT, COLUNA1_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(COLUNA2_PORT, COLUNA2_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(COLUNA3_PORT, COLUNA3_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(COLUNA4_PORT, COLUNA4_PIN, GPIO_PIN_SET);
}

char Teclado_LerTecla(void)
{
  char mapa[4][4] =
  {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
  };

  GPIO_TypeDef *coluna_port[4] =
  {
    COLUNA1_PORT,
    COLUNA2_PORT,
    COLUNA3_PORT,
    COLUNA4_PORT
  };

  uint16_t coluna_pin[4] =
  {
    COLUNA1_PIN,
    COLUNA2_PIN,
    COLUNA3_PIN,
    COLUNA4_PIN
  };

  GPIO_TypeDef *linha_port[4] =
  {
    LINHA1_PORT,
    LINHA2_PORT,
    LINHA3_PORT,
    LINHA4_PORT
  };

  uint16_t linha_pin[4] =
  {
    LINHA1_PIN,
    LINHA2_PIN,
    LINHA3_PIN,
    LINHA4_PIN
  };

  for (uint8_t coluna = 0; coluna < 4; coluna++)
  {
    Teclado_SetColunasHigh();

    HAL_GPIO_WritePin(coluna_port[coluna], coluna_pin[coluna], GPIO_PIN_RESET);

    HAL_Delay(2);

    for (uint8_t linha = 0; linha < 4; linha++)
    {
      if (HAL_GPIO_ReadPin(linha_port[linha], linha_pin[linha]) == GPIO_PIN_RESET)
      {
        HAL_Delay(30);

        if (HAL_GPIO_ReadPin(linha_port[linha], linha_pin[linha]) == GPIO_PIN_RESET)
        {
          while (HAL_GPIO_ReadPin(linha_port[linha], linha_pin[linha]) == GPIO_PIN_RESET)
          {
            HAL_Delay(10);
          }

          Teclado_SetColunasHigh();

          return mapa[linha][coluna];
        }
      }
    }
  }

  Teclado_SetColunasHigh();

  return 0;
}

void Servo_Iniciar(void)
{
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

void Servo_SetPulseUs(uint16_t pulse_us)
{
  if (pulse_us < 1000)
  {
    pulse_us = 1000;
  }

  if (pulse_us > 2000)
  {
    pulse_us = 2000;
  }

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse_us / 10);
}

void Servo_SetAngle(uint8_t angle)
{
  uint16_t pulse_us;

  if (angle > 180)
  {
    angle = 180;
  }

  pulse_us = 1000 + ((uint32_t)angle * 1000) / 180;

  Servo_SetPulseUs(pulse_us);
}

uint8_t Botao_Pressionado(void)
{
  static uint8_t botao_anterior = GPIO_PIN_SET;
  uint8_t botao_atual;

  /*
   * Com pull-up:
   * solto = GPIO_PIN_SET
   * pressionado = GPIO_PIN_RESET
   */
  botao_atual = HAL_GPIO_ReadPin(BOTAO_PORT, BOTAO_PIN);

  /*
   * Detecta a transicao:
   * antes estava solto e agora foi pressionado.
   */
  if (botao_anterior == GPIO_PIN_SET && botao_atual == GPIO_PIN_RESET)
  {
    HAL_Delay(30);

    if (HAL_GPIO_ReadPin(BOTAO_PORT, BOTAO_PIN) == GPIO_PIN_RESET)
    {
      botao_anterior = GPIO_PIN_RESET;
      return 1;
    }
  }

  /*
   * Quando soltar, arma novamente para detectar o proximo clique.
   */
  if (botao_atual == GPIO_PIN_SET)
  {
    botao_anterior = GPIO_PIN_SET;
  }

  return 0;
}

void AbrirPeloBotao(void)
{
  /*
   * Botao alterna:
   * se estiver fechado em 0 graus, abre para 180.
   * se estiver aberto em 180 graus, fecha para 0.
   */
  if (servo_aberto == 0)
  {
    Servo_SetAngle(180);
    servo_aberto = 1;
  }
  else
  {
    Servo_SetAngle(0);
    servo_aberto = 0;
  }

  for (int i = 0; i < 3; i++)
  {
    LED_Ambos_Blink();
    HAL_Delay(150);
  }
}

void AcessoLiberado(void)
{
  /*
   * Acesso liberado:
   * servo abre para 180 graus e permanece aberto.
   * So fecha quando o botao for pressionado.
   */
  Servo_SetAngle(180);
  servo_aberto = 1;

  for (int i = 0; i < 3; i++)
  {
    LED_Verde_Blink();
    HAL_Delay(150);
  }
}

void AcessoNegado(void)
{
  for (int i = 0; i < 3; i++)
  {
    LED_Vermelho_Blink();
    HAL_Delay(150);
  }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}

#endif
