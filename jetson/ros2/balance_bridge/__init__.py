"""
balance_bridge — ROS 2 package.

When running from source (not from a colcon install), this __init__ adds
the repo's jetson/ and tuning_code/ directories to sys.path so that
jetson_bridge and teensy_comms are importable without extra setup.

When running from a colcon install the source tree is not present in the
same relative location.  In that case set PYTHONPATH to include both
directories before launching, or use the provided launch file which does
this automatically if BALANCE_BRIDGE_REPO_ROOT is set.
"""

import os as _os
import sys as _sys

_here = _os.path.dirname(_os.path.abspath(__file__))

# jetson/ros2/balance_bridge  →  jetson/ros2  →  jetson  →  repo_root
_repo_root = _os.path.abspath(_os.path.join(_here, '..', '..', '..'))
_candidates = [
    _os.path.join(_repo_root, 'jetson'),
    _os.path.join(_repo_root, 'tuning_code'),
]
for _p in _candidates:
    if _os.path.isdir(_p) and _p not in _sys.path:
        _sys.path.insert(0, _p)
