# Normal G-Buffer — 6C단계 저장 노멀 vs 깊이 복원 노멀

> Branch: `feature/normal-gbuffer` · Status: in-progress · Updated: 2026-11-07
> waterfront 업그레이드 6C단계. 앞 단계: [../contact-foam/NOTES.md](../contact-foam/NOTES.md)

## 1. Goal

- **한 문장 요약:** 불투명 패스가 색과 함께 **월드 노멀을 별도 버퍼에 저장**(G-Buffer의 노멀 한 장)하고, 물 패스가 6B의 "깊이에서 추정한 노멀" 대신 그것을 바로 읽을 수 있게 해서 **두 방식을 같은 샷에서 화질과 성능으로 비교**한다.
- **범위 (사용자 결정 2026-11-07):** 비교 측정과 나중에 참고할 캡처만 하고 끝낸다. 거품 모양은 바꾸지 않는다 (수면 고리는 언리얼 거리장 단계).
- 성능 측정은 6B·6C에서만 한다.

## 2. 방법

```text
불투명 패스 (테스트 장면 물체):  SV_Target0 = HDR 색   SV_Target1 = 월드 노멀 × 0.5 + 0.5, a = 1 (노멀 있음)
                                  ↑ 두 장에 동시에 그린다 (MRT, Multiple Render Targets)
물 패스:  노멀 출처 = Depth reconstructed → 6B: 깊이 4번 읽기 + 위치 복원 4번 + 외적
                     Stored scene normal  → 노멀 버퍼 1번 읽기
```

| 항목 | 결정 |
|---|---|
| 노멀 버퍼 형식 | `R10G10B10A2_UNORM` (채널당 10비트, 알파 2비트 = 노멀 있음 표시). 매 프레임 0으로 지움 → 하늘·물 없는 곳 = 노멀 없음 |
| 쓰는 물체 | 테스트 장면 물체만. 하늘은 노멀을 쓰지 않으므로 MRT는 물체를 그리는 동안만 묶는다 |
| 노멀 버퍼 끔 | Depth reconstructed를 고르면 노멀 버퍼를 묶지도 쓰지도 않는다 → 6B와 같은 비용 |

**디버그 뷰**

| 번호 | 이름 | 내용 |
|---|---|---|
| 22 | Normal from depth | 6B 그대로 (깊이 복원) |
| 23 | Stored scene normal | 노멀 버퍼 그대로 (같은 색 규칙) |
| 24 | Normal difference | 두 노멀의 각도 차이 (검정 0° → 흰색 30° 이상) |

## 3. Inputs / Outputs

| 종류 | 이름 | 비고 |
|---|---|---|
| Texture | 장면 노멀 버퍼 (t6) | HDR 타깃과 같은 크기, 리사이즈 때 함께 다시 만듦 |
| CBuffer | `g_SceneParams.w` | 노멀 출처 (0 깊이 복원, 1 저장 노멀) |
| ImGui (Water 창, New: Contact Foam) | Normal source | Depth reconstructed / Stored scene normal (G-Buffer) |
| CLI | `--normal-source depth\|stored` | 캡처·측정용 |

## 4. Acceptance Criteria

**에이전트 검증**

- [ ] 빌드 경고 0, 디버그 레이어 경고 0, 기존 샷 21장 바이트 동일
- [ ] Depth reconstructed = 6B 화면과 바이트 동일
- [ ] 저장 노멀(23) vs 정답지 법선(Scene Debug 4): 모든 측정 픽셀에서 오차가 저장 정밀도 수준
- [ ] 깊이 복원(22) vs 저장(23) 차이 지도와 거품 차이를 같은 6샷에서 캡처 (참고용)
- [ ] 성능: 샷별 패스 GPU 시간 — 불투명 구간(노멀 쓰기 비용)과 물 구간(복원 계산 vs 한 번 읽기), 실제 거품 사용과 전 화면 노멀(디버그 22 vs 23) 두 경우

**사용자 검증**

- [ ] Normal source를 바꿔 가며 모서리·레일·기둥에서 거품과 노멀이 어떻게 달라지는지 본다
- [ ] 두 방식의 비용 차이(노멀 버퍼 메모리·쓰기 vs 픽셀당 계산)를 숫자로 설명할 수 있다

## 5. Notes

- 언리얼 대응: 디퍼드 렌더러의 GBufferA가 월드 노멀을 담는다. 15단계(RenderDoc)에서 직접 찾아본다.
