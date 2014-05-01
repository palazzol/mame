Current h8 work:
- adding watchdog
- adding dma
- adding dtc

Also starts of adding the serial on the tmp68301

TODO before mainline commit:
- finish the implementation.  I forgot what's missing, but at a minimum:
  - cybikoxt is waiting on a dma register
  - I'm certain the hookups are missing in most h8 cpus
  - after that, drop the verbosity levels to "bearable"

- find out why namco system 23 bootup suddendly became very
  unreliable, and fix it

- either finish the tmp68301 serial (test case = junai series, the
  tmp68301 communicates with a h8) or split it out for later work
