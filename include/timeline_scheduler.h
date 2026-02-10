#ifndef TIMELINE_SCHEDULER_H
#define TIMELINE_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include la definizione delle strutture dati di configurazione */
#include "timeline_config.h"

/**
 * @brief Avvia lo Scheduler basato su Timeline (Time-Triggered).
 * * Questa funzione:
 * 1. Inizializza le strutture dati interne dello scheduler.
 * 2. Crea il task "Orchestrator" ad alta priorità che gestisce il tempo.
 * 3. Avvia il kernel di FreeRTOS (vTaskStartScheduler).
 * * @note Questa funzione non ritorna mai, a meno che non ci sia 
 * memoria insufficiente per creare l'Idle task.
 * * @param pxConfig Puntatore alla configurazione globale (Major Frame e lista Task).
 */
void vStartTimelineScheduler(const TimelineConfig_t *pxConfig);

/**
 * @brief Ottiene il tempo corrente relativo all'inizio del Major Frame.
 * * Utile se un task HRT vuole sapere quanto tempo gli rimane prima della deadline.
 * * @return uint32_t Tempo in millisecondi dall'inizio del frame attuale.
 */
uint32_t ulGetTimelineCurrentTime(void);

/**
 * @brief Funzione di hook per gestire le "Deadline Miss".
 * * Questa funzione viene chiamata dall'Orchestrator se un task HRT
 * non termina entro il suo slot temporale e deve essere terminato forzatamente.
 * Può essere implementata dall'utente per loggare l'errore.
 * * @param pcTaskName Nome del task che ha fallito.
 */
void vApplicationDeadlineMissedHook(const char *pcTaskName);

#ifdef __cplusplus
}
#endif

#endif /* TIMELINE_SCHEDULER_H */
