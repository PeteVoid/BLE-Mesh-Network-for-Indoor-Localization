# PROTOCOL_V1.md

> BLE Mesh RSSI 보고 프로토콜 (Version 1)
>
> 본 문서는 `PROTO_VER_V1` 기반의 CBOR 직렬화 규격 및 필드 정의를 기술한다.  
> 모든 REPORT 패킷은 이 명세를 기반으로 구성되어야 하며,  
> `ESP32` 단과 `Mother(Raspberry Pi)` 간 교환되는 데이터의 상호 운용성을 보장한다.

---

## 1. 버전 및 상수

| 이름                   | 값     | 설명                             |
| ---------------------- | ------ | -------------------------------- |
| `PROTO_VER_V1`         | `0x01` | 현재 프로토콜 버전               |
| `TXP_SENTINEL_UNKNOWN` | `0x7F` | 송신 전력이 불명확할 때 사용     |
| `OBS_KIND_ADV`         | `0x00` | Advertising 수신에서 관측된 RSSI |
| `OBS_KIND_MESH_ADV`    | `0x01` | Mesh 릴레이에서 관측된 RSSI      |
| `OBS_KIND_MESH_REPORT` | `0x02` | Mesh REPORT 수신에서 관측된 RSSI |

> **참고:** `OBS_KIND_*` 값은 `report_entry_v1_t.kind` 필드에 들어간다.  
> 나중 버전(v2)에서 `OBS_KIND_CTRL`, `OBS_KIND_BEACON` 등이 확장될 수 있다.

---

## 2. 데이터 타입 정의

### 2.1 기본 타입

| 타입명      | 크기       | 설명                                                 |
| ----------- | ---------- | ---------------------------------------------------- |
| `node_id_t` | `uint32_t` | 32비트 노드 식별자 (`MAC` 6바이트 해시 또는 내부 ID) |
| `EpochMs`   | `uint64_t` | UNIX epoch (ms) 단위 시각                            |
| `RssiDbm`   | `int8_t`   | RSSI(dBm) 값                                         |
| `Ttl`       | `uint8_t`  | Mesh TTL 값                                          |
| `Seq`       | `uint32_t` | 발신자별 시퀀스 번호                                 |
| `ReportSeq` | `uint16_t` | REPORT 자체의 시퀀스 번호                            |

---

## 3. 메시지 계층 구조

### 3.1 REPORT Frame

REPORT는 Mesh 상에서 `VM_REPORT (Opcode = 0x02)`으로 전송되는  
RSSI 통계 보고용 메시지이다.

#### 구조체 정의 (`report_v1_t`)

```c
typedef struct __attribute__((packed)) {
    uint8_t  ver;             // PROTO_VER_V1
    node_id_t reporter_id;    // 보고자(Anchor/Child)
    uint16_t window_ms;       // 집계 윈도 크기 (기본 1000 ms)
    uint16_t report_seq;      // REPORT 자체 시퀀스
    uint8_t  entry_count;     // entries[] 길이
    report_entry_v1_t entries[REPORT_ENTRY_MAX]; // 간선별 RSSI 통계
} report_v1_t;
```

#### **제약 조건**

- entry_count <= REPORT_ENTRY_MAX (기본 12)
- window_ms는 항상 1000의 배수 권장
- ver == PROTO_VER_V1

---

### **3.2 REPORT Entry**

REPORT 내부의 각 엔트리는

Child ↔ Reporter 또는 Relay ↔ Reporter 간 **단일 링크의 RSSI 통계**를 의미한다.

#### **구조체 정의 (**

#### **report_entry_v1_t**

#### **)**

```
typedef struct __attribute__((packed)) {
    node_id_t from_id;   // 관측 대상(Child 등)
    node_id_t to_id;     // 관측자(나 자신)
    uint8_t   kind;      // OBS_KIND_*
    uint32_t  seq;       // 대상의 시퀀스 번호
    int8_t    rssi_med;  // 중앙값
    int8_t    p25;       // 25% 백분위
    int8_t    p75;       // 75% 백분위
    uint8_t   n;         // 샘플 수 (≤255)
    int8_t    txp_dbm;   // 송신 전력(dBm)
    uint8_t   recv_ttl;  // 수신 시 남은 TTL
} report_entry_v1_t;
```

#### **필드 의미**

| **필드명** | **설명**                                         |
| ---------- | ------------------------------------------------ |
| from_id    | RSSI 측정 대상 노드                              |
| to_id      | 측정 노드(나)                                    |
| kind       | 관측 타입 (ADV/MESH_ADV/MESH_REPORT)             |
| seq        | 대상 노드의 광고 시퀀스                          |
| rssi_med   | 중앙값 RSSI                                      |
| p25, p75   | 하위/상위 25%, 75% 백분위                        |
| n          | 샘플 개수                                        |
| txp_dbm    | 송신 전력 (알 수 없을 경우 TXP_SENTINEL_UNKNOWN) |
| recv_ttl   | REPORT 수신 시 잔여 TTL (0 = 직접 수신)          |

---

### **3.3 Mother JSON Frame**

Mother는 REPORT 수신 후 다음 구조로 JSON Lines로 변환한다.

```
{
  "ingress_ts": 1730112345678,
  "report": {
    "ver": 1,
    "reporter_id": 11649857,
    "window_ms": 1000,
    "report_seq": 82,
    "entries": [
      {
        "from_id": 65537,
        "to_id": 11649857,
        "kind": 0,
        "seq": 237,
        "rssi_med": -67,
        "p25": -70,
        "p75": -65,
        "n": 5,
        "txp_dbm": -4,
        "recv_ttl": 0
      }
    ]
  }
}
```

---

## **4. CBOR 인코딩 규약**

### **4.1 인코딩 포맷**

모든 REPORT는 CBOR로 직렬화되며,

필드 순서는 고정된 **정적 배열 포맷(array-based encoding)** 을 따른다.

#### **REPORT 인코딩**

```
[
  ver,
  reporter_id,
  window_ms,
  report_seq,
  entry_count,
  [
    entry_0,
    entry_1,
    ...
  ]
]
```

#### **REPORT ENTRY 인코딩**

```
[
  from_id,
  to_id,
  kind,
  seq,
  rssi_med,
  p25,
  p75,
  n,
  txp_dbm,
  recv_ttl
]
```

> 필드 누락 금지.

> 각 정수형은 **LE 32/16/8bit unsigned 또는 signed integer**로 인코딩된다.

---

### **4.2 인코더/디코더 함수**

#### **인코더**

```
bool report_v1_cbor_encode(const report_v1_t* rpt, uint8_t* out_buf, size_t buf_sz, size_t* out_len);
```

- 입력: report_v1_t\*
- 출력: out_buf (CBOR 직렬화 결과), out_len (길이)
- 반환: 성공 시 true, 실패 시 false

#### **디코더**

```
bool report_v1_cbor_decode(report_v1_t* out_rpt, const uint8_t* buf, size_t len);
```

- 입력: CBOR 버퍼
- 출력: 구조체 복원
- 검증: ver == PROTO_VER_V1 인지 확인

---

## **5. 예시**

### **5.1 REPORT 생성 예시 (ESP32 측)**

```
report_v1_t rpt = {
    .ver = PROTO_VER_V1,
    .reporter_id = 0xB0F001,
    .window_ms = 1000,
    .report_seq = 42,
    .entry_count = 1
};

report_entry_v1_t e1 = {
    .from_id = 0xA001,
    .to_id = 0xB0F001,
    .kind = OBS_KIND_ADV,
    .seq = 237,
    .rssi_med = -67,
    .p25 = -70,
    .p75 = -65,
    .n = 5,
    .txp_dbm = -4,
    .recv_ttl = 0
};

rpt.entries[0] = e1;

uint8_t buf[256];
size_t len = 0;
report_v1_cbor_encode(&rpt, buf, sizeof(buf), &len);
mesh_publish_report(buf, len);
```

---

### **5.2 Mother 수신 후 저장 형태 (JSON Line)**

```
{"ingress_ts":1730112345678,"report":{"ver":1,"reporter_id":11534592,"window_ms":1000,"report_seq":42,"entries":[{"from_id":65537,"to_id":11534592,"kind":0,"seq":237,"rssi_med":-67,"p25":-70,"p75":-65,"n":5,"txp_dbm":-4,"recv_ttl":0}]}}
```

---

## **6. 제약 조건 및 오류 처리**

| **항목**           | **조건**            | **위반 시 처리**        |
| ------------------ | ------------------- | ----------------------- |
| ver                | 반드시 PROTO_VER_V1 | 디코드 실패             |
| entry_count        | ≤ REPORT_ENTRY_MAX  | 가장 마지막 엔트리 드랍 |
| rssi_med, p25, p75 | ∈ [-100, 0]         | 비정상 경고             |
| n                  | ≥ 1                 | 0이면 무시              |
| recv_ttl           | ≤ 7                 | 초과 시 7로 clamp       |

---

## **7. 상위 계층 매핑**

| **계층** | **프로토콜**                      | **전송 경로**                    |
| -------- | --------------------------------- | -------------------------------- |
| L4       | CBOR over BLE Mesh (Vendor Model) | G_REPORT 그룹 또는 Proxy Unicast |
| L3       | BLE Mesh Transport                | Segment/Relay 지원               |
| L2       | BLE Link Layer                    | 광고·스캔 병행                   |

---

## **8. 확장 및 버전 호환성**

- **v1 → v2 변경 예상**
  - entries[]에 hop_rssi_chain[] 추가 가능
  - reporter_loc 필드 추가 (Anchor 위치 캐시)
  - signature 필드 추가 (무결성 검증)
- **호환성 원칙**
  - v1 해석기는 추가 필드 무시
  - 필드 순서 변경 금지

---

## **9. 구현 참고 (ESP-IDF)**

- 인코딩 라이브러리: tinycbor (ESP-IDF 내장)
- API 스켈레톤:
  - report_v1_cbor_encode()
  - report_v1_cbor_decode()
  - mesh_publish_report()
  - mesh_vendor_model_send(opcode, payload, len)

빌드 시 report_v1.c 및 report_v1_cbor.c를 포함해야 한다.

---

**문서 버전**: 1.0

**참조 문서**: DESIGN_OVERVIEW.md, DEDUP_LOGIC.md
