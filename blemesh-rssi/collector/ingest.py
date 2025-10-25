import serial, time, os, json, ujson, cbor2
from datetime import datetime, timezone

PORT = "/dev/ttyACM0"
BAUD = 921600
LOG_DIR = "/var/log/evac-mesh"

# 디듑 캐시
seen_reports = {}   # key=(reporter_id, report_seq) -> ts
seen_entries = {}   # key=(from_id, to_id, seq) -> ts

TTL_REPORTS = 30
TTL_ENTRIES = 5

def rotate_path(now):
    d = datetime.fromtimestamp(now, tz=timezone.utc).strftime("%Y-%m-%d")
    os.makedirs(LOG_DIR, exist_ok=True)
    return os.path.join(LOG_DIR, f"{d}.log")

def sweep(tt: dict, ttl: int, now: float):
    drop = [k for k,v in tt.items() if now - v > ttl]
    for k in drop: tt.pop(k, None)

def decode_line(line: bytes):
    # Proxy에서 CBOR raw로 올 수도, JSON line으로 올 수도 있음.
    # 우선 JSON 우선, 실패 시 CBOR로 시도.
    try:
        return ujson.loads(line)
    except Exception:
        try:
            obj = cbor2.loads(line)
            # CBOR가 상위 report만 담겼다면 JSON으로 래핑
            return {"ingress_ts": int(time.time()*1000), "report": obj}
        except Exception:
            return None

def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    cur_path = rotate_path(time.time())
    log = open(cur_path, "a", buffering=1)

    last_health = 0
    report_ok = entry_ok = dup_r = dup_e = 0

    while True:
        line = ser.readline()
        now = time.time()
        if not line:
            # health 주기
            if now - last_health > 60:
                print(f"[HEALTH] reports={report_ok} entries={entry_ok} dupR={dup_r} dupE={dup_e}")
                last_health = now
            sweep(seen_reports, TTL_REPORTS, now)
            sweep(seen_entries, TTL_ENTRIES, now)
            continue

        obj = decode_line(line.strip())
        if not obj:
            # .bad 저장
            badp = rotate_path(now)+".bad"
            with open(badp, "ab") as bf: bf.write(line)
            continue

        # ingress_ts 보정
        obj.setdefault("ingress_ts", int(now*1000))

        # REPORT 디듑
        rep = obj.get("report") or obj  # 둘 중 하나
        ver = rep.get("ver", 1)
        rid = rep["reporter_id"]
        rseq= rep.get("report_seq", -1)

        keyR = (rid, rseq)
        if rseq != -1:
            if keyR in seen_reports:
                dup_r += 1
                continue
            seen_reports[keyR] = now

        # ENTRY 디듑 + 저장
        entries = rep.get("entries", [])
        kept = []
        for e in entries:
            kind = e.get("kind", "ADV")
            if kind == "ADV":
                fid = e["from_id"]; tid = e["to_id"]; seq = e.get("seq", -1)
                if seq == -1:
                    continue
                keyE = (fid, tid, seq)
                if keyE in seen_entries:
                    dup_e += 1
                    continue
                seen_entries[keyE] = now
            kept.append(e)

        # entries 덮어쓰기
        rep["entries"] = kept
        report_ok += 1
        entry_ok += len(kept)

        # 롤링
        path = rotate_path(now)
        if path != cur_path:
            log.close()
            cur_path = path
            log = open(cur_path, "a", buffering=1)

        log.write(json.dumps({"ingress_ts": obj["ingress_ts"], "report": rep}) + "\n")

if __name__ == "__main__":
    main()