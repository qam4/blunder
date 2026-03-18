"""Machine information collection for benchmark reproducibility.

Captures CPU model and core count so time-bound benchmark results
can be compared across different hardware.
"""

from __future__ import annotations

import os
import platform
import re
from dataclasses import dataclass


@dataclass
class MachineInfo:
    cpu_model: str  # e.g. "AMD EPYC 9R14" or "Intel Core i7-12700K"
    cpu_cores: int  # logical core count
    os_name: str  # e.g. "Linux", "Darwin", "Windows"


def get_machine_info() -> MachineInfo:
    """Collect CPU model and core count. Best-effort on all platforms."""
    cpu_model = _get_cpu_model()
    cpu_cores = os.cpu_count() or 0
    os_name = platform.system()
    return MachineInfo(cpu_model=cpu_model, cpu_cores=cpu_cores, os_name=os_name)


def _get_cpu_model() -> str:
    """Extract CPU model string. Linux reads /proc/cpuinfo, others use platform."""
    if platform.system() == "Linux":
        try:
            with open("/proc/cpuinfo") as f:
                for line in f:
                    if line.startswith("model name"):
                        return line.split(":", 1)[1].strip()
        except OSError:
            pass
    # Fallback
    return platform.processor() or "unknown"
