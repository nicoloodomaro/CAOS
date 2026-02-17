Appendice: Differenze pratiche tra `tasks.c` e `port.c`

Questa appendice spiega in modo diretto e pratico le differenze tra i due file FreeRTOS che abbiamo modificato e perché scegliere una o l'altra posizione per chiamare il tick hook influisce su latenza, portabilità e sicurezza.

- `FreeRTOS/FreeRTOS/Source/tasks.c` (kernel core)
  - Ruolo: incremento del tick (`xTaskIncrementTick()`), gestione liste di delay, e selezione del prossimo task (`vTaskSwitchContext()`).
  - Modifica: inserita la chiamata a `vTimelineKernelHookTick()` dopo l'incremento del tick e l'hook di selezione del task.
  - Pro: portabile (lavora con tutte le porting di FreeRTOS), semplice da testare.
  - Contro: leggermente più tardi rispetto all'ISR del port (ma comunque con jitter molto basso). Usare quando si preferisce compatibilità.

- `FreeRTOS/FreeRTOS/Source/portable/.../port.c` (layer di porting)
  - Ruolo: codice hardware-specifico che riceve per primo l'interrupt timer (es. `SysTick_Handler`) e implementa primitive di yield da ISR.
  - Opzione: chiamare `vTimelineKernelHookTick()` direttamente nell'ISR del port (`TIMELINE_CALL_FROM_PORT`).
  - Pro: latenza minima — il timeline vede il tick immediatamente all'ingresso nell'ISR (utile per jitter estremamente stringenti).
  - Contro: meno portabile (ogni port richiede adattamento), deve usare solo API *FromISR* e rimanere molto breve.

Nota su doppie invocazioni
Se abiliti la chiamata dal `port.c` devi evitare la chiamata anche in `tasks.c` per lo stesso tick. Per questo motivo abbiamo introdotto la guardia:

```c
#ifndef TIMELINE_CALL_FROM_PORT
    vTimelineKernelHookTick( xConstTickCount, &xSwitchRequired );
#endif
```

Raccomandazione pratica
- Per sviluppo e test iniziali: usare la chiamata in `tasks.c` (più semplice, portabile).
- Per target con requisiti di jitter molto stretti: abilitare `TIMELINE_CALL_FROM_PORT` sul port specifico e misurare il comportamento su hardware reale.
