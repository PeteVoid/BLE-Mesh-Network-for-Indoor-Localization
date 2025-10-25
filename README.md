# BLEMesh RSSI Localization Project

ESP32 BLE Mesh 기반 실내 위치추정 시스템  
(Child/Anchor/Proxy + Mother + Estimator Pipeline)

---

## Structure

- **ESP32 Nodes (main/)**: BLE Mesh + Advertising + RSSI 집계
- **Mother (collector/)**: CBOR 수신 + 디듑 + 로그 관리
- **Estimator (estimator/)**: 위치추정 (Weighted NLLS)

---

## Directory Overview

```
main/        -> ESP32 firmware (C)
collector/   -> Mother ingestion (Python)
estimator/   -> Position estimator (Python)
docs/        -> Protocol, Design, Roadmap
```

### Detail

```
blemesh-rssi/
├─ README.md
├─ LICENSE
├─ .gitignore
│
├─ sdkconfig.defaults               # IDF 기본 설정값
├─ idf_component.yml                # ESP-IDF dependency (TinyCBOR)
├─ CMakeLists.txt                   # ESP-IDF 프로젝트 루트
│
├─ main/                            # ── ESP32 펌웨어 (ESP-IDF)
│  ├─ CMakeLists.txt
│  ├─ app_main.c                    # 엔트리 (Mesh, GAP, AGG Task 통합)
│  ├─ adv_scan.c                    # Advertising + Scanning 태스크
│  ├─ agg_report.c                  # RSSI 집계(1초 윈도) 및 REPORT 생성
│  ├─ mesh_vendor.c                 # Vendor Model 초기화 & Publish
│  ├─ mesh_vendor.h
│  │
│  ├─ proto_v1.h                    # 공통 타입/상수
│  ├─ adv_frame_v1.h                # ADV 프레임 정의 (packed)
│  ├─ report_v1.h                   # REPORT 구조체 (entries)
│  ├─ report_v1_cbor.c              # TinyCBOR 인코더
│  ├─ report_v1_cbor.h
│  │
│  ├─ util_ringbuf.h                # RSSI 버퍼 구조체 (윈도)
│  ├─ util_stats.c                  # median/p25/p75 계산
│  └─ (optional)                    # 향후 추가 모듈들 (e.g. ctrl_sync.c)
│
├─ collector/                       # ── Mother(Raspberry Pi) 수집기
│  ├─ ingest.py                     # CBOR/JSON 수신 + 디듑 + 로그 롤링
│  ├─ requirements.txt              # pyserial, cbor2, ujson
│  ├─ README.md                     # 간단 실행 가이드
│  └─ (optional) systemd/evac-mesh.service  # 자동 실행 유닛 파일
│
├─ estimator/                       # ── Mother 후처리/위치추정
│  ├─ wnlls.py                      # Weighted Nonlinear Least Squares 추정
│  ├─ anchors.json                  # 앵커 좌표 정의 (테스트용)
│  ├─ calibration.ipynb             # P0/n 추정 노트북 (선택)
│  └─ README.md                     # 추정기 사용법
│
└─ docs/                            # ── 기술 문서 / 설계서
   ├─ DESIGN_OVERVIEW.md            # 전체 구조 요약 (지금 설계서)
   ├─ PROTOCOL_V1.md                # 프레임/CBOR 포맷 명세
   ├─ DEDUP_LOGIC.md                # Mother 디듑 알고리즘 설명
   ├─ BLE_MESH_GUIDE.md             # 프로비저닝/바인딩 절차
   └─ ROADMAP.md                    # 단계별 진행계획 (1~8단계 로드맵)
```

---

## References

- Espressif BLE Mesh Developer Guide
- TinyCBOR library
- FreeRTOS + ESP-IDF v5.x

---

## Build (ESP-IDF 5.x)

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

---

## Run

### Mother

```bash
cd collector
pip install -r requirements.txt
sudo python ingest.py
```

### Estimaate Position

```bash
cd estimator
python wnlls.py ../var/log/evac-mesh/2025-10-25.log
```
