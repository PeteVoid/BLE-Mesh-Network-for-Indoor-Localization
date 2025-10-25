# DESIGN_OVERVIEW.md

> BLE Mesh 기반 RSSI 수집·전송·추정 파이프라인의 전체 설계 개요서 (v1).  
> 본 문서는 시스템 구성, 데이터/제어 흐름, 시간·무선 파라미터, 모듈 경계, 장애·운영 전략, 품질 지표까지 한 곳에 정리한다.  
> 상세 프로토콜/필드 정의와 디듑 로직, 프로비저닝 절차, 로드맵은 각각 `PROTOCOL_V1.md`, `DEDUP_LOGIC.md`, `BLE_MESH_GUIDE.md`, `ROADMAP.md`를 참조.

---

## 0. 목표와 비전

- **목표**: ESP32 노드군(Child/Anchor/Proxy)과 Mother(Raspberry Pi)로 구성된 BLE Mesh 네트워크에서, Child의 광고(ADV) RSSI를 신뢰도 있는 통계로 수집·상신하여 Mother에서 실시간(1 Hz) 위치 추정이 가능하게 한다.
- **핵심 제약**:
  - 광고/스캔/Mesh 전송이 동일 2.4GHz 자원을 공유하므로 무선 슬롯 경합 최소화 필요.
  - 트래픽 폭주 방지를 위해 원시 샘플 대신 윈도(1s) 통계 요약만 전송.
  - 실내 환경의 반사·다중경로로 인한 RSSI 분산을 고려하여 IQR 기반 품질 측정 내장.
- **비전**: v1에서 안정적 수집·추정 파이프라인 완성 → v2에서 자율 릴레이 최적화, 필터 기반 추적 확장.

---

## 1. 역할과 책임

### 1.1 Child (ESP32)

- **주요 동작**
  - GAP Advertising 송신 (`ADV_INTERVAL = 200 ms`)
  - 필요 시 Mesh 수신·릴레이 수행(옵션)
- **전송 데이터**
  - `from_id`, `seq`, `txp_dbm`, `flags`, `crc8`
- **핵심 함수**
  - `slot_offset_compute()` : 광고 시간 분산
  - `ping_publish()` : 광고 패킷 생성 및 송신
- **책임**
  - `seq`를 1씩 증가시키며 원발신 이벤트 식별
  - `slot_offset_compute(node_id, slot_ms, slots)`로 충돌 완화

---

### 1.2 Anchor (ESP32)

- **주요 동작**
  - GAP 스캔으로 Child 광고 수신
  - 1초 윈도 단위로 RSSI 통계 계산
  - Mesh를 통해 `VM_REPORT` 전송
- **전송 데이터**
  - `entries[]` 배열에 `(from_id, to_id, kind, seq, rssi_med, p25, p75, n, txp_dbm, recv_ttl)`
- **핵심 함수**
  - `agg_update()` : RSSI 집계 버퍼 갱신
  - `agg_snapshot()` : median/p25/p75/n 계산
  - `report_publish()` : REPORT 메시지 전송
- **책임**
  - 이상치 억제 (`IQR`, `Hampel`)
  - REPORT 크기 제어 (엔트리 상한 또는 분할 전송)
  - 릴레이는 Anchor의 20–30%만 활성

---

### 1.3 Proxy / Provisioner (ESP32)

- **주요 동작**
  - Mesh REPORT 수신 → Mother로 USB CDC 브리지 (라인 단위 JSON/CBOR)
  - Provisioner로서 키·주소·그룹 주입, GATT Proxy 제공
- **핵심 함수**
  - `bridge_emit_json()` : REPORT를 JSON Lines로 내보내기
  - `bridge_read_ctrl_line()` : Mother로부터 CTRL 명령 수신 (나중)
- **책임**
  - REPORT 원문 보존 (데이터 변조 금지)
  - 포트 단절·복구 시 재시도 및 상태 로그 유지

---

### 1.4 Mother (Raspberry Pi)

- **주요 동작**
  - 직렬 수신 → 파싱/검증 → 디듑 → 파일 롤링 저장 → 헬스 통계
- **핵심 함수**
  - `ingest_line_read()`, `ingest_parse()`, `ingest_validate()`
  - `store_append()`, `store_rotate_if_needed()`
  - `stats_update()`, `health_emit()`
- **책임**
  - 디듑 수행: `(reporter_id, report_seq)` / `(from_id, to_id, seq)`
  - RSSI 범위 검증 및 `.bad` 파일 분리 저장
  - 수집률·중복률·가용성 모니터링

---

## 2. 데이터 모델

### 2.1 ADV Frame (Child → All)

| 필드         | 설명                        |
| ------------ | --------------------------- |
| `ver`        | 프로토콜 버전               |
| `company_id` | 제조사 코드 (0xFFFF 내부용) |
| `from_id`    | 발신 노드 ID                |
| `seq`        | 시퀀스 번호 (이벤트 ID)     |
| `txp_dbm`    | 송신 전력(dBm)              |
| `flags`      | 예약 비트                   |
| `crc8`       | 오류 검출용 CRC             |

- **역할**: `(from_id, seq)` 조합으로 이벤트 식별

### 2.2 REPORT (Anchor → Mesh)

| 필드          | 설명                          |
| ------------- | ----------------------------- |
| `ver`         | 버전 (현재 1)                 |
| `reporter_id` | 보고 노드 ID                  |
| `window_ms`   | 집계 윈도 크기 (기본 1000 ms) |
| `report_seq`  | 보고 메시지 자체 시퀀스       |
| `entries[]`   | 링크 통계 배열                |

- `entries[]` 필드:
  - `from_id`, `to_id`, `kind`, `seq`, `rssi_med`, `p25`, `p75`, `n`, `txp_dbm`, `recv_ttl`
- **역할**: 1초 윈도 동안 관측된 RSSI 통계 요약

### 2.3 Mother JSON Line

```json
{
  "ingress_ts": 1730112345678,
  "report": { ... }
}
```

- **역할**: 수집 시각 및 메타데이터 포함 저장

---

## **3. 시간·무선 파라미터**

| **항목**                       | **권장값**      | **설명**               |
| ------------------------------ | --------------- | ---------------------- |
| ADV_INTERVAL                   | 200 ms          | 광고 주기              |
| slot_offset_compute(id, 80, 4) | 0/80/160/240 ms | Child 광고 오프셋 분산 |
| SCAN_INTERVAL                  | 500 ms          | GAP 스캔 주기          |
| SCAN_WINDOW                    | 50–100 ms       | GAP 스캔 윈도          |
| RX_WINDOW_MS                   | 1000            | 집계 윈도              |
| DEFAULT_TTL                    | 2               | 기본 TTL               |
| RELAY_RATIO                    | 0.2–0.3         | 릴레이 활성 비율       |
| RSSI_RANGE                     | -100 ~ -30 dBm  | 허용 RSSI 범위         |
| IQR_MAX                        | 10 dB           | 이상치 판단 기준       |

---

## **4. ESP32 모듈 구조**

### **모듈 A — 스케줄·시간 동기**

- time_sync_apply(ctrl_time, drift_hint)
- slot_offset_compute(node_id, slot_ms, slots)
- periodic_tick(now_ms)

### **모듈 B — TTL·릴레이 정책**

- ttl_should_forward(ttl)
- relay_role_enabled()
- relay_role_set(enabled)
- ttl_policy_apply(default_ttl, zone_hint)

### **모듈 C — PING 송신 (Child)**

- ping_init(t_ping_ms, offset_ms)
- ping_should_fire(now_ms)
- ping_publish(now_ms, seq, txp_dbm, ttl)

### **모듈 D — 수신·RSSI 집계 (Anchor/Child)**

- rx_on_ping(child_id, meta_rssi, rx_ts_ms)
- agg_update(child_id, meta_rssi, rx_ts_ms)
- agg_should_flush(now_ms)
- agg_snapshot(child_id)
- report_publish(child_id, reporter_id, seq_last, stats, dst, ttl)

### **모듈 E — Mesh 바인딩**

- mesh_keys_bind(netkey_id, appkey_id)
- model_bind_vendor(appkey_id)
- pubsub_config(model, pub, subs)
- gatt_proxy_set(enabled)

### **모듈 F — CTRL 처리 (확장)**

- ctrl_apply(payload)
- ctrl_build_time(now_ms)

### **브리지 I/O**

- bridge_emit_json(report)
- bridge_read_ctrl_line()

---

## **5. 데이터 흐름**

1. Child가 200 ms마다 광고(from_id, seq, txp_dbm) 송신
2. Anchor가 스캔 → agg_update()로 1초 윈도에 샘플 누적
3. agg_snapshot()으로 rssi_med, p25, p75, n 계산
4. report_publish()로 VM_REPORT(CBOR payload) Mesh 전송
5. Proxy가 REPORT 수신 → bridge_emit_json()으로 Mother 전송
6. Mother가 ingest_parse() 및 디듑 수행 → 로그 저장
7. Estimator가 (from_id, seq) 그룹으로 위치 추정 수행

---

## **6. 디듑·정합성 요약**

- **REPORT 디듑 키**: (reporter_id, report_seq)
- **ENTRY 디듑 키**: (from_id, to_id, seq)
- **보존 기간**:
  - Report: 30 s
  - Entry: 5 s
- **파싱 실패 처리**: .bad 파일로 분리 저장
- **값 검증 규칙**:
  - RSSI ∈ [-100, 0]
  - n ≤ 255
  - IQR > 10 → 품질 경고

---

## **7. 위치 추정 요약**

- **입력**: 동일 (from_id, seq)의 여러 Anchor 관측치
- **거리 변환식**:

```
d_i = 10^((P0 - rssi_med) / (10 * n_env))
```

-
- **가중치**:

```
w_i = n / (IQR + ε)
```

-
- **추정 알고리즘**: Weighted Nonlinear Least Squares (LM/SVD)
- **초기값**: Anchor 위치의 가중 평균
- **후처리**: 필요 시 Kalman / Particle Filter 적용
- **캘리브레이션**: 1, 3, 5 m 기준 P0, n_env 추정

---

## **8. 신뢰성·장애 대응**

| **범주**    | **정책**                              |
| ----------- | ------------------------------------- |
| 무선 경합   | 스캔 윈도 ≤ 100 ms, interval ≥ 500 ms |
| REPORT 크기 | 엔트리 과다 시 분할 또는 상한(12개)   |
| 릴레이 제한 | Anchor의 20–30%만 ON                  |
| I/O 복구    | 포트 백오프 1→2→5 s, 임시파일 저장    |
| 로그 제어   | 경고 로그 분당 1회 제한               |

---

## **9. 운영 메트릭 및 수락 기준**

| **메트릭**      | **설명**             | **목표** |
| --------------- | -------------------- | -------- |
| report_rate     | REPORT 수집률(건/초) | ≥ 0.95   |
| dup_ratio       | 중복률               | ≤ 0.03   |
| drop_ratio      | 드랍률               | ≤ 0.05   |
| avg_report_size | 평균 REPORT 크기     | ≤ 200 B  |
| rssi_rmse       | 위치 RMSE(정적 대상) | ≤ 3–5 m  |

---

## **10. 보안 및 버전 관리**

- ver 필드로 프로토콜 호환성 유지.
  - 필드 추가(뒤쪽)는 동일 ver 허용.
  - 타입 변경/삭제 시 ver+1.
- NodeId는 32-bit 해시 사용, 충돌 감시.
- MAC ↔ NodeId 매핑은 Mother에서 관리.
- SSID, 비밀번호, 키 등 민감정보는 로그/커밋 금지.

---

## **11. 디렉토리 구조**

```
main/        # ESP-IDF 소스
collector/   # Mother 수집기 (Python)
estimator/   # 위치 추정 스크립트
docs/        # 설계 및 명세 문서
```

루트 파일:

CMakeLists.txt, sdkconfig.defaults, idf_component.yml, .gitignore, README.md

---

## **12. 확장 계획**

- VM_CTRL 활성화 → Mother→Mesh 제어 경로 확립
- Hop별 recv_ttl 체인 수집 (경로 분석용)
- 릴레이/TTL 자동 조정 (자율 최적화)
- 동적 대상 추적 (Kalman / Particle Filter)

---

## **부록 A. 주요 용어**

| **용어**           | **설명**           |
| ------------------ | ------------------ |
| from_id            | 발신자 ID          |
| to_id              | 수신자 ID          |
| seq                | 발신자의 시퀀스    |
| rssi_med, p25, p75 | RSSI 통계값        |
| n                  | 표본 개수          |
| recv_ttl           | Mesh 수신 TTL      |
| report_seq         | REPORT 자체 시퀀스 |

---

## **부록 B. 체크리스트**

- idf.py build 성공
- Mesh 프로비저닝 완료
- ADV / SCAN 동작 확인
- REPORT 1 Hz 수신
- Mother 디듑 및 로그 정상
- wnlls.py 추정 결과 확인
- HEALTH 메트릭 출력 정상

---

**문서 버전**: v1.0 (프로토콜 ver=1 기준)

**참조 문서**: PROTOCOL_V1.md, DEDUP_LOGIC.md, BLE_MESH_GUIDE.md, ROADMAP.md
