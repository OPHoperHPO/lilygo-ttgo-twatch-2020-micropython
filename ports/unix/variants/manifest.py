freeze_as_mpy('$(MPY_DIR)/tools', 'upip.py')
freeze_as_mpy('$(MPY_DIR)/tools', 'upip_utarfile.py', opt=3)
freeze('$(MPY_DIR)/lib/lv_bindings/driver/linux', 'evdev.py')
