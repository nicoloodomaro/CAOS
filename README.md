# Timeline Scheduler (Kernel-Level Integration)

Questa implementazione separa chiaramente:

- modello di configurazione timeline (`timeline_config.*`)
- core scheduler deterministico (`timeline_scheduler.*`)
- bridge di integrazione nel kernel FreeRTOS (`timeline_kernel_hooks.*`)

## Obiettivo

Sostituire il comportamento puramente priority-based con uno schema time-triggered:

- major frame ciclico
- subframe fissi
- task Hard Real-Time con finestra start/deadline
- task Soft Real-Time in slack time

## File creati

- `timeline_scheduler.h`
- `timeline_scheduler.c`
- `timeline_kernel_hooks.h`
- `timeline_kernel_hooks.c`
- `timeline_config.h`
- `timeline_config.c`

## Logica principale

1. `xTimelineSchedulerConfigure(...)`
Valida la configurazione: frame validi, offset coerenti, numero task entro limiti.

2. `xTimelineSchedulerCreateManagedTasks()`
Crea tutti i task gestiti dal timeline scheduler e li sospende inizialmente.

3. `vTimelineSchedulerKernelStart(...)`
Imposta il tick iniziale del major frame e azzera lo stato runtime.

4. `vTimelineSchedulerOnTickFromISR(...)`
Su ogni tick:
- calcola posizione nel major frame
- rilascia HRT al loro `start`
- marca deadline miss quando oltre `end`
- rilascia SRT solo se nessun HRT e` attivo

5. `xTimelineSchedulerSelectNextTask(...)`
Hook chiamato dal kernel per scegliere il prossimo task:
- priorita` assoluta a HRT nella loro finestra valida
- poi SRT in ordine statico
- altrimenti fallback al task selezionato normalmente dal kernel

## Come integrarlo nel kernel FreeRTOS

In `FreeRTOS/Source/tasks.c` aggiungi chiamate ai hook nei punti chiave:

1. All'avvio scheduler:
- chiama `vTimelineKernelHookSchedulerStart( xTickCount )`

2. Nel tick increment (path ISR):
- chiama `vTimelineKernelHookTick(xTickCount, &xHigherPriorityTaskWoken)`

3. Nel punto in cui il kernel seleziona il prossimo task da eseguire:
- sostituisci la selezione finale con:
  - `pxSelected = xTimelineKernelHookSelectTask(pxSelected, xTickCount)`

## Note importanti

- La terminazione forzata per deadline miss e` gestita in modo deferred: il miss viene marcato in ISR, la delete avviene nel path kernel non-ISR.
- Questo schema evita operazioni non sicure in ISR.
- Per jitter <= 1 tick reale, serve timer HW/port layer coerente con i requisiti della board.
- Se l'applicazione non ha configurato esplicitamente il timeline (`xTimelineSchedulerConfigure`),
  l'hook di start effettua bootstrap automatico con `gTimelineConfig`.
