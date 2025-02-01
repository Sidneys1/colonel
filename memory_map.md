# Memory Map

(As of 2024-02-06)

|    From    |     To     |    Size | Free | Devicetree Path             | Description |
|-----------:|:-----------|--------:|:----:|:----------------------------|:------------|
|`0x00000000`|`0x00100000`|   1 `M` |  ✓   |
|`0x00100000`|`0x00101000`|   4 `K` |      | /soc/test                   | SiFive "Test Finisher" device |
|`0x00101000`|`0x00102000`|   4 `K` |      | /soc/rtc                    | Real-time clock |
|`0x00102000`|`0x02000000`|  30 `M` |  ✓   |
|`0x02000000`|`0x02010000`|  64 `K` |      | /soc/clint                  | Core-local interrupts |
|`0x02010000`|`0x0c000000`| 159 `M` |  ✓   |
|`0x0c000000`|`0x0c210000`|   2 `M` |      | /soc/plic                   | Interrupt controller |
|`0x0c210000`|`0x10000000`|  61 `M` |  ✓   |
|`0x10000000`|`0x10000100`| 256 `B` |      | /soc/uart                   | I/O UART |
|`0x10001000`|`0x10002000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10002000`|`0x10003000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10003000`|`0x10004000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10004000`|`0x10005000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10005000`|`0x10006000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10006000`|`0x10007000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10007000`|`0x10008000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10008000`|`0x10009000`|   4 `K` |      | /soc/virtio_mmio            | For legacy virtio support |
|`0x10009000`|`0x10100000`| 988 `K` |  ✓   |
|`0x10100000`|`0x10100018`|  24 `B` |      | /fw-cfg                     |
|`0x10100018`|`0x20000000`| 254 `M` |  ✓   |
|`0x20000000`|`0x22000000`|  32 `M` |      | /flash                      |
|`0x22000000`|`0x30000000`| 224 `M` |  ✓   |
|`0x30000000`|`0x40000000`| 256 `M` |      | /soc/pci                    |
|`0x40000000`|`0x80000000`|   1 `G` |  ✓   |
|`0x80000000`|`0x88000000`| 128 `M` |      | /memory                     |
|`0x80000000`|`0x80040000`| 256 `K` |      | /reserved-memory/mmode_resv | Machine-mode firmware memory |

<!-- PCI child-bus mappings:
child-bus-address=0x01000002000000,parent-bus-address=0x40000000,length=0x40000000, child-bus-address=0x40000000,parent-bus-address=0x0300000003,length=0x03, child-bus-address=0x01,parent-bus-address=0x03,length=0x1060, child-bus-address=0x30000000,parent-bus-address=0x10000000,length=0x030, child-bus-address=0xf803,parent-bus-address=0x08ee,length=0xff, child-bus-address=0x0304,parent-bus-address=0xdd0,length=0x0304, child-bus-address=0x6470636900,parent-bus-address=0x0316,length=0x067063692d, child-bus-address=0x686f73742d656361,parent-bus-address=0x6d2d67656e657269,length=0x6300000303, child-bus-address=0x0411,parent-bus-address=0x0203,length=0x048d, child-bus-address=0x0103,parent-bus-address=0x041d,length=0x0302, child-bus-address=0x0176697274,parent-bus-address=0x696f5f6d6d696f40,length=0x3130303038303030, child-bus-address=0x03,parent-bus-address=0x04d2,length=0x0803, child-bus-address=0x04c1,parent-bus-address=0x0503,length=0x1060, child-bus-address=0x10008000,parent-bus-address=0x1000,length=0x030c
 -->