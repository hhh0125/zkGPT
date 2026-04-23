# hyperplonk/fields/__init__.py
from importlib import import_module
from typing import Optional

_active_field_mod: Optional[object] = None
_active_curve_config: Optional[object] = None  # 注意：保存的是实例，而不是模块

def select(name: str) -> None:
    """
    选择活动曲线。
    """
    global _active_field_mod, _active_curve_config
    key = name.lower()
    if key in {"bls12_381", "fr_bls12_381", "bls12-381"}:
        # 域参数
        _active_field_mod = import_module("hyperplonk.fields.bls12_381")
        # 曲线配置模块
        curve_mod = import_module("hyperplonk.subroutines.src.pcs.ec.bls12_381")
        _active_curve_config = curve_mod.CURVE_CONFIG
    else:
        raise ValueError(f"Unknown curve name: {name}")

def _get_active_field():
    if _active_field_mod is None:
        raise RuntimeError("Active field not selected. Call fields.select(...) first.")
    return _active_field_mod

def _get_active_curve_config():
    if _active_curve_config is None:
        raise RuntimeError("Active curve not selected. Call fields.select(...) first.")
    return _active_curve_config
