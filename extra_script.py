"""
cdc-dfu — PlatformIO library extra script.

Loaded automatically via library.json build.extraScript.

1. Compiles cdc_dfu.cpp into the firmware (LDF can't auto-detect it
   since nothing #includes from the library).
2. When upload_protocol = dfu, injects a DTR trigger pre-action so the
   device enters ROM DFU bootloader before dfu-util runs.

No manual extra_scripts or #include needed.
"""

Import("env")

import os
from SCons.Script import DefaultEnvironment

# Must use DefaultEnvironment() — the library gets a cloned env,
# but delayed actions (AddPreAction) are only processed on the real project env.
_project_env = DefaultEnvironment()

# ── Force-compile library source into the firmware ──────────────────────
# PlatformIO LDF won't build libraries that nothing #includes from, so we
# compile the source directly via the project environment.
# Dir('.') in SCons SConscript context gives the directory of this script.
from SCons.Script import Dir
_lib_dir = Dir('.').abspath
_src_dir = os.path.join(_lib_dir, "src")
_src_file = os.path.join(_src_dir, "cdc_dfu.cpp")

if os.path.isfile(_src_file):
    # Add the DTR_TOGGLING_SEQ define (normally from library.json build.flags,
    # but LDF skips our library so those flags aren't propagated).
    _project_env.Append(CPPDEFINES=["DTR_TOGGLING_SEQ"])
    _project_env.BuildSources(
        os.path.join("$BUILD_DIR", "cdc-dfu"),
        _src_dir
    )

# ── DFU trigger pre-action ──────────────────────────────────────────────
_upload_protocol = _project_env.subst("$UPLOAD_PROTOCOL")

if _upload_protocol == "dfu":

    def _trigger_dfu_before_upload(source, target, env):
        """Toggle DTR + send magic bytes to enter DFU bootloader."""
        import time
        import serial
        import serial.tools.list_ports

        VID, PID_CDC = 0x0483, 0x5740
        TIMEOUT = 5

        def find_cdc_port():
            for p in serial.tools.list_ports.comports():
                if p.vid == VID and p.pid == PID_CDC:
                    return p.device
            return None

        port = find_cdc_port()
        if port is None:
            print("No STM32 CDC device found - assuming already in DFU mode.")
            return

        print("Triggering DFU bootloader on %s ..." % port)
        try:
            s = serial.Serial(port, 115200, timeout=0.5, dsrdtr=False)
            for _ in range(5):
                s.dtr = not s.dtr
                time.sleep(0.05)
            s.dtr = False
            time.sleep(0.05)
            s.write(b"DFU!")
            s.flush()
        except serial.SerialException:
            pass
        finally:
            try:
                s.close()
            except Exception:
                pass

        # Wait for CDC port to disappear (device entering DFU mode)
        t0 = time.time()
        while time.time() - t0 < TIMEOUT:
            if find_cdc_port() is None:
                time.sleep(1)  # Settle delay for DFU USB enumeration
                return
            time.sleep(0.1)
        print("WARNING: device did not enter DFU mode within timeout.")

    _project_env.AddPreAction("upload", _trigger_dfu_before_upload)
