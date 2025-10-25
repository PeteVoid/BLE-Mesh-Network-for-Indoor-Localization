# BLEMesh RSSI Firmware

ESP32 기반 BLE Mesh 노드 펌웨어와 Mother/Estimator 파이프라인을 담고 있는 루트 프로젝트다.
`main/`에 포함된 ESP-IDF 애플리케이션이 Child/Anchor/Proxy 역할을 수행하며, 수집기는
`collector/`, 위치 추정기는 `estimator/` 디렉터리에 존재한다.

## Directory

- `main/` – GAP Advertising/Scanning, RSSI 집계, Mesh Vendor Report 송신
- `collector/` – Mother(Raspberry Pi) 수집기. CBOR/JSON 수신 → 디듑 → 로그/헬스 출력
- `estimator/` – Weighted NLLS 기반 위치 추정 스크립트와 앵커 정의
- `docs/` – 프로토콜, 디듑 로직, Mesh 가이드, 로드맵 등의 상세 문서
- `sdkconfig.defaults`, `idf_component.yml` – ESP-IDF 기본 설정과 TinyCBOR 의존성

## Build & Flash (ESP-IDF 5.x)

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

필요시 `idf.py menuconfig`로 Wi-Fi/BLE 파라미터를 커스터마이징한다. Mesh 프로비저닝 절차와
앵커/프록시 설정은 `docs/BLE_MESH_GUIDE.md`에 정리되어 있다.

## Runtime Flow

1. Child 노드는 200 ms 간격으로 Manufacturer AD 프레임(`adv_frame_v1_t`)을 송신한다.
2. Anchor/Proxy 노드는 스캔한 RSSI 샘플을 `agg_report.c`의 1 초 윈도 버퍼에 누적 후
   중앙값·사분위수·샘플 수로 요약해 CBOR REPORT(`report_v1_t`)를 Mesh로 퍼블리시한다.
3. Mother 수집기(`collector/ingest.py`)가 USB CDC로 REPORT를 받아 디듑·로그 저장을 수행한다.
4. 저장된 로그는 `estimator/wnll.py`에서 Weighted Nonlinear Least Squares로 위치를 추정한다.

## Related Docs

- `docs/DESIGN_OVERVIEW.md` – 전체 파이프라인 개요
- `docs/PROTOCOL_V1.md` – ADV/REPORT/CBOR 필드 정의
- `docs/DEDUP_LOGIC.md` – Mother 디듑 규칙 요약
- `docs/ROADMAP.md` – 단계별 개발 계획
