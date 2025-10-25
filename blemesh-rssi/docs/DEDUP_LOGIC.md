# DEDUP_LOGIC.md

> BLE Mesh RSSI Localization System — Mother Node Duplicate Suppression Spec
>
> 본 문서는 Mother (Raspberry Pi) 수집기에서 수행되는 중복 데이터 제거 정책의 상세 규격을 정의한다.  
> 이 로직은 `ingest.py` 또는 동등한 모듈에서 실행되며, 수집된 `REPORT` 및 `ENTRY` 데이터의 중복 및 재전송 오류를 억제하여 신뢰도 있는 위치추정 데이터셋을 유지한다.

---

## 1. 개요

BLE Mesh 네트워크에서는 다음과 같은 중복 요인이 존재한다.

1. **릴레이 중복** — 동일 `VM_REPORT` 가 여러 경로로 전달될 수 있다.
2. **Proxy 중복** — 하나의 REPORT가 복수 Proxy를 경유할 수 있다.
3. **재송신 중복** — `TTL` 오차 또는 Mesh queue overflow로 같은 패킷이 다시 전송될 수 있다.

이를 억제하기 위해 Mother는 두 단계 키 체계로 디듑을 수행한다.

- **REPORT 중복 제거 단계 1:** `(reporter_id, report_seq)`
- **ENTRY 중복 제거 단계 2:** `(from_id, to_id, seq)`

---

## 2. 데이터 구조

### 2.1 REPORT Key Cache

```python
ReportKey = tuple[int, int]  # (reporter_id, report_seq)
report_seen: dict[ReportKey, float]  # value = 마지막 ingress_ts (epoch sec)
```

### **2.2 ENTRY Key Cache**

```
EntryKey = tuple[int, int, int]  # (from_id, to_id, seq)
entry_seen: dict[EntryKey, float]
```

### **2.3 정리 스케줄러**

```
def gc_sweep(now: float):
    for k, ts in list(report_seen.items()):
        if now - ts > REPORT_TTL_SEC:
            del report_seen[k]
    for k, ts in list(entry_seen.items()):
        if now - ts > ENTRY_TTL_SEC:
            del entry_seen[k]
```

---

## **3. 시간 창 및 보존 기간**

| **항목**   | **상수**        | **기본값(초)** | **설명**                   |
| ---------- | --------------- | -------------- | -------------------------- |
| REPORT TTL | REPORT_TTL_SEC  | 30             | REPORT 중복 기록 보존 시간 |
| ENTRY TTL  | ENTRY_TTL_SEC   | 5              | ENTRY 중복 기록 보존 시간  |
| GC 주기    | GC_INTERVAL_SEC | 1              | 주기적 정리 간격           |

> 모든 TTL은 수신 타임스탬프(ingress_ts) 기준으로 계산한다.

---

## **4. 처리 흐름**

### **4.1 라인 수신**

```
line = ingest_line_read(serial)
frame = ingest_parse(line)
if not frame:
    bad_file_append(line)
    return
```

### **4.2 REPORT 중복 검사**

```
key_r = (frame.report.reporter_id, frame.report.report_seq)
if key_r in report_seen:
    stats.dup_report += 1
    return
report_seen[key_r] = now
```

### **4.3 ENTRY 중복 검사**

```
for e in frame.report.entries:
    key_e = (e.from_id, e.to_id, e.seq)
    if key_e in entry_seen:
        stats.dup_entry += 1
        continue
    entry_seen[key_e] = now
    store_append(frame, now)
```

> REPORT 중복은 메시지 전체를 드랍하고,

> ENTRY 중복은 해당 엔트리만 스킵한다.

---

## **5. 로그 분류 정책**

- **정상 라인:** /var/log/evac-mesh/YYYY-MM-DD.log
- **파싱 실패:** /var/log/evac-mesh/YYYY-MM-DD.bad
- **백업 또는 롤오버 실패:** /var/log/evac-mesh/fallback.log

---

## **6. 예외 처리 규칙**

| **조건**                       | **동작**                                |
| ------------------------------ | --------------------------------------- |
| CBOR/JSON decode 실패          | .bad 파일에 원문 라인 기록              |
| 필드 누락                      | ingest_validate() 가 False 반환 후 드랍 |
| RSSI 범위 [-100, 0] 초과       | 경고 로그 후 값 clamp                   |
| n = 0                          | 엔트리 드랍                             |
| ingress_ts 역행 (시간 뒤로 감) | 로컬 시각으로 대체                      |

---

## **7. 통계 갱신**

```
def stats_update(frame: ReportFrame):
    stats.total_reports += 1
    stats.entries_total += len(frame.report.entries)
    stats.last_seen[frame.report.reporter_id] = frame.ingress_ts
```

헬스 출력(health_emit(period_s)) 예시:

```
[HEALTH] reports=1123 entries=8764 dup=3(0.26%) uptime=00:05:22
```

---

## **8. 성능 및 메모리 정책**

| **요소**          | **제한**                 | **근거**                                |
| ----------------- | ------------------------ | --------------------------------------- |
| REPORT cache 최대 | 4096 건                  | 30 초 TTL, 1 Hz \* 4k anchor 기준       |
| ENTRY cache 최대  | 8192 건                  | 5 초 TTL, anchor 2 k \* child 4 개 예상 |
| GC 시간           | < 10 ms @ Raspberry Pi 4 | O(n) dict 삭제 가능                     |
| 디스크 쓰기       | JSON line append 모드    | fsync 비활성 (버퍼링 기반)              |

---

## **9. 샘플 의사코드 (통합 루프)**

```
def main_loop():
    next_gc = time.time()
    while True:
        now = time.time()
        line = ingest_line_read(serial)
        if not line:
            if now >= next_gc:
                gc_sweep(now)
                next_gc = now + GC_INTERVAL_SEC
            continue
        frame = ingest_parse(line)
        if not frame or not ingest_validate(frame):
            bad_file_append(line)
            continue
        # REPORT dedup
        key_r = (frame.report.reporter_id, frame.report.report_seq)
        if key_r in report_seen:
            continue
        report_seen[key_r] = now
        # ENTRY dedup
        for e in frame.report.entries:
            key_e = (e.from_id, e.to_id, e.seq)
            if key_e in entry_seen:
                continue
            entry_seen[key_e] = now
            store_append(frame, now)
        stats_update(frame)
```

---

## **10. 디듑 충돌 및 에지 케이스**

| **상황**                     | **영향**                    | **대응**                             |
| ---------------------------- | --------------------------- | ------------------------------------ |
| seq wrap-around (32bit)      | 수 시간 후 충돌 가능        | report_seq 또는 ingress_ts 보조 검증 |
| 다중 Anchor 동시 보고        | 의도된 중복 (ENTRY 간 차이) | ENTRY dedup 단계 통과                |
| Proxy 중복 경유              | report_seq 같음             | REPORT dedup 단계 차단               |
| 시간 불일치 (Pi clock drift) | TTL 오류 가능               | NTP 동기화 추천                      |
| 파일 잠금 경합               | I/O 지연                    | append 모드 + line buffering 사용    |

---

## **11. 향후 개선 계획**

1. **Bloom Filter 캐시** 적용 → 메모리 절감.
2. **SQLite 백엔드** 옵션 → 디스크 지속성 보장.
3. **adaptive TTL** → 트래픽 부하 따라 REPORT/ENTRY TTL 조정.
4. **Duplicate Heatmap** 시각화 → 네트워크 루프 탐지 자동화.

---

## **12. 요약**

- REPORT 중복 키 = (reporter_id, report_seq)
- ENTRY 중복 키 = (from_id, to_id, seq)
- TTL = REPORT 30 s, ENTRY 5 s
- GC = 1 s 주기
- 중복 라인 드랍, 파싱 실패 .bad 분리, 정상 라인 JSON append
- 통계 출력 및 헬스 모니터링 내장

---

**문서 버전:** 1.0

**참조:** DESIGN_OVERVIEW.md, PROTOCOL_V1.md
