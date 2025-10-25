# ROADMAP.md

> BLE Mesh 기반 RSSI Localization System — Development Roadmap
>
> 본 문서는 시스템의 개발 단계별 목표, 구현 범위, 검증 계획, 품질 기준, 향후 확장 일정을 기술한다.  
> 모든 항목은 실제 코드 구조(`main/`, `collector/`, `estimator/`) 및 프로토콜(`PROTO_VER_V1`)과 직접 연동된다.

---

## 1. 프로젝트 개요

**프로젝트명:** BLE Mesh RSSI Localization  
**하드웨어:** ESP32 DevKitC / Raspberry Pi 4  
**목표:** 실내 환경에서 Child–Anchor 간 RSSI를 확률적 모델로 분석하여 위치 추정  
**기간:** 2025.09 ~ 2026.02  
**리포지토리 구조:**

```
/main         → ESP-IDF 펌웨어
/collector    → Mother(Raspberry Pi) 수집기
/estimator    → 위치 추정기
/docs         → 설계 문서 세트
```

---

## 2. 개발 단계 요약

| 단계        | 기간    | 코드명           | 목표                                                |
| ----------- | ------- | ---------------- | --------------------------------------------------- |
| **Phase 0** | 2025.09 | `bootstrap`      | 프로젝트 초기화, 하드웨어 검증                      |
| **Phase 1** | 2025.10 | `mesh-mvp`       | BLE Mesh + Advertising + CBOR REPORT 최소 기능 구현 |
| **Phase 2** | 2025.11 | `dedup-pipeline` | Mother 수집기 완성, 중복 제거 및 로그 롤링          |
| **Phase 3** | 2025.12 | `rssi-model`     | RSSI 분포 모델링, 가중치 기반 거리 변환             |
| **Phase 4** | 2026.01 | `estimation`     | 위치 추정(Weighted NLLS + Kalman)                   |
| **Phase 5** | 2026.02 | `release-v1`     | 통합 테스트, 문서화, 코드 정리                      |

---

## 3. Phase 0 — 초기 세팅 (`bootstrap`)

**목표:** 환경 구축 및 하드웨어 검증

- ESP-IDF 설치 (`v5.x`)
- Python 수집기 환경 설정 (`venv + pyserial + cbor2`)
- BLE Mesh 기본 예제 (`esp_ble_mesh_light`) 기반 실험
- ADC, GPIO, 전원 안정화 검증

**산출물:**

- `idf.py build` 정상 동작
- `esp_ble_mesh_init()` 성공 로그 확인
- `/main/` 빌드 및 flash 스크립트 작성
- `README_SETUP.md` (설치 가이드)

---

## 4. Phase 1 — Mesh MVP (`mesh-mvp`)

**목표:** 최소 동작 가능한 메시 네트워크 구축

- Advertising, Scanning, Mesh REPORT 전송
- 구조체: `report_v1_t`, `report_entry_v1_t`
- 직렬화: `report_v1_cbor_encode()`, `decode()`
- 전송: `mesh_publish_report()`
- 수신: Proxy → Mother USB 브릿지 (`bridge_emit_json()`)

**테스트 조건:**

- Child ↔ Anchor 1:1 RSSI 수집 확인
- Mother 로그에서 `report_seq` 증가 확인

**완료 기준:**

- 1 Hz REPORT 생성/수신 성공
- CBOR 인코딩 정확성 검증

---

## 5. Phase 2 — Dedup Pipeline (`dedup-pipeline`)

**목표:** Mother의 중복 제거 및 저장 파이프라인 완성

- 모듈: `/collector/ingest.py`
- 디듑 키:
  - REPORT: `(reporter_id, report_seq)`
  - ENTRY: `(from_id, to_id, seq)`
- TTL: REPORT 30s, ENTRY 5s
- 주기적 GC (`gc_sweep()`)

**테스트 조건:**

- 중복 REPORT 재전송 시 Drop 확인
- `.bad` 파일에 파싱 실패 라인 저장
- 헬스 로그 (`health_emit()`) 정상 출력

**완료 기준:**

- REPORT/ENTRY 중복률 ≤ 3%
- 로그 롤링 정상 수행

---

## 6. Phase 3 — RSSI Model (`rssi-model`)

**목표:** RSSI 통계 기반 거리 추정 모델 구현

- 데이터: `rssi_med`, `p25`, `p75`, `n`
- 거리 추정식:

```
d_i = 10^((P0 - rssi_med) / (10 * n_env))
```

- 신뢰도 가중치:

```
w_i = n / (IQR + ε)
```

- Python 구현 파일: `/estimator/model_rssi.py`
- 그래프 시각화 (`matplotlib` 기반)

**테스트 조건:**

- 고정 거리(1m/3m/5m)에서 RSSI 샘플 수집
- RMSE 계산 및 `P0`, `n_env` 보정

**완료 기준:**

- 평균 오차 ≤ 2 dB
- `anchors.json` 보정 테이블 생성

---

## 7. Phase 4 — Estimation (`estimation`)

**목표:** Weighted Nonlinear Least Squares 위치 추정

- 입력: 동일 `(from_id, seq)` 의 여러 Anchor REPORT
- 알고리즘: Levenberg–Marquardt / SVD 기반 WNLLS
- 후처리:
- `Kalman` 필터 → 연속 추정
- `Particle Filter` (선택적)
- 출력:
- `{x, y, z, variance, t_epoch}`
- 시각화: `/estimator/plot_estimate.py`

**테스트 조건:**

- 최소 Anchor 3개 이상일 때 위치 계산 가능
- RMSE ≤ 5m
- 추적 샘플 레이트 1Hz 유지

**완료 기준:**

- 실내 테스트 코스에서 평균 RMSE ≤ 4m
- 시간 기반 추정 안정성 ±1m/s

---

## 8. Phase 5 — Release (`release-v1`)

**목표:** 통합 및 문서화, 신뢰성 확보

- 모든 모듈 버전 고정 (`PROTO_VER_V1`)
- 문서 패키징 (`docs/`)
- Mother 자동 실행 스크립트 (`systemd` 서비스)
- 전체 End-to-End 테스트 (Child–Anchor–Mother–Estimator)

**테스트 조건:**

- 장치 10대 이상 구성 시 Mesh 안정성 유지
- 보고 누락률 ≤ 5%
- Mother CPU 점유율 < 40%
- Estimator 연속 추적 시 오차 ±3m 이하

**완료 기준:**

- `v1.0` 릴리스 태그 푸시
- `CHANGELOG.md` 업데이트
- `release.zip` 배포

---

## 9. 품질 관리 기준 (QA Metrics)

| 항목               | 지표                         | 기준    |
| ------------------ | ---------------------------- | ------- |
| 빌드 신뢰성        | `idf.py build` 성공률        | ≥ 99%   |
| REPORT 수집률      | Mother 수신 건수 / 송신 건수 | ≥ 95%   |
| 중복률             | 드랍된 중복 비율             | ≤ 3%    |
| 오차 (RSSI → 거리) | RMSE                         | ≤ 2 dB  |
| 오차 (위치 추정)   | RMSE                         | ≤ 4 m   |
| Mother 안정성      | CPU 사용률                   | ≤ 40%   |
| 로그 일관성        | `.log` / `.bad` 분리         | 100%    |
| 전력 효율          | Child 평균 소비전류          | ≤ 30 mA |

---

## 10. 향후 확장 계획

| 시점     | 이름           | 설명                                               |
| -------- | -------------- | -------------------------------------------------- |
| **v2.0** | `ctrl-sync`    | Mother → Node 제어 채널 (`VM_CTRL`) 활성화         |
| **v2.1** | `hop-trace`    | 릴레이 경로(`recv_ttl`) 추적, Mesh 품질 맵         |
| **v2.2** | `adaptive-ttl` | RSSI 분포 기반 TTL 자동 조정                       |
| **v3.0** | `fingerprint`  | ML 기반 RSSI Fingerprinting 위치 추정              |
| **v3.1** | `auto-calib`   | 자동 캘리브레이션 및 P0/n_env 실시간 보정          |
| **v3.5** | `full-cloud`   | Mother → 클라우드 데이터 업로드 및 시각화 대시보드 |

---

## 11. 테스트 및 검증 시나리오

| 테스트 명             | 환경                | 목표             | 통과 기준                |
| --------------------- | ------------------- | ---------------- | ------------------------ |
| `basic_mesh_ping`     | 1 Child / 1 Anchor  | Mesh 연결 테스트 | PING 수신률 ≥ 99%        |
| `multi_anchor_report` | 1 Child / 3 Anchors | REPORT 수집      | 3개 REPORT 동일 seq 수신 |
| `relay_path_check`    | TTL=2               | 릴레이 품질 확인 | TTL hop loss ≤ 1         |
| `mother_ingest`       | Raspberry Pi 4      | 수집기 정상 동작 | 중복률 ≤ 3%              |
| `estimation_static`   | 고정 위치           | 위치 정확도      | RMSE ≤ 4 m               |
| `estimation_dynamic`  | 이동 대상           | 추적 연속성      | 속도 오차 ≤ ±1 m/s       |

---

## 12. 코드 릴리스 정책

- **버전 태그 규칙:**  
  `v<proto>.<phase>.<patch>` (예: `v1.2.0`)
- **커밋 메시지 형식:**
- `feat(main): ...`
- `fix(collector): ...`
- `test(estimator): ...`
- **릴리스 절차:**

1. 모든 Phase 테스트 완료
2. `docs/` 업데이트
3. `CHANGELOG.md` 생성
4. `git tag` → `push`
5. `release.zip` 생성 및 업로드

---

## 13. 운영 자동화

Mother 시스템은 `systemd` 서비스로 실행한다.

### 13.1 서비스 파일 예시

`/etc/systemd/system/evac-mesh.service`

```
[Unit]
Description=BLE Mesh Data Collector
After=network.target

[Service]
ExecStart=/usr/bin/python3 /home/pi/collector/ingest.py
WorkingDirectory=/home/pi/collector
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable evac-mesh
sudo systemctl start evac-mesh
sudo systemctl status evac-mesh
```

---

## **14. 버전 맵**

```
v1.0 ──────┬─── Mesh MVP
            ├─── Dedup Pipeline
            ├─── RSSI Model
            ├─── Estimation Core
            └─── Release Candidate
v2.0 ──────┬─── CTRL Channel
            ├─── Adaptive TTL
            └─── Hop Trace
v3.0 ──────┬─── ML Fingerprinting
            └─── Cloud Integration
```

---

## **15. 결론**

이 로드맵의 목표는 BLE Mesh 네트워크를 기반으로 **현실적인 실내 위치 추정 시스템의 프로토타입을 완성**하는 것이다.

각 단계는 독립적으로 검증 가능하며,

다음 원칙을 유지한다:

1. **단순성:** 메시지 포맷, 프로토콜, 구조 최소화
2. **투명성:** 모든 단계 로깅 및 검증 가능
3. **확장성:** v2/v3에서 추가 센서·제어 가능
4. **재현성:** 동일 설정으로 항상 동일 결과 재현

---

**문서 버전:** 1.0

**참조 문서:** DESIGN_OVERVIEW.md, PROTOCOL_V1.md, DEDUP_LOGIC.md, BLE_MESH_GUIDE.md
