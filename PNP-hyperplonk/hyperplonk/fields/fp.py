# fields/fq.py
# Facade for Fq: forwards attribute access to the active curve module.
from . import _get_active_field

def __getattr__(attr: str):
    """
    Forward attribute access for Fq.
    Priority:
      1) top-level symbol in the active curve module (if provided);
      2) attribute/method on the curve module's Fq shell/class.
    """
    impl = _get_active_field()

    if hasattr(impl, attr):
        return getattr(impl, attr)

    Fp = getattr(impl, "Fp")  # raises if curve module doesn't export Fq
    return getattr(Fp, attr) if attr != "Fp" else Fp
