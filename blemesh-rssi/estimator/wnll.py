import json, numpy as np

# 앵커 좌표 정의 (예시)
ANCHORS = {
    "0xB0F001": np.array([ 0.0, 0.0]),
    "0xB0F002": np.array([10.0, 0.0]),
    "0xB0F003": np.array([10.0, 8.0]),
    "0xB0F004": np.array([ 0.0, 8.0]),
}

P0 = -59.0    # 1m RSSI
N_ENV = 1.9   # 환경 계수

def rssi_to_dist(rssi):
    return 10 ** ((P0 - rssi) / (10 * N_ENV))

def estimate_origin(entries):
    # entries: 같은 (from_id, seq)에 대해 다양한 to_id(앵커) 관측 목록
    obs = []
    for e in entries:
        if e["kind"] != "ADV": continue
        aid = e["to_id"]
        if aid not in ANCHORS: continue
        d = rssi_to_dist(e["rssi_med"])
        iqr = abs(e.get("p75", e["rssi_med"]) - e.get("p25", e["rssi_med"]))
        n = e.get("n", 1)
        w = n / (iqr + 1e-3)
        obs.append((ANCHORS[aid], d, w))
    if len(obs) < 3: return None

    # 초기값: 가중 평균 위치
    x0 = sum(p*w for p,_,w in obs) / sum(w for _,_,w in obs)

    # LM 간단 반복
    x = x0.copy()
    for _ in range(20):
        J = []; r = []; W=[]
        for p, d, w in obs:
            dx = x - p
            r_i = np.linalg.norm(dx) - d
            if np.linalg.norm(dx) < 1e-6: continue
            J.append((dx/np.linalg.norm(dx)))
            r.append(r_i)
            W.append(w)
        J = np.vstack(J); r = np.array(r)[:,None]; W = np.diag(W)
        try:
            # (J^T W J) dx = - J^T W r
            H = J.T @ W @ J
            g = J.T @ W @ r
            dx = -np.linalg.solve(H, g).flatten()
        except np.linalg.LinAlgError:
            break
        x = x + dx
        if np.linalg.norm(dx) < 1e-3: break
    return x

if __name__ == "__main__":
    # 예시: 최근 로그에서 같은 from_id/seq 묶어서 추정
    import sys
    lines = open(sys.argv[1]).read().strip().splitlines()
    by_key = {}
    for ln in lines[-500:]:
        rec = json.loads(ln)
        rep = rec["report"]
        for e in rep["entries"]:
            if e.get("kind") != "ADV": continue
            k = (e["from_id"], e["seq"])
            by_key.setdefault(k, []).append(e)

    for k, entries in list(by_key.items())[:5]:
        pos = estimate_origin(entries)
        if pos is not None:
            print(f"target={k} -> est={pos}")