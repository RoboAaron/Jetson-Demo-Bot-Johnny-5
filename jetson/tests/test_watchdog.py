"""
Unit tests for JetsonBridge watchdog behaviour.

Run without a robot or serial port:
    pytest jetson/tests/test_watchdog.py -v

All serial I/O is mocked so tests are fully offline.
"""

import os
import sys
import time
import threading

import pytest
from unittest.mock import MagicMock, patch, call

# Make jetson_bridge importable from the jetson/ directory
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
for _p in [os.path.join(_REPO, 'jetson'), os.path.join(_REPO, 'tuning_code')]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

from jetson_bridge import JetsonBridge  # noqa: E402


# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def _make_mock_comms():
    """Return a MagicMock that looks like a connected TeensyComms instance."""
    m = MagicMock()
    m.connected = True
    m.connect.return_value = True
    m.disconnect.return_value = None
    m.find_devices.return_value = ['/dev/ttyACM_MOCK']
    m.device_info = {'/dev/ttyACM_MOCK': 'Teensy 4.1 USB Serial'}
    m.get_data.return_value = {
        'connected': True,
        'imu': {
            'roll': 0.0, 'pitch': 0.0, 'yaw': 0.0,
            'velocity_actual': 0.0, 'velocity_setpoint': 0.0,
        },
        'tuning': {},
        'comm': {},
        'history': [],
        'last_update': time.time(),
        'roll_sign_inverted': False,
        'motor_directions_swapped': False,
    }
    m.send_jetson_command.return_value = True
    return m


# ─────────────────────────────────────────────────────────────────────────────
# Watchdog tests
# ─────────────────────────────────────────────────────────────────────────────

class TestWatchdog:
    """Watchdog must send (0.0, 0.0) when no command arrives within the timeout."""

    def test_watchdog_fires_after_timeout(self):
        """After watchdog_timeout with no commands, bridge sends a halt."""
        TIMEOUT = 0.20  # seconds (keep short for a fast test)

        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(
                device='/dev/ttyACM_MOCK',
                watchdog_timeout=TIMEOUT,
            )
            bridge.start()

            # Send one command to reset the timer, then collect calls after that
            bridge.set_velocity(0.5, 0.0)
            mock_comms.send_jetson_command.reset_mock()

            # Wait for watchdog to fire (2× timeout + margin)
            time.sleep(TIMEOUT * 2 + 0.05)

            bridge.stop()

        halt_calls = [c for c in mock_comms.send_jetson_command.call_args_list
                      if c == call(0.0, 0.0)]
        assert halt_calls, (
            f"Expected at least one halt(0.0, 0.0) call, "
            f"got: {mock_comms.send_jetson_command.call_args_list}"
        )

    def test_watchdog_does_not_fire_while_commands_flow(self):
        """Watchdog must NOT halt while /cmd_vel commands arrive regularly."""
        TIMEOUT = 0.30
        CMD_INTERVAL = 0.05   # 20 Hz — well within the timeout
        TEST_DURATION = 0.50

        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(
                device='/dev/ttyACM_MOCK',
                watchdog_timeout=TIMEOUT,
            )
            bridge.start()
            mock_comms.send_jetson_command.reset_mock()

            # Keep feeding commands
            t_end = time.time() + TEST_DURATION
            while time.time() < t_end:
                bridge.set_velocity(0.3, 0.0)
                time.sleep(CMD_INTERVAL)

            # Allow half a watchdog cycle before checking
            time.sleep(TIMEOUT * 0.5)

            # Collect any halt calls that happened DURING the command stream
            halt_calls = [c for c in mock_comms.send_jetson_command.call_args_list
                          if c == call(0.0, 0.0)]
            bridge.stop()

        assert not halt_calls, (
            f"Watchdog fired unexpectedly during active command stream: {halt_calls}"
        )

    def test_watchdog_fires_again_after_commands_stop(self):
        """Watchdog keeps re-firing as long as the bridge is running and silent."""
        TIMEOUT = 0.15

        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(
                device='/dev/ttyACM_MOCK',
                watchdog_timeout=TIMEOUT,
            )
            bridge.start()

            # Let commands flow briefly, then stop
            bridge.set_velocity(0.2, 0.1)
            time.sleep(0.05)
            mock_comms.send_jetson_command.reset_mock()

            # Silence — watchdog should fire multiple times
            time.sleep(TIMEOUT * 4)
            bridge.stop()

        halt_count = sum(
            1 for c in mock_comms.send_jetson_command.call_args_list
            if c == call(0.0, 0.0)
        )
        # Watchdog loop sleeps 100 ms; expect at least 2 halt calls in 4×TIMEOUT
        assert halt_count >= 2, (
            f"Expected multiple halt calls, got {halt_count}: "
            f"{mock_comms.send_jetson_command.call_args_list}"
        )


# ─────────────────────────────────────────────────────────────────────────────
# Halt command
# ─────────────────────────────────────────────────────────────────────────────

class TestHalt:

    def test_halt_sends_zero_velocity(self):
        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(device='/dev/ttyACM_MOCK', watchdog_timeout=10.0)
            bridge.start()
            result = bridge.halt()
            bridge.stop()

        assert result is True
        mock_comms.send_jetson_command.assert_any_call(0.0, 0.0)

    def test_stop_sends_halt_before_disconnect(self):
        """bridge.stop() must send a final zero-command before disconnecting."""
        mock_comms = _make_mock_comms()
        call_order = []
        mock_comms.send_jetson_command.side_effect = lambda v, s: call_order.append(('cmd', v, s)) or True
        mock_comms.disconnect.side_effect = lambda: call_order.append(('disconnect',))

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(device='/dev/ttyACM_MOCK', watchdog_timeout=10.0)
            bridge.start()
            bridge.stop()

        # The final halt must come before the disconnect
        assert ('cmd', 0.0, 0.0) in call_order, f"No halt in: {call_order}"
        halt_idx = next(i for i, e in enumerate(call_order) if e == ('cmd', 0.0, 0.0))
        disc_idx  = call_order.index(('disconnect',))
        assert halt_idx < disc_idx, (
            f"Halt ({halt_idx}) must precede disconnect ({disc_idx}) in: {call_order}"
        )


# ─────────────────────────────────────────────────────────────────────────────
# set_velocity / get_state
# ─────────────────────────────────────────────────────────────────────────────

class TestSetVelocity:

    def test_set_velocity_forwards_to_comms(self):
        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(device='/dev/ttyACM_MOCK', watchdog_timeout=10.0)
            bridge.start()
            ok = bridge.set_velocity(0.4, -0.3)
            bridge.stop()

        assert ok is True
        mock_comms.send_jetson_command.assert_any_call(0.4, -0.3)

    def test_get_state_returns_dict_with_connected_key(self):
        mock_comms = _make_mock_comms()

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            bridge = JetsonBridge(device='/dev/ttyACM_MOCK', watchdog_timeout=10.0)
            bridge.start()
            state = bridge.get_state()
            bridge.stop()

        assert isinstance(state, dict)
        assert 'connected' in state
        assert state['connected'] is True


# ─────────────────────────────────────────────────────────────────────────────
# Reconnect logic (lightweight — just checks the plumbing, not timing)
# ─────────────────────────────────────────────────────────────────────────────

class TestReconnect:

    def test_disconnect_event_triggers_reconnect_attempt(self):
        """Simulating a serial drop should cause bridge to attempt reconnect."""
        mock_comms = _make_mock_comms()
        reconnect_attempts = []

        original_connect = mock_comms.connect.side_effect

        def track_connect(device):
            reconnect_attempts.append(device)
            mock_comms.connected = True
            return True

        mock_comms.connect.side_effect = track_connect

        with patch('jetson_bridge.TeensyComms', return_value=mock_comms):
            import jetson_bridge as jb_mod
            orig_interval = jb_mod.RECONNECT_INTERVAL_S

            # Speed up reconnect interval for testing
            jb_mod.RECONNECT_INTERVAL_S = 0.1
            try:
                bridge = JetsonBridge(device='/dev/ttyACM_MOCK', watchdog_timeout=10.0)
                bridge.start()
                reconnect_attempts.clear()  # ignore initial connect

                # Simulate link loss
                mock_comms.connected = False
                bridge._on_disconnect()

                # Give reconnect thread time to fire
                time.sleep(0.5)
                bridge.stop()
            finally:
                jb_mod.RECONNECT_INTERVAL_S = orig_interval

        assert reconnect_attempts, "Expected at least one reconnect attempt after disconnect"
