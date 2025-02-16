/**
  ******************************************************************************
  * @file    usbd_audio_if.c
  * @brief   Implementacja interfejsu USB AUDIO z buforowaniem, dekodowaniem
  *          i przekazywaniem danych do SAI z wykorzystaniem DMA.
  ******************************************************************************
  * @attention
  *
  * Ta implementacja zakłada, że format danych przesyłanych przez USB jest PCM,
  * stereo, 16-bit, z częstotliwością 44100 Hz. Odebrane dane są zapisywane
  * w ring bufferze, a następnie przekazywane do interfejsu SAI przy użyciu
  * wyzwalanych callbacków DMA.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio_if.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include <string.h>         // dla memcpy
#include "sai.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Rozmiar ring buffera powinien być wielokrotnością rozmiaru bloku DMA */
#define AUDIO_RING_BUFFER_SIZE   4096U
/* Rozmiar bloku danych przekazywanego do SAI – zależy od ustawień DMA */
#define SAI_DMA_CHUNK_SIZE       512U

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Private_Macros */
/* USER CODE END Private_Macros */

/* Private variables ---------------------------------------------------------*/
/* Ring buffer na dane audio */
static uint8_t audioRingBuffer[AUDIO_RING_BUFFER_SIZE];
/* Indeks zapisu i odczytu */
static volatile uint32_t writeIndex = 0;
static volatile uint32_t readIndex  = 0;

/* Stały feedback dla 44100 Hz w formacie 10.14 (3 bajty, little-endian) */
static uint8_t feedbackData[3] = { 0x00, 0x11, 0x2B };

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Extern variables ----------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Private function prototypes -----------------------------------------------*/
static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options);
static int8_t AUDIO_DeInit_FS(uint32_t options);
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol);
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd);
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_GetState_FS(void);



/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

void SAI_Transmit_DMA(uint8_t *pData, uint16_t Size)
{
  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, pData, Size) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Funkcja wewnętrzna do wypełniania bufora DMA danymi z ring buffera */
static void SAI_Buffer_Fill(void)
{
  uint32_t available;
  if (writeIndex >= readIndex) {
    available = writeIndex - readIndex;
  } else {
    available = AUDIO_RING_BUFFER_SIZE - (readIndex - writeIndex);
  }

  if (available >= SAI_DMA_CHUNK_SIZE)
  {
    uint8_t dmaBuffer[SAI_DMA_CHUNK_SIZE];
    for (uint32_t i = 0; i < SAI_DMA_CHUNK_SIZE; i++)
    {
      dmaBuffer[i] = audioRingBuffer[readIndex];
      readIndex = (readIndex + 1U) % AUDIO_RING_BUFFER_SIZE;
    }
    SAI_Transmit_DMA(dmaBuffer, SAI_DMA_CHUNK_SIZE);
  }
}
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/* Struktura interfejsu USB AUDIO */
USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS =
{
  AUDIO_Init_FS,
  AUDIO_DeInit_FS,
  AUDIO_AudioCmd_FS,
  AUDIO_VolumeCtl_FS,
  AUDIO_MuteCtl_FS,
  AUDIO_PeriodicTC_FS,
  AUDIO_GetState_FS,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Inicjalizuje warstwę middleware AUDIO.
  * @param  AudioFreq: Zadana częstotliwość (oczekujemy 44100Hz)
  * @param  Volume: Początkowy poziom głośności
  * @param  options: Opcje inicjalizacji (zarezerwowane)
  * @retval USBD_OK w przypadku powodzenia, USBD_FAIL w przeciwnym
  */
static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options)
{
  /* USER CODE BEGIN AUDIO_Init_FS */
  (void)AudioFreq;
  (void)Volume;
  (void)options;
  /* Inicjalizacja ring buffera i indeksów */
  writeIndex = 0U;
  readIndex  = 0U;

  /* Tu można wywołać dodatkową inicjalizację SAI, np. konfigurację DMA.
     W tej podstawowej implementacji zakładamy, że SAI jest skonfigurowany osobno. */

  return (USBD_OK);
  /* USER CODE END AUDIO_Init_FS */
}

/**
  * @brief  De-inicjalizuje warstwę AUDIO.
  * @param  options: Opcje de-inicjalizacji (zarezerwowane)
  * @retval USBD_OK
  */
static int8_t AUDIO_DeInit_FS(uint32_t options)
{
  /* USER CODE BEGIN AUDIO_DeInit_FS */
  (void)options;
  return (USBD_OK);
  /* USER CODE END AUDIO_DeInit_FS */
}

/**
  * @brief  Obsługuje komendy audio przesyłane z hosta.
  *         Ta funkcja zapisuje odebrane dane do ring buffera, z którego będą
  *         pobierane kolejne bloki do transmisji przez SAI.
  * @param  pbuf: Wskaźnik do bufora z danymi audio
  * @param  size: Rozmiar danych (w bajtach)
  * @param  cmd: Polecenie (np. AUDIO_CMD_START, AUDIO_CMD_PLAY)
  * @retval USBD_OK
  */
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN AUDIO_AudioCmd_FS */
  (void)cmd;

  /* Sprawdzenie przepełnienia bufora */
  uint32_t spaceAvailable = (writeIndex >= readIndex) ?
                            (AUDIO_RING_BUFFER_SIZE - (writeIndex - readIndex)) :
                            (readIndex - writeIndex);

  if (size > spaceAvailable)
  {
    /* Przepełnienie bufora - można dodać obsługę błędu */
    return USBD_FAIL;
  }

  /* Kopiowanie danych do ring buffera */
  for (uint32_t i = 0; i < size; i++)
  {
    audioRingBuffer[writeIndex] = pbuf[i];
    writeIndex = (writeIndex + 1U) % AUDIO_RING_BUFFER_SIZE;
  }

  /* Wywołanie funkcji wypełniającej bufor DMA */
  SAI_Buffer_Fill();

  return (USBD_OK);
  /* USER CODE END AUDIO_AudioCmd_FS */
}

/**
  * @brief  Ustawia głośność odtwarzania.
  * @param  vol: Poziom głośności (0-100)
  * @retval USBD_OK
  */
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol)
{
  /* USER CODE BEGIN AUDIO_VolumeCtl_FS */
  (void)vol;
  return (USBD_OK);
  /* USER CODE END AUDIO_VolumeCtl_FS */
}

/**
  * @brief  Ustawia tryb mute.
  * @param  cmd: Komenda mute (0 = off, 1 = on)
  * @retval USBD_OK
  */
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd)
{
  /* USER CODE BEGIN AUDIO_MuteCtl_FS */
  (void)cmd;
  return (USBD_OK);
  /* USER CODE END AUDIO_MuteCtl_FS */
}

/**
  * @brief  Callback okresowej transmisji (Periodic Transfer Callback).
  *         W tej implementacji nie wykonujemy dodatkowych operacji, ale
  *         funkcja jest zachowywana dla zgodności z API.
  * @param  pbuf: Wskaźnik do bufora (nieużywany)
  * @param  size: Rozmiar danych (nieużywany)
  * @param  cmd: Kod operacji (nieużywany)
  * @retval USBD_OK
  */
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN AUDIO_PeriodicTC_FS */
  (void)pbuf;
  (void)size;
  (void)cmd;
  return (USBD_OK);
  /* USER CODE END AUDIO_PeriodicTC_FS */
}

/**
  * @brief  Zwraca aktualny stan interfejsu AUDIO.
  * @retval Stan audio (tutaj zawsze USBD_OK)
  */
static int8_t AUDIO_GetState_FS(void)
{
  /* USER CODE BEGIN AUDIO_GetState_FS */
  return (USBD_OK);
  /* USER CODE END AUDIO_GetState_FS */
}

/**
  * @brief  Callback wywoływany przy pełnym zakończeniu transmisji DMA.
  *         Po zakończeniu transmisji wywołuje funkcję uzupełniającą bufor SAI.
  */
void TransferComplete_CallBack_FS(void)
{
	  /* Aktualizacja feedbacku może być wykonywana osobno – tutaj najpierw
	     uzupełniamy blok DMA kolejnymi danymi z ring buffera */
	  SAI_Buffer_Fill();
	  /* Dodatkowo można tu dodać dynamiczne przeliczanie feedbacku */
	  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_FULL);
}

/**
  * @brief  Callback wywoływany przy zakończeniu połowy transmisji DMA.
  *         Analogicznie do pełnego transferu uzupełnia drugą połowę bufora.
  */
void HalfTransfer_CallBack_FS(void)
{
  SAI_Buffer_Fill();
  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_HALF);
}

/**
  * @brief  Aktualizuje dane feedback dla hosta.
  *         W tej implementacji feedback pozostaje stały – odpowiada 44100 Hz.
  */
void AUDIO_FeedbackUpdate_44100Hz(void)
{
  /* Ustawienie stałego feedbacku w formacie 10.14 (little-endian) */
  feedbackData[0] = 0x00;  /* LSB */
  feedbackData[1] = 0x11;
  feedbackData[2] = 0x2B;  /* MSB */

  /* Transmisja danych feedback przez odpowiedni endpoint IN (np. 0x81) */
  USBD_LL_Transmit(&hUsbDeviceFS, 0x81, feedbackData, 3);
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
