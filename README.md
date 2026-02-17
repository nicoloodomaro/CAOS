# Timeline Scheduler (Kernel-Level Integration)

Questo README descrive come e fatto il timeline scheduler nel progetto, come e integrato nel kernel FreeRTOS, e dove verificare i test.

## Scopo

Il modulo timeline introduce una schedulazione time-triggered sopra FreeRTOS:
- major frame ciclico
- subframe fissi
- task HRT (hard real-time) con finestra [start, end)
- task SRT (soft real-time) eseguite nel tempo libero

## File e responsabilita

- `App/config/timeline_config.c`
- Definisce task demo, timing del frame e `gTimelineConfig`.

- `App/include/timeline_scheduler.h`
- API pubblica del timeline scheduler.
- Definizioni di strutture dati e trace event.

- `App/config/timeline_scheduler.c`
- Implementazione core: validate, release, deadline, select next task, trace.

- `App/include/timeline_kernel_hooks.h`
- Interfaccia hook verso kernel.

- `App/config/timeline_kernel_hooks.c`
- Bridge tra kernel e scheduler timeline.

- `FreeRTOS_copy/Source/tasks.c`
- Punti di integrazione nel kernel (start, tick, select).

- `App/app_main.c`
- Monitor task UART che legge trace e stampa sequenza frame (`[TL-SEQ]`).

## Strutture dati principali

### `TimelineTaskConfig_t` (`App/include/timeline_scheduler.h`)

Campi:
- `pcName`: nome task
- `pxTaskCode`: funzione task reale
- `xType`: `TIMELINE_TASK_HRT` o `TIMELINE_TASK_SRT`
- `ulSubframeId`: subframe target per HRT
- `ulStartOffsetMs`: offset start nella subframe
- `ulEndOffsetMs`: offset deadline/fine finestra nella subframe
- `uxPriority`: priorita FreeRTOS per task wrapper
- `usStackDepthWords`: stack in word

### `TimelineConfig_t` (`App/include/timeline_scheduler.h`)

Campi:
- `ulMajorFrameMs`: durata major frame
- `ulSubframeMs`: durata subframe
- `pxTasks`: array configurazione task
- `ulTaskCount`: numero task

### `TimelineTaskRuntime_t` (`App/include/timeline_scheduler.h`)

Campi runtime per ogni task:
- `xHandle`
- `ulReleaseCount`
- `ulCompletionCount`
- `ulDeadlineMissCount`
- `xIsActive`
- `xCompletedInWindow`
- `xDeadlineMissPendingKill`

### `TimelineTraceEvent_t` (`App/include/timeline_scheduler.h`)

Evento trace:
- `xTick`, `ulFrameId`, `ulSubframeId`, `uxTaskIndex`, `xType`

## Flusso di esecuzione

1. `vTaskStartScheduler()` nel kernel invoca `vTimelineKernelHookSchedulerStart(...)`.
2. Il bridge (`timeline_kernel_hooks.c`) configura scheduler (`xTimelineSchedulerConfigure`) e crea task wrapper (`xTimelineSchedulerCreateManagedTasks`).
3. A ogni tick, il kernel invoca `vTimelineKernelHookTick(...)`.
4. Il timeline (`vTimelineSchedulerOnTickFromISR`) rilascia HRT/SRT, rileva deadline miss, gestisce rollover frame.
5. In context switch, kernel invoca `xTimelineKernelHookSelectTask(...)`.
6. `xTimelineSchedulerSelectNextTask(...)` puo sovrascrivere la scelta standard e imporre HRT attiva nella finestra.

## Funzioni principali e ruolo

- `xTimelineSchedulerConfigure(...)`
- Valida config e inizializza stato interno.

- `xTimelineSchedulerCreateManagedTasks()`
- Crea un wrapper task per ogni entry config (`xTaskCreate`) e lo sospende.

- `vTimelineSchedulerKernelStart(...)`
- Inizializza inizio frame e stato runtime all avvio kernel.

- `vTimelineSchedulerOnTickFromISR(...)`
- Core temporale:
- calcola posizione nel frame
- release HRT quando `xTickInSubframe == start`
- marca miss quando fuori finestra
- rilascia SRT solo se nessun HRT attivo

- `xTimelineSchedulerSelectNextTask(...)`
- Priorita logica:
- prima HRT attiva in finestra
- poi prima SRT attiva
- altrimenti fallback alla scelta standard FreeRTOS

- `vTimelineSchedulerTaskCompletedFromTaskContext(...)`
- Marca completion e rilascia subito la prossima SRT in ordine, se possibile.

- `ulTimelineSchedulerTraceRead(...)`
- Espone trace ring buffer al monitor.

## Integrazione kernel: punti esatti

In `FreeRTOS_copy/Source/tasks.c`:
- start hook: `vTimelineKernelHookSchedulerStart(...)`
- tick hook: `vTimelineKernelHookTick(...)`
- select hook: `xTimelineKernelHookSelectTask(...)`

Motivazione:
- senza hook, FreeRTOS decide solo per priorita
- con hook, il timeline puo far rispettare finestre HRT in modo deterministico

## Test e verifica nel progetto

### Scenario demo configurato

In `App/config/timeline_config.c` e presente un profilo di test HRT/SRT.

Nota: alcuni commenti del profilo non sono allineati ai valori correnti (esempio tempi HRT_A/HRT_C). Per verifiche affidarsi ai valori numerici della tabella task.

### Dove vedere il comportamento runtime

- `App/app_main.c` task `prvTimelineMonitorTask`
- legge trace da `ulTimelineSchedulerTraceRead(...)`
- stampa via UART la sequenza per frame/subframe (`[TL-SEQ] ...`)

### Come eseguire

Da `App/Makefile`:
- build: `make`
- run QEMU: `make run`
- debug QEMU: `make debug`

## Opzione avanzata: hook tick dal port

Nel repository esiste supporto opzionale `TIMELINE_CALL_FROM_PORT` in alcuni `port.c` (es. template e ARM_CM35P). Nel target attuale (`ARM_CM3` nel Makefile), il path usato e quello in `tasks.c`.

## Limiti e note operative

- Deadline miss: mark in ISR, delete/recreate demandata a path safe non-ISR (timer daemon callback).
- SRT ordinate in ordine compile-time (validato in `prvValidateConfig`).
- HRT nella stessa subframe non devono sovrapporsi (validazione config).
- Precisione dipende da tick rate (`configTICK_RATE_HZ`) e latenza ISR del target.

## Riferimenti interni

- `TIMELINE_KERNEL_INTEGRATION_REPORT.md`
- `TIMELINE_KERNEL_APPENDIX.md`
