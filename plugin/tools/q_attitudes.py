from __future__ import annotations

def flat(active_lanes: int = 6) -> dict[int, float]:
    return {i: 0.0 for i in range(1, active_lanes + 1)}

def single_bloomer(bloomer_lane: int, delta_r: float,
                   active_lanes: int = 6) -> dict[int, float]:
    result = {i: 0.0 for i in range(1, active_lanes + 1)}
    result[bloomer_lane] = delta_r
    return result

def asymmetric_relay(arm: list[int], disarm: list[int],
                     arm_delta: float = 0.22,
                     disarm_delta: float = 0.40,
                     active_lanes: int = 6) -> dict[int, float]:
    result = {i: 0.0 for i in range(1, active_lanes + 1)}
    for lane in arm:
        result[lane] = arm_delta
    for lane in disarm:
        result[lane] = -disarm_delta
    return result

def spare_the_air(air_lane: int | list[int], mouth_delta: float = 0.04,
                  active_lanes: int = 6) -> dict[int, float]:
    if isinstance(air_lane, int):
        air_lanes = [air_lane]
    else:
        air_lanes = air_lane
    result = {}
    for i in range(1, active_lanes + 1):
        result[i] = 0.0 if i in air_lanes else mouth_delta
    return result

def uniform_gentle(delta_r: float = 0.07,
                   active_lanes: int = 6) -> dict[int, float]:
    return {i: delta_r for i in range(1, active_lanes + 1)}

def all_negative(delta_r: float = -0.04,
                 active_lanes: int = 6) -> dict[int, float]:
    return {i: delta_r for i in range(1, active_lanes + 1)}

def lurker_snap(corner_roots, lane: int, r_start: float, r_end: float):
    for qi in [2, 3]:
        stage = corner_roots[qi][lane - 1]
        stage[1] = r_end
    return corner_roots

def revoice_tuner(corner_roots, lane_hz_shifts: dict[int, float]):
    for qi in [2, 3]:
        for lane, new_hz in lane_hz_shifts.items():
            stage = corner_roots[qi][lane - 1]
            stage[0] = new_hz
    return corner_roots

def parse_q_spec(spec: str) -> dict[int, float]:
    parts = spec.split(":")
    name = parts[0]

    if name == "flat" or name == "dead_q":
        return flat()

    if name == "single-bloomer":
        lane = int(parts[1])
        delta = float(parts[2]) if len(parts) > 2 else 0.25
        return single_bloomer(lane, delta)

    if name == "asymmetric-relay":
        arm_str = parts[1]
        disarm_str = parts[2]
        arm_delta = float(parts[3]) if len(parts) > 3 else 0.22
        disarm_delta = float(parts[4]) if len(parts) > 4 else 0.40
        arm = [int(x) for x in arm_str.split(",")]
        disarm = [int(x) for x in disarm_str.split(",")]
        return asymmetric_relay(arm, disarm, arm_delta, disarm_delta)

    if name == "spare-the-air":
        air = int(parts[1])
        delta = float(parts[2]) if len(parts) > 2 else 0.04
        return spare_the_air(air, delta)

    if name == "uniform-gentle":
        delta = float(parts[1]) if len(parts) > 1 else 0.07
        return uniform_gentle(delta)

    if name == "all-negative":
        delta = float(parts[1]) if len(parts) > 1 else -0.04
        return all_negative(delta)

    raise ValueError(f"Unknown Q attitude: {name}")
