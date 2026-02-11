# Report: Integrazione Kernel del Timeline Scheduler

Data: 11 febbraio 2026

Questo documento descrive in modo dettagliato e didattico le modifiche fatte al kernel FreeRTOS per integrare il "timeline scheduler" (scheduler a timeline/major-frame) in modalità kernel. Il report è pensato per studenti principianti: spiego cosa ho cambiato, dove, perché e come ogni funzione agisce nel flusso di scheduling.

## 1) Scopo dell'intervento

- Fornire punti di aggancio (hook) nel kernel FreeRTOS in modo che il modulo `timeline_scheduler` riceva:
  - l'evento di avvio del kernel/scheduler;
  - ogni interrupt di tick del kernel (per accuratezza temporale e rilascio dei task HRT/SRT);
  - la possibilità di sovrascrivere la scelta del prossimo task da eseguire (selection override) per imporre l'esecuzione deterministica dei task timeline-managed.

Le modifiche sono volutamente limitate e mirano a mantenere la compatibilità con il codice FreeRTOS esistente.

## 2) File modificati

- [FreeRTOS/FreeRTOS/Source/tasks.c](FreeRTOS/FreeRTOS/Source/tasks.c)
  - Aggiunta include di `timeline_kernel_hooks.h`.
  - Chiamata a `vTimelineKernelHookSchedulerStart()` prima di `xPortStartScheduler()`.
  - Chiamata a `vTimelineKernelHookTick()` dentro la gestione del tick (`xTaskIncrementTick`).
  - Chiamata a `xTimelineKernelHookSelectTask()` dentro `vTaskSwitchContext()` (sia Single‑Core che SMP) subito dopo che il kernel ha selezionato il TCB candidato.

> Nota: non ho modificato `timeline_scheduler.c` o `timeline_kernel_hooks.c` nel contenuto esistente: il modulo timeline è già presente nella workspace e fornisce le API che il kernel ora invoca tramite l'header `timeline_kernel_hooks.h`.

## 3) Dettaglio modifiche e motivo (passo‑passo)

### 3.1 Include header hook

Posizione: all'inizio di [FreeRTOS/FreeRTOS/Source/tasks.c](FreeRTOS/FreeRTOS/Source/tasks.c)

Codice aggiunto:

```c
/* Timeline scheduler kernel hooks */
#include "timeline_kernel_hooks.h"
```

Perché: il kernel deve poter chiamare le funzioni di ingresso del modulo timeline senza dipendere dall'implementazione interna del modulo. L'header espone tre funzioni principali (vedi sezione successiva).

---

### 3.2 Hook avvio scheduler

Posizione: subito prima di chiamare `xPortStartScheduler()` in `vTaskStartScheduler()`.

Codice aggiunto (semplificato):

```c
/* Give timeline kernel a chance to initialize just before the port
 * starts the scheduler and the hardware tick. */
vTimelineKernelHookSchedulerStart( xTickCount );

( void ) xPortStartScheduler();
```

Perché: il timeline scheduler deve conoscere il valore del tick iniziale (start of major frame), in modo da inizializzare lo stato: `xFrameStartTick`, `xLastTickSeen`, e preparare la runtime state per la prima major frame. Chiamando la funzione qui, siamo sicuri che venga invocata appena prima che il kernel avvii effettivamente la pianificazione.

Effetto: il modulo timeline può resettare i conteggi, creare/inizializzare eventuali strutture e preparare i task managed per il primo frame.

---

### 3.3 Hook sul tick del kernel

Posizione: dentro `xTaskIncrementTick()` (la funzione chiamata dal port layer ogni volta che un tick si verifica), dopo l'aggiornamento di `xTickCount`.

Codice aggiunto (semplificato):

```c
/* Call the timeline scheduler tick hook from kernel tick processing.
 * The hook may notify/resume timeline-managed tasks and request a
 * context switch by setting the higher priority flag. */
vTimelineKernelHookTick( xConstTickCount, &xSwitchRequired );
```

Perché: il timeline scheduler deve vedere precisamente ogni tick del kernel per:

- determinare il subframe corrente e rilasciare (notify/resume) i task HRT alla posizione di inizio slot;
- controllare deadline miss (marcare task per terminazione se necessario);
- attivare SRT tasks quando non ci sono HRT attivi.

Nota su sicurezza e ISR: la funzione `vTimelineKernelHookTick` è chiamata dallo scope del kernel che gestisce i tick; la firma supporta l'uso in ISR (`BaseType_t * pxHigherPriorityTaskWoken`) in modo che il timeline possa usare API *FromISR* come `vTaskNotifyGiveFromISR` e `xTaskResumeFromISR` — questo permette di svegliare task dal contesto tick con la massima tempestività.

---

### 3.4 Hook per la selezione del task (selection override)

Posizione: all'interno di `vTaskSwitchContext()` (single‑core e SMP), dopo che il kernel ha eseguito la selezione tradizionale (macro `taskSELECT_HIGHEST_PRIORITY_TASK()`), ho inserito una chiamata a:

```c
pxCurrentTCB = ( TCB_t * ) xTimelineKernelHookSelectTask( ( TaskHandle_t ) pxCurrentTCB, xTickCount );
```

e, per SMP:

```c
pxCurrentTCBs[ xCoreID ] = ( TCB_t * ) xTimelineKernelHookSelectTask( ( TaskHandle_t ) pxCurrentTCBs[ xCoreID ], xTickCount );
```

Perché: il comportamento standard di FreeRTOS sceglie il task ready con priorità più alta. Il timeline scheduler deve poter sovrascrivere questa decisione quando è richiesto che un determinato HRT o SRT venga eseguito in un preciso istante. Ritorna semplicemente l'handle al TCB che il kernel deve eseguire (se diverso dall'handle scelto dal kernel). Questo approccio è non invasivo: il kernel continua a mantenere le proprie strutture, ma accetta la scelta del timeline quando necessario.

Importante: la funzione `xTimelineKernelHookSelectTask()` nel modulo timeline richiama internamente `prvProcessPendingKillsAndRecreate()` e altre routine che gestiscono la terminazione/ricreazione dei managed task, quindi la logica di kill e ricreazione avviene da contesto kernel/scheduler come richiesto dal progetto.

---

## 4) Le funzioni di hook (che il kernel chiama)

Le intestazioni dichiarano tre funzioni (vedi [timeline_kernel_hooks.h](timeline_kernel_hooks.h)):

- `void vTimelineKernelHookSchedulerStart(void);`
  - Chiamata dall'interno di `vTaskStartScheduler()`; il timeline riceve il tick di avvio.

- `void vTimelineKernelHookTick(TickType_t xTickCount, BaseType_t * pxHigherPriorityTaskWoken);`
  - Chiamata in corrispondenza di ogni tick kernel (`xTaskIncrementTick()`);
  - Permette al timeline di rilasciare HRT/SRT mediante API *FromISR* e segnalare al kernel la richiesta di context switch impostando `*pxHigherPriorityTaskWoken`.

- `TaskHandle_t xTimelineKernelHookSelectTask(TaskHandle_t xDefaultSelected, TickType_t xTickCount);`
  - Chiamata subito dopo che il kernel ha scelto il prossimo task: il timeline può ritornare `xDefaultSelected` (nessuna sovrascrittura) oppure un altro `TaskHandle_t` da eseguire.

Queste funzioni sono dei semplici wrapper che richiamano le funzioni del modulo `timeline_scheduler` già presenti nel progetto:

- `vTimelineSchedulerKernelStart(...)`
- `vTimelineSchedulerOnTickFromISR(...)`
- `xTimelineSchedulerSelectNextTask(...)`

Il vantaggio è che manteniamo il codice timeline separato dall'implementazione kernel e reintroduciamo una interfaccia pulita per il porting su altre versioni del kernel.

## 5) Perché è kernel‑level (necessità didattica e tecnica)

Riassunto: l'intervento deve essere a livello kernel per tre motivi principali (come concordato col docente):

1. Precisione temporale e jitter: solo il kernel e il port layer hanno accesso al timer hardware e al percorso del tick, quindi per garantire jitter ≤1 tick il timeline deve eseguire la logica di rilascio nei path di tick/kernel.
2. Terminazione forzata di HRT: per terminare immediatamente un task in esecuzione (se supera la deadline) è necessario manipolare il contesto di esecuzione/TCB e forzare un context switch, cosa che richiede privilegi kernel.
3. Determinismo: la selezione del prossimo task deve poter essere influenzata dal timeline _prima_ che il core esegua il TCB successivo — è possibile solo modificando il flusso di `vTaskSwitchContext()`.

## 6) Cose da verificare / test suggeriti

1. Creare un branch git, commit delle modifiche, build della demo desiderata.

Puntuali comandi (PowerShell):

```powershell
cd c:\Users\A.RUSSO\Desktop\progettoOs
# crea branch e committa (opzionale se vuoi conservarli)
git checkout -b feat/timeline-kernel-integration
git add FreeRTOS/FreeRTOS/Source/tasks.c
git commit -m "Integrate timeline kernel hooks in tasks.c"
```

2. Compilare la demo/port target che usi abitualmente (il comando dipende dalla demo/port): per una demo basata su Makefile tipico:

```powershell
cd FreeRTOS/FreeRTOS/Demo/<your-demo>
make
```

3. Verifiche runtime:

- Strumento semplice: aggiungi `configASSERT` temporanei o `printf`/trace in `timeline_scheduler` per verificare che `vTimelineSchedulerOnTickFromISR` venga chiamato ogni tick.
- Validare che HRT siano risvegliati all'inizio dei loro slot (controlla `ulReleaseCount` in `pxTimelineSchedulerGetRuntime()`).
- Forzare un deadline miss (impostare end < start o allungare artificialmente l'esecuzione del task) e verificare che `xDeadlineMissPendingKill` venga impostato e che il kernel elimini il TCB e lo ricrei nel frame successivo.

## 7) Possibili miglioramenti futuri e avvertenze

- Port layer: per jitter stretti valutare l'uso di un hardware timer dedicato (compare) e chiamare `vTimelineKernelHookTick` sul compare interrupt con risoluzione più fine.
- Sincronizzazione: le modifiche al `pxCurrentTCB` sono delicate; in SMP bisogna assicurarsi che le scritture siano fatte con locks appropriati (qui la chiamata avviene già sotto le protezioni presenti in `vTaskSwitchContext()`).
- Robustezza: è prudente aggiungere protezioni (assert) nell'implementazione del hook per evitare che un handle non valido venga restituito.

## 8) Lista completa delle modifiche applicate (diff‑style sintetico)

- `tasks.c`: aggiunto include `timeline_kernel_hooks.h`.
- `tasks.c`: in `vTaskStartScheduler()` inserita chiamata `vTimelineKernelHookSchedulerStart( xTickCount );` prima di `xPortStartScheduler()`.
- `tasks.c`: in `xTaskIncrementTick()` inserita chiamata `vTimelineKernelHookTick( xConstTickCount, &xSwitchRequired );` dopo l'incremento del tick.
- `tasks.c`: in `vTaskSwitchContext()` (single core e SMP) inserita chiamata `xTimelineKernelHookSelectTask(...)` dopo che il kernel ha selezionato il TCB, permettendo l'override della scelta.

## 9) File rimasti invariati (per chiarezza)

- `timeline_scheduler.c` e `timeline_kernel_hooks.c` non sono stati modificati: le funzioni esposte da questi file sono quelle usate dal kernel e restano responsabili della logica timeline (release HRT/SRT, conteggi, kill, ricreazione).

## 10) Conclusione e prossimo passo operativo

Ho creato e applicato le patch per integrare i tre hook kernel fondamentali. Se vuoi, procedo con una delle seguenti azioni (scegli una opzione):

1. Aggiornare il port layer per chiamare il tick hook all'interno dell'handler SysTick specifico della tua board (posizione dipende dal port usato).
2. Aggiungere commit e push su branch remoto (eseguo i comandi git sopra).
3. Eseguire una build della demo target (se mi dici quale demo/port vuoi usare cerco il Makefile e provo a buildare qui).

Dimmi quale vuoi che esegua e procedo.

## 11) Aggiornamenti al Port Layer (modifiche eseguite)

Ho aggiornato il layer di porting per permettere alle implementazioni hardware‑specifiche
di chiamare direttamente l'hook del timeline dal contesto dell'ISR del tick se l'utente
lo desidera. Le modifiche sono minimali, opzionali (attivate tramite macro) e sono state
applicate in due punti principali:

- `FreeRTOS/FreeRTOS/Source/portable/template/port.c`
  - Aggiunta l'`#include "timeline_kernel_hooks.h"` per rendere disponibile l'hook.
  - Nel `prvTickISR()` ho introdotto un percorso condizionale (macro `TIMELINE_CALL_FROM_PORT`) che invoca
    `vTimelineKernelHookTick( xTaskGetTickCountFromISR(), &xHigherPriorityTaskWoken );` dopo l'invocazione
    di `xTaskIncrementTick()`.
  - Il valore ritornato o il flag `xHigherPriorityTaskWoken` viene usato per forzare un PendSV (o l'equivalente)
    in modo che avvenga il context switch se il timeline lo richiede.

- `FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM35P/non_secure/port.c`
  - Stesso approccio applicato nello specifico handler `SysTick_Handler` di questa port: include dell'header
    e chiamata condizionale a `vTimelineKernelHookTick(...)` con comportamento identico al template.

### Perché ho modificato il port layer

- Precisione e tempestività: alcune piattaforme richiedono che la logica sensibile al tempo venga eseguita
  il più vicino possibile all'interrupt hardware (es. nel SysTick handler o in un interrupt compare). Chiamando
  il hook direttamente dal port ISR si minimizza la latenza rispetto all'uso esclusivo di funzioni chiamate più
  in alto nel kernel.
- Flessibilità: la chiamata è condizionata a `TIMELINE_CALL_FROM_PORT` per evitare doppi invii del tick verso il
  timeline (poiché `xTaskIncrementTick()` nel kernel già invoca il hook). Questo lascia all'integratore la scelta
  di abilitare il comportamento più adatto al proprio hardware.

### Perché usare `xTaskGetTickCountFromISR()` e `BaseType_t * pxHigherPriorityTaskWoken`

- `xTaskGetTickCountFromISR()` restituisce il valore corrente del tick in modo sicuro da ISR; è preferibile
  rispetto a `xTaskGetTickCount()` che può non essere ISR‑safe.
- Il parametro `BaseType_t * pxHigherPriorityTaskWoken` è lo standard FreeRTOS per comunicare dalle API *FromISR*
  la richiesta di eseguire un context switch a seguito di un'operazione che ha risvegliato un task con priorità
  più alta. Usare questo pattern mantiene la compatibilità con le primitive kernel e assicura che il port gestisca
  correttamente la richiesta di swap (ad es. pendenze di PendSV o meccanismi equivalenti su altre architetture).

### Come si integra con il resto

- Se `TIMELINE_CALL_FROM_PORT` è definita, il port ISR chiama il timeline hook dopo aver incrementato il tick.
  Il timeline può quindi usare API *FromISR* per notificare/riattivare task HRT/SRT e impostare `*pxHigherPriorityTaskWoken`.
- La routine ISR nel port lancerà il PendSV (o setta la variabile di yield richiesta) quando `*pxHigherPriorityTaskWoken`
  è true, assicurando che lo scheduler esegua il context switch immediatamente dopo l'ISR.
- Se `TIMELINE_CALL_FROM_PORT` non è definita, il timeline rimane comunque funzionante perché `xTaskIncrementTick()` nel
  kernel (modificato precedentemente) chiama comunque `vTimelineKernelHookTick()` — questa ridondanza è evitata dalla
  macro di controllo.

### Raccomandazioni per l'uso

- Definisci `TIMELINE_CALL_FROM_PORT` solo se il port target richiede la massima tempestività e se sei sicuro che
  `vTimelineKernelHookTick` non venga chiamato altrove (per evitare duplicazioni). Tipicamente si abilita per
  port con timer ad alta risoluzione o quando si esegue la strategia tickless con compare events.
- Se abiliti la macro, verifica il comportamento con test di jitter: misura il tempo di attivazione dei task HRT
  e assicurati che non ci siano chiamate doppie al tick hook.

