# Estimator

Mother가 저장한 REPORT 로그를 읽어 Child 위치를 추정하는 Python 도구다.
`wnll.py`는 Weighted Nonlinear Least Squares(WNLLS) 알고리즘으로 RSSI→거리 변환 후
최소 3개의 앵커 관측을 사용해 2D 좌표를 계산한다.

## Files

- `wnll.py` – 핵심 추정 스크립트. numpy만 의존한다.
- `anchor.json` – 실제 배치 시 사용할 앵커 좌표 정의(커스텀 포맷 준비용).
- `calibration.ipynb` – P0, n_env 파라미터를 현장 측정으로 보정하는 노트북.

## Usage

```bash
cd estimator
python wnll.py ../var/log/evac-mesh/2025-10-25.log
```

스크립트는 입력 로그의 최근 500줄을 읽어 `(from_id, seq)` 묶음마다 추정을 수행하고,
`target=(child_id, seq) -> est=[x, y]` 형태로 출력한다.

## Customization

- **Anchor 좌표**: `ANCHORS` 딕셔너리를 현장 좌표(미터 단위)로 수정하거나,
  별도 JSON 로딩 로직을 추가해 `anchor.json`에서 불러오도록 확장한다.
- **전파 파라미터**: `P0`, `N_ENV` 값을 캘리브레이션 노트북에서 측정한 값으로 교체한다.
- **후처리**: Kalman/Particle 필터, 속도 추정 등은 `estimate_origin` 결과를
  소비하는 별도 스크립트에서 구현할 수 있다.

## Pipeline Reminder

1. Mother 로그는 `collector/ingest.py`가 생성하는 JSON Lines 형식이다.
2. 각 `entries[]` 요소는 동일한 Child 광고 사건을 여러 Anchor가 보고한 값이다.
3. `wnll.py`는 동일 `(from_id, seq)` 묶음만 사용하므로, 로그에 Anchor가 3개 미만이면
   추정 결과가 `None`으로 나온다.
