# trace_plonk.py
from __future__ import annotations
import json, time, functools, contextlib
from dataclasses import dataclass, asdict
from typing import Any, Dict, List, Optional, Tuple

try:
    import cupy as cp
    _HAS_CUPY = True
except Exception:
    _HAS_CUPY = False

try:
    import torch
    _HAS_TORCH = True
    torch.cuda.synchronize()
except Exception:
    _HAS_TORCH = False

# ===== 基本数据结构 =====
@dataclass
class BufferInfo:
    name: str
    kind: str           # "field", "curve", "bytes", "unknown"
    shape: Tuple[int, ...]
    dtype: str
    nbytes: int
    inplace: bool = False

@dataclass
class StageRecord:
    name: str
    inputs: List[BufferInfo]
    outputs: List[BufferInfo]
    t_ms: float
    notes: Optional[str] = None

class TraceState:
    def __init__(self):
        self.stages: List[StageRecord] = []
        self._last_producer: Dict[str, str] = {}  # buffer_name -> stage_name
        self.edges: List[Tuple[str, str, str]] = []  # (producer, consumer, buffer)
        self._cur_stage_inputs: List[BufferInfo] = []
        self._cur_stage_name: Optional[str] = None

    def begin_stage(self, name: str):
        self._cur_stage_name = name
        self._cur_stage_inputs = []

    def end_stage(self, record: StageRecord):
        # 记录依赖边
        for buf in record.inputs:
            if buf.name in self._last_producer:
                self.edges.append((self._last_producer[buf.name], record.name, buf.name))
        for buf in record.outputs:
            self._last_producer[buf.name] = record.name
        self.stages.append(record)
        self._cur_stage_name = None
        self._cur_stage_inputs = []

    def add_input(self, info: BufferInfo):
        self._cur_stage_inputs.append(info)

STATE = TraceState()

# ===== 工具：识别张量/数组，统计字节 =====
def _as_buffer(name: str, x: Any, inplace=False) -> BufferInfo:
    # Cupy
    if _HAS_CUPY and isinstance(x, cp.ndarray):
        return BufferInfo(name, guess_kind(name, x), tuple(x.shape), str(x.dtype), int(x.nbytes), inplace)
    # Torch
    if _HAS_TORCH and isinstance(x, torch.Tensor):
        nbytes = x.numel() * x.element_size()
        return BufferInfo(name, guess_kind(name, x), tuple(x.shape), str(x.dtype), int(nbytes), inplace)
    # Numpy 或 bytes
    try:
        import numpy as np
        if isinstance(x, np.ndarray):
            return BufferInfo(name, guess_kind(name, x), tuple(x.shape), str(x.dtype), int(x.nbytes), inplace)
    except Exception:
        pass
    if isinstance(x, (bytes, bytearray)):
        return BufferInfo(name, "bytes", (len(x),), "byte", len(x), inplace)
    return BufferInfo(name, "unknown", (), type(x).__name__, 0, inplace)

# 根据名字/形状做一个经验分类（可按需改造）
def guess_kind(name: str, arr: Any) -> str:
    n = name.lower()
    if any(k in n for k in ["srs", "twiddle", "omega", "root"]):
        return "const"
    if any(k in n for k in ["g1", "g2", "point", "ec", "msm"]):
        return "curve"
    if any(k in n for k in ["poly", "coeff", "eval", "field", "scalar"]):
        return "field"
    if any(k in n for k in ["transcript", "challenge", "hash", "proof", "bytes"]):
        return "bytes"
    return "unknown"

# ===== GPU/CPU 计时器（自动选择） =====
@contextlib.contextmanager
def _timer_gpu_cpu():
    use_gpu = False
    if _HAS_CUPY and cp.cuda.runtime.getDeviceCount() > 0:
        start = cp.cuda.Event(); end = cp.cuda.Event()
        start.record(); yield
        end.record(); end.synchronize()
        t_ms = cp.cuda.get_elapsed_time(start, end)
        return
    if _HAS_TORCH and torch.cuda.is_available():
        torch.cuda.synchronize()
        t0 = time.perf_counter(); yield
        torch.cuda.synchronize()
        t_ms = (time.perf_counter() - t0) * 1e3
        return
    # 纯 CPU
    t0 = time.perf_counter(); yield
    t_ms = (time.perf_counter() - t0) * 1e3
    return

# 简易：返回毫秒
def time_stage(fn, *args, **kwargs) -> Tuple[Any, float]:
    if _HAS_CUPY and cp.cuda.runtime.getDeviceCount() > 0:
        start = cp.cuda.Event(); end = cp.cuda.Event()
        start.record(); out = fn(*args, **kwargs); end.record(); end.synchronize()
        return out, float(cp.cuda.get_elapsed_time(start, end))
    if _HAS_TORCH and torch.cuda.is_available():
        torch.cuda.synchronize()
        t0 = time.perf_counter(); out = fn(*args, **kwargs)
        torch.cuda.synchronize()
        return out, float((time.perf_counter()-t0)*1e3)
    t0 = time.perf_counter(); out = fn(*args, **kwargs)
    return out, float((time.perf_counter()-t0)*1e3)

# ===== 装饰器：标记阶段，并声明入/出 =====
def stage(name: str, inputs: Dict[str, Any], outputs: Dict[str, Any], notes: str = None):
    """
    用法：
    @stage("NTT", inputs=lambda a: {"a": a}, outputs=lambda A: {"A": A})
    def ntt(a): ...
    """
    def wrapper(fn):
        @functools.wraps(fn)
        def inner(*args, **kwargs):
            STATE.begin_stage(name)
            # 先把入参转成 BufferInfo
            in_map = inputs(*args, **kwargs) if callable(inputs) else inputs
            in_bufs = [_as_buffer(k, v) for k, v in in_map.items()]
            for b in in_bufs:
                STATE.add_input(b)
            # 计时执行
            out, t_ms = time_stage(fn, *args, **kwargs)
            # 将输出也转成 BufferInfo（支持函数从返回值里取）
            out_map = outputs(out, *args, **kwargs) if callable(outputs) else outputs
            out_bufs = [_as_buffer(k, v) for k, v in out_map.items()]
            STATE.end_stage(StageRecord(name, in_bufs, out_bufs, t_ms, notes))
            return out
        return inner
    return wrapper

# 手动登记（适合难以装饰的位置）
def mark_stage(name: str, inputs: Dict[str, Any], outputs: Dict[str, Any], t_ms: float, notes: str = None):
    in_bufs = [_as_buffer(k, v) for k, v in inputs.items()]
    out_bufs = [_as_buffer(k, v) for k, v in outputs.items()]
    for b in in_bufs:
        STATE.add_input(b)
    STATE.end_stage(StageRecord(name, in_bufs, out_bufs, float(t_ms), notes))

def dump_trace_json(path="plonk_trace.json", extra_meta: Dict[str, Any] = None):
    data = {
        "pipeline": [asdict(s) for s in STATE.stages],
        "edges": list(STATE.edges),
        "meta": extra_meta or {},
    }
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    return path
