# BLE_MESH_GUIDE.md

> BLE Mesh Configuration & Operation Guide (ESP32 기반 RSSI Localization 시스템)

---

## 1. 개요

본 문서는 ESP32 BLE Mesh 네트워크를 구성하고 운영하기 위한 절차와  
필요한 API, 모델, 그룹, 키 바인딩 방식을 상세히 설명한다.

구성은 다음 4가지 계층으로 이루어진다:

1. **Mother (Raspberry Pi)** — 중앙 제어 및 로그 수집
2. **Proxy (ESP32)** — GATT ↔ Mesh 브릿지
3. **Anchor (ESP32)** — 광고 수신, RSSI 집계, REPORT 송신
4. **Child (ESP32)** — 주기적 Advertising 송신

---

## 2. BLE Mesh 기본 개념 요약

| 용어          | 설명                                            |
| ------------- | ----------------------------------------------- |
| `Provisioner` | 새로운 노드를 Mesh 네트워크에 등록하는 역할     |
| `Node`        | Mesh에 포함된 장치 (Anchor/Child/Proxy 등)      |
| `Element`     | 하나의 Node 내에서 모델들을 포함하는 단위       |
| `Model`       | 실제 기능 단위 (Vendor Model, Generic Model 등) |
| `AppKey`      | Application 레벨 메시지 암호화 키               |
| `NetKey`      | 네트워크 레벨 메시지 암호화 키                  |
| `Publish`     | 메시지를 송신할 주소 또는 그룹                  |
| `Subscribe`   | 메시지를 수신할 그룹 리스트                     |
| `TTL`         | 메시지가 전달될 수 있는 최대 홉 수              |
| `Relay`       | 다른 노드로 메시지를 재전송할 수 있는 기능      |
| `Proxy`       | GATT(BLE 연결) ↔ Mesh 패킷을 변환해주는 노드    |

---

## 3. 프로비저닝 절차

### 3.1 초기 준비

```bash
idf.py -p /dev/ttyUSB0 erase_flash
idf.py -p /dev/ttyUSB0 flash monitor
```

1. Mother는 **Provisioner** 역할을 하지 않음.

   → Proxy ESP32 중 하나가 Provisioner 역할 수행.

2. 모든 Anchor/Child는 **Unprovisioned Node** 상태로 시작.

### **3.2 Provisioner 절차 (Proxy Node)**

```
esp_ble_mesh_init();
esp_ble_mesh_provisioner_enable(ESP_BLE_MESH_PROV_GATT | ESP_BLE_MESH_PROV_ADV);
```

- BLE Advertising을 스캔하여 Unprovisioned Device 탐색
- esp_ble_mesh_provisioner_provision() 호출로 등록

```
esp_ble_mesh_provisioner_provision(dev_uuid, net_idx, addr, flags, iv_index);
```

등록 후 다음 정보가 생성됨:

- NetKey (네트워크 전체 공통)
- AppKey (Vendor Model용)
- Unicast Address (노드 고유 주소)

---

## **4. 키 바인딩**

### **4.1 NetKey / AppKey 바인딩**

```
esp_ble_mesh_provisioner_add_local_net_key(net_key, &net_idx);
esp_ble_mesh_provisioner_add_local_app_key(app_key, net_idx, &app_idx);
```

### **4.2 모델 바인딩**

```
esp_ble_mesh_provisioner_bind_app_key_to_model(app_idx, element_addr, vendor_model_id, company_id);
```

- **Vendor Model ID**: 0x0001 (내부 정의)
- **Company ID**: 0xFFFF (임시, 내부 프로젝트용)

---

## **5. Vendor Model (RSSI Reporting)**

### **5.1 모델 구성**

| **모델명** | **모델 ID** | **방향**                 | **설명**                              |
| ---------- | ----------- | ------------------------ | ------------------------------------- |
| VM_PING    | VENDOR/0x01 | TX (Child)               | Child가 광고 이벤트를 Mesh로 알림     |
| VM_REPORT  | VENDOR/0x02 | TX (Anchor) / RX (Proxy) | Anchor가 RSSI 통계 보고               |
| VM_CTRL    | VENDOR/0x03 | RX (Node)                | Mother 제어 명령 수신 (TTL, Relay 등) |

### **5.2 모델 선언 (C 코드 예시)**

```
ESP_BLE_MESH_MODEL_PUB_DEFINE(vendor_pub, 0, ROLE_NODE);
esp_ble_mesh_model_t vendor_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(COMPANY_ID, VENDOR_MODEL_ID, vendor_op, &vendor_pub, NULL),
};
```

vendor_op 배열에는 다음 오퍼레이션이 정의됨:

```
ESP_BLE_MESH_VENDOR_MODEL_OP(VM_PING, 0, ping_handler),
ESP_BLE_MESH_VENDOR_MODEL_OP(VM_REPORT, 0, report_handler),
ESP_BLE_MESH_VENDOR_MODEL_OP(VM_CTRL, 0, ctrl_handler),
```

---

## **6. Publish / Subscribe 구성**

### **6.1 그룹 주소 정의**

| **그룹명** | **주소 (hex)** | **설명**                           |
| ---------- | -------------- | ---------------------------------- |
| G_PING     | 0xC001         | Child들이 PING 송신하는 그룹       |
| G_REPORT   | 0xC002         | Anchor들이 REPORT 송신하는 그룹    |
| G_CTRL     | 0xC003         | Mother가 제어 명령을 송신하는 그룹 |

### **6.2 Pub/Sub 설정**

```
esp_ble_mesh_model_pub_set(&vendor_models[MODEL_PING], G_PING);
esp_ble_mesh_model_sub_add(&vendor_models[MODEL_REPORT], G_REPORT);
```

또는 추상화 함수 사용:

```
pubsub_config("PING", pub=G_PING);
pubsub_config("REPORT", subs=[G_REPORT]);
```

---

## **7. 릴레이 및 TTL 정책**

### **7.1 릴레이 설정**

```
esp_ble_mesh_node_set_relay(true);
```

또는 래퍼:

```
relay_role_set(true);
```

### **7.2 TTL 정책**

```
uint8_t ttl = ttl_policy_apply(DEFAULT_TTL, zone_hint);
```

- DEFAULT_TTL = 2
- zone_hint = 복도(2) / 음영지대(3)

릴레이 가능 조건:

```
if (ttl_should_forward(ttl)) {
    mesh_vendor_model_relay(packet);
}
```

---

## **8. Proxy / Mother 브리지 구성**

### **8.1 GATT Proxy 모드 활성화**

```
esp_ble_mesh_proxy_client_enable();
gatt_proxy_set(true);
```

### **8.2 Mother 직렬 연결**

Mother는 /dev/ttyUSBx 포트를 통해 Proxy와 통신한다.

```
sudo stty -F /dev/ttyUSB0 115200 raw -echo
cat /dev/ttyUSB0
```

Proxy의 출력 포맷 (JSON Lines):

```
{"ingress_ts":1730112345678,"report":{"ver":1,"reporter_id":0xB0F001,...}}
```

---

## **9. Mesh 초기화 순서 요약**

```
esp_ble_mesh_init();
esp_ble_mesh_provisioner_enable();
mesh_keys_bind(netkey_id, appkey_id);
model_bind_vendor(appkey_id);
pubsub_config("PING", pub=G_PING);
pubsub_config("REPORT", pub=G_REPORT, subs=[G_CTRL]);
relay_role_set(true);
gatt_proxy_set(true);
```

> ⚠️ 프로비저닝 완료 후 **AppKey 바인딩 → Pub/Sub 설정** 순으로 진행해야 한다.

> 순서가 어긋나면 ESP_BLE_MESH_INVALID_BINDING 오류 발생 가능.

---

## **10. 디버깅 및 로깅**

### **10.1 로그 태그**

| **태그**  | **의미**                 |
| --------- | ------------------------ |
| MESH_INIT | Mesh 초기화 단계         |
| PROV      | Provisioner 관련 이벤트  |
| VENDOR_OP | Vendor Model 패킷 처리   |
| CBOR_ENC  | REPORT 직렬화 처리       |
| BRIDGE    | Proxy → Mother 직렬 출력 |

### **10.2 주요 로그 확인 명령**

```
idf.py monitor | grep REPORT
idf.py monitor | grep PING
```

---

## **11. 운영 팁**

- Provisioner는 1개만 존재해야 한다.
  복수 Provisioner가 있으면 주소 충돌 발생.
- REPORT 손실이 잦을 경우:
  - 스캔 윈도(SCAN_WINDOW)를 줄이거나
  - 릴레이 비율을 20% 이하로 낮춘다.
- esp_ble_mesh_node_reset() 호출 시
  해당 노드는 Unprovisioned 상태로 돌아간다.
- Proxy는 GATT 연결 유지 시간(CONN_SUPERVISION_TIMEOUT)을 6초 이상으로 설정.

---

## **12. Mother 제어 (CTRL 모델 확장)**

나중 확장을 위한 명세이다.

```
typedef struct {
    uint8_t set;        // "schedule" | "ttl" | "relay" | "subscribe" | "time"
    uint8_t value[8];   // 구체적 제어 값
} ctrl_payload_t;
```

Mother는 Mesh로 VM_CTRL을 송신하여 노드의 동작 파라미터를 갱신할 수 있다.

예시:

- set="ttl" → 모든 Anchor의 DEFAULT_TTL 변경
- set="relay" → 특정 Node의 릴레이 토글
- set="time" → 기준 Epoch 동기화 (time_sync_apply() 호출)

---

## **13. 테스트 절차 요약**

| **단계** | **장비** | **설명**                                  |
| -------- | -------- | ----------------------------------------- |
| ①        | Proxy    | Provisioner Enable + GATT Proxy ON        |
| ②        | Anchor   | Unprovisioned → Provisioned + REPORT 설정 |
| ③        | Child    | ADV 송신, Mesh 구독 필요 없음             |
| ④        | Mother   | USB 직렬 연결, JSON 수집                  |
| ⑤        | 검증     | REPORT 수신 확인 + 헬스 메트릭 출력       |

---

## **14. 주요 함수 요약**

| **함수명**                                 | **설명**                 |
| ------------------------------------------ | ------------------------ |
| mesh_init_and_start(node_id, proxy, relay) | BLE Mesh 초기화 및 시작  |
| mesh_keys_bind(netkey_id, appkey_id)       | NetKey / AppKey 바인딩   |
| model_bind_vendor(appkey_id)               | Vendor Model 바인딩      |
| pubsub_config(model, pub, subs)            | Pub/Sub 주소 설정        |
| relay_role_set(enabled)                    | 릴레이 기능 설정         |
| ttl_policy_apply(default_ttl, zone_hint)   | 구역별 TTL 조정          |
| gatt_proxy_set(enabled)                    | GATT Proxy 모드 설정     |
| bridge_emit_json(report)                   | Proxy → Mother 직렬 출력 |
| ctrl_apply(payload)                        | Mother 제어 명령 처리    |

---

**문서 버전:** 1.0

**참조:** DESIGN_OVERVIEW.md, PROTOCOL_V1.md, DEDUP_LOGIC.md, ROADMAP.md
