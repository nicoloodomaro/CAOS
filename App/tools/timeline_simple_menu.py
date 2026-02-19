#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
APP_DIR = SCRIPT_DIR.parent
GENERATED_DIR = APP_DIR / "generated"
SPEC_PATH = GENERATED_DIR / "timeline_problem.json"
GENERATED_C_PATH = GENERATED_DIR / "timeline_config_generated.c"


def _read_int(prompt: str, minimum: int = 0, default: Optional[int] = None) -> int:
    while True:
        suffix = f" [{default}]" if default is not None else ""
        raw = input(f"{prompt}{suffix}: ").strip()
        if raw == "" and default is not None:
            value = default
        else:
            try:
                value = int(raw)
            except ValueError:
                print("Inserisci un numero intero valido.")
                continue
        if value < minimum:
            print(f"Il valore deve essere >= {minimum}.")
            continue
        return value


def _read_name(prompt: str) -> str:
    while True:
        raw = input(f"{prompt}: ").strip()
        if raw:
            return raw
        print("Il nome non puo' essere vuoto.")


def _load_spec(path: Path) -> Optional[Dict[str, Any]]:
    if not path.exists():
        return None
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None
    if not isinstance(raw, dict):
        return None
    return raw


def _validate_spec(spec: Dict[str, Any]) -> Tuple[bool, List[str]]:
    errors: List[str] = []
    major = spec.get("major_frame_ms")
    sub = spec.get("subframe_ms")
    tasks = spec.get("tasks")

    if not isinstance(major, int) or major <= 0:
        errors.append("major_frame_ms deve essere un intero > 0")
    if not isinstance(sub, int) or sub <= 0:
        errors.append("subframe_ms deve essere un intero > 0")
    if isinstance(major, int) and isinstance(sub, int) and major > 0 and sub > 0:
        if (major % sub) != 0:
            errors.append("major_frame_ms deve essere multiplo di subframe_ms")

    if not isinstance(tasks, list) or len(tasks) == 0:
        errors.append("tasks deve essere una lista non vuota")
        return False, errors

    if isinstance(tasks, list) and len(tasks) > 32:
        errors.append("tasks deve avere al massimo 32 elementi")

    subframes = (major // sub) if isinstance(major, int) and isinstance(sub, int) and sub > 0 else 0
    found_srt = False
    names = set()

    for idx, task in enumerate(tasks):
        prefix = f"tasks[{idx}]"
        if not isinstance(task, dict):
            errors.append(f"{prefix} deve essere un oggetto")
            continue

        name = task.get("name")
        typ = task.get("type")
        execution = task.get("execution_ms")
        subframe_id = task.get("subframe_id")
        start_ms = task.get("start_ms")
        end_ms = task.get("end_ms")

        if not isinstance(name, str) or not name.strip():
            errors.append(f"{prefix}.name non valido")
        elif name in names:
            errors.append(f"{prefix}.name duplicato: {name}")
        else:
            names.add(name)

        if typ not in ("HRT", "SRT"):
            errors.append(f"{prefix}.type deve essere HRT o SRT")
            continue

        if typ == "SRT":
            found_srt = True
        elif found_srt:
            errors.append(f"{prefix}: HRT deve comparire prima di SRT")

        if not isinstance(execution, int) or execution <= 0:
            errors.append(f"{prefix}.execution_ms deve essere > 0")

        if not isinstance(subframe_id, int) or subframe_id < 0:
            errors.append(f"{prefix}.subframe_id non valido")
        elif typ == "HRT" and subframe_id >= subframes:
            errors.append(f"{prefix}.subframe_id fuori range 0..{max(subframes - 1, 0)}")

        if not isinstance(start_ms, int) or start_ms < 0:
            errors.append(f"{prefix}.start_ms non valido")
        if not isinstance(end_ms, int) or end_ms < 0:
            errors.append(f"{prefix}.end_ms non valido")

        if typ == "HRT":
            if isinstance(start_ms, int) and isinstance(end_ms, int):
                if end_ms <= start_ms:
                    errors.append(f"{prefix}: serve end_ms > start_ms")
                if isinstance(sub, int) and end_ms > sub:
                    errors.append(f"{prefix}: end_ms deve essere <= subframe_ms")

    return len(errors) == 0, errors


def _analyze_spec(spec: Dict[str, Any]) -> Tuple[bool, List[str]]:
    ok, validation_errors = _validate_spec(spec)
    if not ok:
        return False, validation_errors

    major = spec["major_frame_ms"]
    sub = spec["subframe_ms"]
    tasks = spec["tasks"]

    subframe_count = major // sub
    hrt_per_subframe = [0 for _ in range(subframe_count)]
    total_hrt = 0
    total_srt = 0
    failures: List[str] = []

    for task in tasks:
        typ = task["type"]
        execution = task["execution_ms"]
        if typ == "HRT":
            sub_id = task["subframe_id"]
            start = task["start_ms"]
            end = task["end_ms"]
            window = end - start
            total_hrt += execution
            hrt_per_subframe[sub_id] += execution
            if execution > window:
                failures.append(
                    f"HRT '{task['name']}': execution {execution} ms > window {window} ms"
                )
        else:
            total_srt += execution

    for sub_id, demand in enumerate(hrt_per_subframe):
        if demand > sub:
            failures.append(
                f"Subframe {sub_id}: domanda HRT {demand} ms > {sub} ms"
            )

    details: List[str] = []
    slack = major - total_hrt
    details.append(f"HRT totale: {total_hrt} ms su frame {major} ms")
    details.append(f"SRT totale: {total_srt} ms su slack nominale {slack} ms")
    if total_srt > slack:
        details.append(
            "Attenzione: SRT totale oltre lo slack nominale, possibili carry-over/miss."
        )

    if failures:
        return False, failures + details
    return True, details


def _identifier(raw: str, used: set[str]) -> str:
    base = re.sub(r"[^0-9A-Za-z_]", "_", raw)
    if not base:
        base = "Task"
    if base[0].isdigit():
        base = f"T_{base}"
    candidate = f"vGeneratedTask_{base}"
    suffix = 1
    while candidate in used:
        candidate = f"vGeneratedTask_{base}_{suffix}"
        suffix += 1
    used.add(candidate)
    return candidate


def _c_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\"", "\\\"")


def _generate_c_source(spec: Dict[str, Any], source_hint: str) -> str:
    tasks = spec["tasks"]
    used_ids: set[str] = set()
    function_names: List[str] = []
    for task in tasks:
        function_names.append(_identifier(task["name"], used_ids))

    out: List[str] = []
    out.append('#include "FreeRTOS.h"')
    out.append('#include "task.h"')
    out.append('#include "timeline_config.h"')
    out.append("")
    out.append("/* Auto-generated file. Do not edit manually. */")
    out.append(f"/* Source spec: {source_hint} */")
    out.append("")
    out.append("static void vBusyWaitMs(const TimelineTaskExecutionInfo_t * pxExecInfo, uint32_t ulDurationMs)")
    out.append("{")
    out.append("    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);")
    out.append("")
    out.append("    if ((pxExecInfo == NULL) || (xTargetTicks == 0U)) {")
    out.append("        return;")
    out.append("    }")
    out.append("")
    out.append("    while (xTimelineSchedulerGetTaskExecutedTicks(*pxExecInfo) < xTargetTicks) {")
    out.append("        taskYIELD();")
    out.append("    }")
    out.append("}")
    out.append("")

    for task, fn in zip(tasks, function_names):
        out.append(f"static void {fn}(void * pvArg)")
        out.append("{")
        out.append("    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;")
        out.append(f"    vBusyWaitMs(pxExecInfo, {task['execution_ms']}U);")
        out.append("}")
        out.append("")

    out.append("static const TimelineTaskConfig_t xTasks[] = {")
    for task, fn in zip(tasks, function_names):
        type_token = "TIMELINE_TASK_HRT" if task["type"] == "HRT" else "TIMELINE_TASK_SRT"
        out.append(
            "    { "
            f"\"{_c_escape(task['name'])}\", {fn}, {type_token}, "
            f"{task['subframe_id']}U, {task['start_ms']}U, {task['end_ms']}U, "
            f"tskIDLE_PRIORITY + {task.get('priority_offset', 1)}U, {task.get('stack_words', 256)}U "
            "},"
        )
    out.append("};")
    out.append("")
    out.append("const TimelineConfig_t gTimelineConfig = {")
    out.append(f"    .ulMajorFrameMs = {spec['major_frame_ms']}U,")
    out.append(f"    .ulSubframeMs = {spec['subframe_ms']}U,")
    out.append("    .pxTasks = xTasks,")
    out.append("    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))")
    out.append("};")
    out.append("")
    return "\n".join(out)


def _print_analysis_result(ok: bool, lines: List[str]) -> None:
    if ok:
        print("Analisi timeline: PASSED")
    else:
        print("Analisi timeline: FAILED")
    for line in lines:
        print(f" - {line}")


def _create_or_update_config() -> None:
    print("\nCreazione configurazione timeline (semplice)")

    major_frame_ms = _read_int("Major frame (ms)", minimum=1)
    subframe_ms = _read_int("Subframe (ms)", minimum=1)
    while major_frame_ms % subframe_ms != 0:
        print("major_frame deve essere multiplo di subframe.")
        subframe_ms = _read_int("Subframe (ms)", minimum=1)

    subframe_count = major_frame_ms // subframe_ms
    hrt_count = _read_int("Numero task HRT", minimum=0)
    srt_count = _read_int("Numero task SRT", minimum=0)
    while (hrt_count + srt_count) == 0:
        print("Serve almeno un task.")
        hrt_count = _read_int("Numero task HRT", minimum=0)
        srt_count = _read_int("Numero task SRT", minimum=0)

    tasks: List[Dict[str, int | str]] = []

    for idx in range(hrt_count):
        print(f"\nHRT {idx + 1}/{hrt_count}")
        name = _read_name("Nome")
        execution_ms = _read_int("Execution time (ms)", minimum=1)
        subframe_id = _read_int("Subframe id", minimum=0)
        while subframe_id >= subframe_count:
            print(f"Subframe id non valido. Range: 0..{subframe_count - 1}")
            subframe_id = _read_int("Subframe id", minimum=0)
        start_ms = _read_int("Start offset (ms)", minimum=0)
        end_ms = _read_int("End offset (ms)", minimum=1)
        while end_ms <= start_ms or end_ms > subframe_ms:
            print(f"Finestra non valida: serve start < end <= {subframe_ms}")
            start_ms = _read_int("Start offset (ms)", minimum=0)
            end_ms = _read_int("End offset (ms)", minimum=1)

        task = {
            "name": name,
            "type": "HRT",
            "execution_ms": execution_ms,
            "subframe_id": subframe_id,
            "start_ms": start_ms,
            "end_ms": end_ms,
            "priority_offset": 4,
            "stack_words": 256,
        }
        tasks.append(task)

    for idx in range(srt_count):
        print(f"\nSRT {idx + 1}/{srt_count}")
        name = _read_name("Nome")
        execution_ms = _read_int("Execution time (ms)", minimum=1)
        task = {
            "name": name,
            "type": "SRT",
            "execution_ms": execution_ms,
            "subframe_id": 0,
            "start_ms": 0,
            "end_ms": 0,
            "priority_offset": 1,
            "stack_words": 256,
        }
        tasks.append(task)

    spec = {
        "major_frame_ms": major_frame_ms,
        "subframe_ms": subframe_ms,
        "analysis": {
            "algorithm": "timeline",
        },
        "tasks": tasks,
    }

    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    SPEC_PATH.write_text(json.dumps(spec, indent=2) + "\n", encoding="utf-8")
    print(f"\nSpec salvata in: {SPEC_PATH}")

    print("\nAnalisi...")
    ok, lines = _analyze_spec(spec)
    _print_analysis_result(ok, lines)

    print("\nGenerazione C...")
    if ok:
        GENERATED_C_PATH.write_text(
            _generate_c_source(spec, source_hint=str(SPEC_PATH)),
            encoding="utf-8",
        )
        print(f"Config C generata in: {GENERATED_C_PATH}")
        print("\nPer usarla:")
        print(" - make cleanobjs all PROFILE=12 TL_DEBUG=1")
    else:
        print("Generazione annullata: correggi prima gli errori.")


def _analyze_existing() -> None:
    spec = _load_spec(SPEC_PATH)
    if spec is None:
        print(f"Spec non trovata: {SPEC_PATH}")
        return
    ok, lines = _analyze_spec(spec)
    _print_analysis_result(ok, lines)


def _regenerate_existing() -> None:
    spec = _load_spec(SPEC_PATH)
    if spec is None:
        print(f"Spec non trovata: {SPEC_PATH}")
        return
    ok, lines = _analyze_spec(spec)
    _print_analysis_result(ok, lines)
    if not ok:
        print("Rigenerazione annullata: correggi prima gli errori.")
        return
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    GENERATED_C_PATH.write_text(
        _generate_c_source(spec, source_hint=str(SPEC_PATH)),
        encoding="utf-8",
    )
    print(f"Config C rigenerata in: {GENERATED_C_PATH}")


def _print_build_help() -> None:
    print("\nComandi utili:")
    print(" - make cleanobjs all PROFILE=7 TL_DEBUG=1            # profili manuali 1..11")
    print(" - make cleanobjs all PROFILE=12 TL_DEBUG=1           # config generata dal menu")
    print(" - make run-single TEST=12 TL_DEBUG=1                 # run rapido profilo generato")


def main() -> int:
    while True:
        print("\n=== Timeline Config Menu ===")
        print("1) Crea/Aggiorna nuova configurazione")
        print("2) Analizza configurazione esistente")
        print("3) Rigenera file C dalla spec esistente")
        print("4) Mostra comandi build/run")
        print("5) Esci")
        choice = input("Scelta: ").strip()

        if choice == "1":
            _create_or_update_config()
        elif choice == "2":
            _analyze_existing()
        elif choice == "3":
            _regenerate_existing()
        elif choice == "4":
            _print_build_help()
        elif choice == "5":
            return 0
        else:
            print("Scelta non valida.")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrotto.")
        sys.exit(130)
