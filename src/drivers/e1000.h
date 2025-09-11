#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../pci/pci.h"
#include "../network/netdev.h"

// E1000 Register Offsets
#define E1000_CTRL      0x00000  // Device Control
#define E1000_STATUS    0x00008  // Device Status
#define E1000_EECD      0x00010  // EEPROM Control
#define E1000_EERD      0x00014  // EEPROM Read
#define E1000_ICR       0x000C0  // Interrupt Cause Read
#define E1000_ITR       0x000C4  // Interrupt Throttling Rate
#define E1000_ICS       0x000C8  // Interrupt Cause Set
#define E1000_IMS       0x000D0  // Interrupt Mask Set
#define E1000_IMC       0x000D8  // Interrupt Mask Clear
#define E1000_RCTL      0x00100  // RX Control
#define E1000_RDBAL     0x02800  // RX Descriptor Base Address Low
#define E1000_RDBAH     0x02804  // RX Descriptor Base Address High
#define E1000_RDLEN     0x02808  // RX Descriptor Length
#define E1000_RDH       0x02810  // RX Descriptor Head
#define E1000_RDT       0x02818  // RX Descriptor Tail
#define E1000_TCTL      0x00400  // TX Control
#define E1000_TDBAL     0x03800  // TX Descriptor Base Address Low
#define E1000_TDBAH     0x03804  // TX Descriptor Base Address High
#define E1000_TDLEN     0x03808  // TX Descriptor Length
#define E1000_TDH       0x03810  // TX Descriptor Head
#define E1000_TDT       0x03818  // TX Descriptor Tail
#define E1000_RAL       0x05400  // Receive Address Low
#define E1000_RAH       0x05404  // Receive Address High

// Control Register bits
#define E1000_CTRL_FD       0x00000001  // Full Duplex
#define E1000_CTRL_LRST     0x00000008  // Link Reset
#define E1000_CTRL_ASDE     0x00000020  // Auto Speed Detection Enable
#define E1000_CTRL_SLU      0x00000040  // Set Link Up
#define E1000_CTRL_ILOS     0x00000080  // Invert Loss of Signal
#define E1000_CTRL_SPD_SEL  0x00000300  // Speed Selection
#define E1000_CTRL_SPD_10   0x00000000  // 10 Mbps
#define E1000_CTRL_SPD_100  0x00000100  // 100 Mbps
#define E1000_CTRL_SPD_1000 0x00000200  // 1000 Mbps
#define E1000_CTRL_FRCSPD   0x00000800  // Force Speed
#define E1000_CTRL_FRCDPLX  0x00001000  // Force Duplex
#define E1000_CTRL_RST      0x04000000  // Device Reset
#define E1000_CTRL_VME      0x40000000  // VLAN Mode Enable

// Receive Control Register bits
#define E1000_RCTL_EN       0x00000002  // Receive Enable
#define E1000_RCTL_SBP      0x00000004  // Store Bad Packets
#define E1000_RCTL_UPE      0x00000008  // Unicast Promiscuous Enabled
#define E1000_RCTL_MPE      0x00000010  // Multicast Promiscuous Enabled
#define E1000_RCTL_LPE      0x00000020  // Long Packet Reception Enable
#define E1000_RCTL_LBM_NO   0x00000000  // No Loopback
#define E1000_RCTL_RDMTS_HALF 0x00000000  // RX Descriptor Minimum Threshold Size
#define E1000_RCTL_BAM      0x00008000  // Broadcast Accept Mode
#define E1000_RCTL_SZ_2048  0x00000000  // RX Buffer Size 2048
#define E1000_RCTL_SECRC    0x04000000  // Strip Ethernet CRC

// Transmit Control Register bits
#define E1000_TCTL_EN       0x00000002  // Transmit Enable
#define E1000_TCTL_PSP      0x00000008  // Pad Short Packets
#define E1000_TCTL_CT       0x00000ff0  // Collision Threshold
#define E1000_TCTL_COLD     0x003ff000  // Collision Distance
#define E1000_TCTL_RTLC     0x01000000  // Re-transmit on Late Collision

// Interrupt bits
#define E1000_ICR_TXDW      0x00000001  // TX Descriptor Written Back
#define E1000_ICR_TXQE      0x00000002  // TX Queue Empty
#define E1000_ICR_LSC       0x00000004  // Link Status Change
#define E1000_ICR_RXSEQ     0x00000008  // RX Sequence Error
#define E1000_ICR_RXDMT0    0x00000010  // RX Descriptor Minimum Threshold Reached
#define E1000_ICR_RXO       0x00000040  // RX Overrun
#define E1000_ICR_RXT0      0x00000080  // RX Timer Interrupt

// Descriptor constants
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32
#define E1000_BUFFER_SIZE   2048

// RX Descriptor Status bits
#define E1000_RXD_STAT_DD   0x01  // Descriptor Done
#define E1000_RXD_STAT_EOP  0x02  // End of Packet

// TX Descriptor Command bits
#define E1000_TXD_CMD_EOP   0x01  // End of Packet
#define E1000_TXD_CMD_IFCS  0x02  // Insert FCS
#define E1000_TXD_CMD_RS    0x08  // Report Status

// TX Descriptor Status bits
#define E1000_TXD_STAT_DD   0x01  // Descriptor Done

// Descriptor structures
typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

// E1000 device structure
typedef struct {
    pci_device_t *pci_dev;
    uintptr_t mmio_base;
    uint8_t mac_address[6];
    
    // RX ring
    e1000_rx_desc_t *rx_desc;
    uint8_t **rx_buffers;
    uint16_t rx_cur;
    
    // TX ring
    e1000_tx_desc_t *tx_desc;
    uint8_t **tx_buffers;
    uint16_t tx_cur;
    
    network_interface_t *netif;
} e1000_device_t;

// Function declarations
int e1000_init(void);
int e1000_register_netdev(void);
int e1000_probe(pci_device_t *pci_dev);
void e1000_reset(e1000_device_t *dev);
void e1000_enable_interrupts(e1000_device_t *dev);
void e1000_disable_interrupts(e1000_device_t *dev);
int e1000_rx_init(e1000_device_t *dev);
int e1000_tx_init(e1000_device_t *dev);
void e1000_read_mac_address(e1000_device_t *dev);
uint32_t e1000_read_reg(e1000_device_t *dev, uint32_t reg);
void e1000_write_reg(e1000_device_t *dev, uint32_t reg, uint32_t value);
int e1000_send_packet(network_interface_t *iface, void *data, size_t len);
int e1000_receive_packet(network_interface_t *iface, void *buffer, size_t max_len);
void e1000_interrupt_handler(e1000_device_t *dev);

#endif // E1000_H
