# Mother Collector

BLE Mesh Proxy(ESP32)가 USB CDC로 전송하는 REPORT 라인을 수신해 디듑, 로그 저장, 헬스
모니터링을 수행하는 Raspberry Pi용 Python 스크립트다. `ingest.py` 하나만 실행하면 되고,
시리얼 포트와 로그 경로는 상단 상수로 조정한다.

## Requirements

- Python 3.9+
- `pip install -r requirements.txt` (pyserial, cbor2, ujson)
- REPORT가 `/dev/ttyACM0`로 USB CDC 형태로 들어오도록 Proxy 구성

## Run

```bash
cd collector
pip install -r requirements.txt
sudo python ingest.py
```

- `PORT`, `BAUD`, `LOG_DIR` 상수로 환경을 맞춘다.
- Systemd 서비스로 자동화하려면 `systemd/evac-mesh.service`를 `/etc/systemd/system`에
  배치 후 `sudo systemctl enable --now evac-mesh.service`.

## Processing Pipeline

1. 한 줄씩 읽어 JSON → 실패 시 CBOR 디코드 후 `{ingress_ts, report}` 래핑.
2. REPORT 키 `(reporter_id, report_seq)`를 30 초 TTL로 디듑.
3. ENTRY 키 `(from_id, to_id, seq)`를 5 초 TTL로 디듑.
4. 통과한 데이터를 날짜별 로그 파일(`${LOG_DIR}/YYYY-MM-DD.log`)에 JSON Lines로 저장.
5. 파싱 실패 라인은 같은 이름의 `.bad` 파일에 원문으로 남김.
6. 60 초마다 health 라인 출력(`reports`, `entries`, `dupR`, `dupE`).

## Tips

- 로그 파일은 다른 파이프라인(`estimator/`)에서 그대로 읽을 수 있다.
- 장기간 실행 시 SD 카드 수명을 위해 `LOG_DIR`을 tmpfs나 외장 SSD로 잡는 것이 좋다.
- Mother 측에서 NodeId ↔ MAC 매핑, 헬스 모니터링, 원격 제어를 추가하려면
  `ingest.py`의 TODO 영역을 확장하거나 별도 프로세스를 붙이면 된다.
