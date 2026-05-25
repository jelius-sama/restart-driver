A small utility to restart unstable WiFi and Bluetooth kernel modules
on Asahi Linux. The brcmfmac and hci_bcm4377 drivers can enter broken
states where WiFi stops scanning or associating, and Bluetooth fails to
reinitialize after being disabled.

Rather than manually running modprobe/rmmod/ip/systemctl sequences,
this tool wraps the full restart sequence for each device behind a
single command.

For WiFi, the full sequence is:
  - Stop iwd
  - Bring wlp1s0f0 down
  - Unload brcmfmac_wcc, brcmutil, brcmfmac
  - Reload brcmfmac
  - Start iwd
  - Wait for wlp1s0f0 to appear in sysfs before bringing it up

The sysfs poll on /sys/class/net/<iface> avoids a race between iwd
registering the interface and the subsequent ip-link call.

For Bluetooth, hci_bcm4377 is cycled via rmmod/modprobe as the
driver does not recover gracefully from a soft disable.

Requires root. Must be run as: sudo restart-driver <wifi|bluetooth>
