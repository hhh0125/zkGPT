# fields/fr.py
# Facade for Fr: forwards attribute access to the active curve module.
from . import _get_active_field

def __getattr__(attr: str):
    """
    Forward attribute access for Fr.
    Priority:
      1) top-level symbol in the active curve module (e.g., zero/one if exposed);
      2) attribute/method on the curve module's Fr shell/class.
    This supports both styles seamlessly.
    """
    impl = _get_active_field()

    # If the curve module exposes the symbol at top level, prefer it.
    if hasattr(impl, attr):
        return getattr(impl, attr)

    # Otherwise, fall back to the Fr shell/class in the module.
    Fr = getattr(impl, "Fr")  # raises if curve module doesn't export Fr
    return getattr(Fr, attr) if attr != "Fr" else Fr
