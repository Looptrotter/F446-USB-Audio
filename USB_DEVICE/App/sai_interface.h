#ifndef SAI_INTERFACE_H
#define SAI_INTERFACE_H

#include <stdint.h>

/**
  * @brief  Inicjuje transmisję danych audio do interfejsu SAI z wykorzystaniem DMA.
  * @param  pData: Wskaźnik do danych, które mają zostać przesłane.
  * @param  Size: Rozmiar danych do przesłania w bajtach.
  * @note   Funkcja powinna być zaimplementowana na podstawie specyfikacji sprzętowej.
  */
void SAI_Transmit_DMA(uint8_t *pData, uint16_t Size);

#endif // SAI_INTERFACE_H