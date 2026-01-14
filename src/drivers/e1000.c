#include "e1000.h"
#include "../memory/memory.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../graphic/graphic.h"
#include "../debug/debug.h"
#include "../timer/timer.h"

static e1000_device_t e1000_dev;
static bool e1000_initialized = false;

// Note: Descriptors and buffers are allocated dynamically in e1000_rx_init/e1000_tx_init
// using kmalloc for better memory management

// Memory mapping for MMIO using virtual memory manager
static void *map_physical_memory(uintptr_t phys_addr, size_t size) {
    DEBUG_INFO("Mapping E1000 MMIO: phys=0x%lx size=0x%lx\n", phys_addr, size);
    
    void *virt_addr = vmm_map_mmio(phys_addr, size);
    if (virt_addr) {
        DEBUG_INFO("E1000 MMIO mapped to virtual address: %p\n", virt_addr);
    } else {
        DEBUG_ERROR("Failed to map E1000 MMIO region\n");
    }
    
    return virt_addr;
}

uint32_t e1000_read_reg(e1000_device_t *dev, uint32_t reg) {
    if (!dev || dev->mmio_base == 0) {
        return 0xFFFFFFFF;
    }
    
    uintptr_t addr = dev->mmio_base + reg;
    DEBUG_DEBUG("e1000_read_reg: addr=0x%lx\n", addr);
    
    volatile uint32_t *mmio = (volatile uint32_t *)addr;
    uint32_t value = *mmio;
    
    DEBUG_DEBUG("e1000_read_reg: value=0x%08x\n", value);
    return value;
}

void e1000_write_reg(e1000_device_t *dev, uint32_t reg, uint32_t value) {
    if (!dev || dev->mmio_base == 0) {
        return;
    }
    
    volatile uint32_t *mmio = (volatile uint32_t *)(dev->mmio_base + reg);
    *mmio = value;
}

void e1000_reset(e1000_device_t *dev) {
    if (!dev || dev->mmio_base == 0) {
        return;
    }
    
    DEBUG_INFO("E1000: Starting device reset...\n");
    
    // Perform a global reset
    e1000_write_reg(dev, E1000_CTRL, E1000_CTRL_RST);
    
    DEBUG_INFO("E1000: Reset command sent, waiting...\n");
    
    // Wait for reset to complete using timer
    timer_sleep_ms(10);
    
    DEBUG_INFO("E1000: Disabling interrupts...\n");
    
    // Disable interrupts
    e1000_write_reg(dev, E1000_IMC, 0xFFFFFFFF);
    
    // Clear any pending interrupts
    (void)e1000_read_reg(dev, E1000_ICR);
    
    DEBUG_INFO("E1000: Reset complete\n");
}

void e1000_read_mac_address(e1000_device_t *dev) {
    // Read MAC address from EEPROM or registers
    // For simplicity, we'll read from the RAL/RAH registers
    uint32_t ral = e1000_read_reg(dev, E1000_RAL);
    uint32_t rah = e1000_read_reg(dev, E1000_RAH);
    
    DEBUG_DEBUG("RAL=0x%08x RAH=0x%08x\n", ral, rah);
    
    dev->mac_address[0] = (ral >> 0) & 0xFF;
    dev->mac_address[1] = (ral >> 8) & 0xFF;
    dev->mac_address[2] = (ral >> 16) & 0xFF;
    dev->mac_address[3] = (ral >> 24) & 0xFF;
    dev->mac_address[4] = (rah >> 0) & 0xFF;
    dev->mac_address[5] = (rah >> 8) & 0xFF;
    
    // If MAC is all zeros, set a default one
    bool all_zero = true;
    for (int i = 0; i < 6; i++) {
        if (dev->mac_address[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    if (all_zero) {
        DEBUG_WARN("MAC address is all zeros, using default\n");
        // Set a default MAC address
        dev->mac_address[0] = 0x52;
        dev->mac_address[1] = 0x54;
        dev->mac_address[2] = 0x00;
        dev->mac_address[3] = 0x12;
        dev->mac_address[4] = 0x34;
        dev->mac_address[5] = 0x56;
        
        // Write it back to the registers
        ral = dev->mac_address[0] | (dev->mac_address[1] << 8) | 
              (dev->mac_address[2] << 16) | (dev->mac_address[3] << 24);
        rah = dev->mac_address[4] | (dev->mac_address[5] << 8) | (1 << 31); // Valid bit
        
        e1000_write_reg(dev, E1000_RAL, ral);
        e1000_write_reg(dev, E1000_RAH, rah);
    }
}

int e1000_rx_init(e1000_device_t *dev) {
    if (!dev) {
        return -1;
    }
    
    DEBUG_INFO("E1000 RX: Allocating descriptor ring...\n");
    
    // Allocate RX descriptor ring using physical pages (DMA needs physical addresses)
    // For DMA, we allocate physical pages and use them directly
    void *rx_desc_phys = physical_alloc_page();
    if (!rx_desc_phys) {
        DEBUG_ERROR("E1000 RX: Failed to allocate descriptor ring\n");
        return -1;
    }
    
    // Access the descriptor ring via HHDM
    dev->rx_desc = (e1000_rx_desc_t*)PHYS_TO_HHDM(rx_desc_phys);
    memset(dev->rx_desc, 0, PAGE_SIZE);
    
    DEBUG_INFO("E1000 RX: Descriptor ring at phys=0x%lx virt=0x%lx\n", 
               (uint64_t)rx_desc_phys, (uint64_t)dev->rx_desc);
    
    DEBUG_INFO("E1000 RX: Allocating buffer pointer array (%d entries, %zu bytes)...\n", 
               E1000_NUM_RX_DESC, sizeof(uint8_t *) * E1000_NUM_RX_DESC);
    
    // Allocate buffer pointer array (use physical page + HHDM since kmalloc is broken)
    void *buf_array_phys = physical_alloc_page();
    if (!buf_array_phys) {
        DEBUG_ERROR("E1000 RX: Failed to allocate buffer array\n");
        return -1;
    }
    dev->rx_buffers = (uint8_t **)PHYS_TO_HHDM(buf_array_phys);
    memset(dev->rx_buffers, 0, PAGE_SIZE);
    
    DEBUG_INFO("E1000 RX: Buffer pointer array at phys=0x%lx virt=0x%lx\n", 
               (uint64_t)buf_array_phys, (uint64_t)dev->rx_buffers);
    DEBUG_INFO("E1000 RX: Allocating %d packet buffers...\n", E1000_NUM_RX_DESC);
    
    // Initialize RX descriptors and buffers
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        // Allocate physical page for each buffer (DMA needs physical addresses)
        void *buf_phys = physical_alloc_page();
        if (!buf_phys) {
            DEBUG_ERROR("E1000 RX: Failed to allocate buffer %d\n", i);
            return -1;
        }
        
        // Store virtual address for kernel access
        dev->rx_buffers[i] = (uint8_t *)PHYS_TO_HHDM(buf_phys);
        
        // Hardware descriptor uses PHYSICAL address
        dev->rx_desc[i].buffer_addr = (uint64_t)buf_phys;
        dev->rx_desc[i].status = 0;
    }
    
    dev->rx_cur = 0;
    
    DEBUG_INFO("E1000 RX: Setting up hardware registers...\n");
    
    // Set up RX registers with PHYSICAL addresses
    if (dev->mmio_base != 0) {
        // Descriptor ring physical address (64-bit split into high/low)
        e1000_write_reg(dev, E1000_RDBAH, 0); // High 32 bits (assuming < 4GB)
        e1000_write_reg(dev, E1000_RDBAL, (uint32_t)(uintptr_t)rx_desc_phys);
        e1000_write_reg(dev, E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
        e1000_write_reg(dev, E1000_RDH, 0);
        e1000_write_reg(dev, E1000_RDT, E1000_NUM_RX_DESC - 1);
        
        // Configure RX control
        uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 | 
                        E1000_RCTL_SECRC | E1000_RCTL_UPE | E1000_RCTL_MPE;
        e1000_write_reg(dev, E1000_RCTL, rctl);
    }
    
    DEBUG_INFO("E1000 RX: Initialization complete\n");
    
    return 0;
}

int e1000_tx_init(e1000_device_t *dev) {
    DEBUG_INFO("E1000 TX: Allocating descriptor ring...\n");
    
    // Allocate TX descriptor ring using physical pages (DMA needs physical addresses)
    void *tx_desc_phys = physical_alloc_page();
    if (!tx_desc_phys) {
        DEBUG_ERROR("E1000 TX: Failed to allocate descriptor ring\n");
        return -1;
    }
    
    // Access the descriptor ring via HHDM
    dev->tx_desc = (e1000_tx_desc_t*)PHYS_TO_HHDM(tx_desc_phys);
    memset(dev->tx_desc, 0, PAGE_SIZE);
    
    DEBUG_INFO("E1000 TX: Descriptor ring at phys=0x%lx virt=0x%lx\n", 
               (uint64_t)tx_desc_phys, (uint64_t)dev->tx_desc);
    
    // Allocate buffer pointer array (use physical page + HHDM since kmalloc is broken)
    void *tx_buf_array_phys = physical_alloc_page();
    if (!tx_buf_array_phys) {
        DEBUG_ERROR("E1000 TX: Failed to allocate buffer array\n");
        return -1;
    }
    dev->tx_buffers = (uint8_t **)PHYS_TO_HHDM(tx_buf_array_phys);
    memset(dev->tx_buffers, 0, PAGE_SIZE);
    
    DEBUG_INFO("E1000 TX: Buffer pointer array at phys=0x%lx virt=0x%lx\n", 
               (uint64_t)tx_buf_array_phys, (uint64_t)dev->tx_buffers);
    DEBUG_INFO("E1000 TX: Allocating %d packet buffers...\n", E1000_NUM_TX_DESC);
    
    // Initialize TX descriptors and buffers
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        // Allocate physical page for each buffer
        void *buf_phys = physical_alloc_page();
        if (!buf_phys) {
            DEBUG_ERROR("E1000 TX: Failed to allocate buffer %d\n", i);
            return -1;
        }
        
        // Store virtual address for kernel access
        dev->tx_buffers[i] = (uint8_t *)PHYS_TO_HHDM(buf_phys);
        
        // Hardware descriptor uses PHYSICAL address
        dev->tx_desc[i].buffer_addr = (uint64_t)buf_phys;
        dev->tx_desc[i].status = E1000_TXD_STAT_DD; // Mark as done initially
    }
    
    dev->tx_cur = 0;
    
    DEBUG_INFO("E1000 TX: Setting up hardware registers...\n");
    
    // Set up TX registers with PHYSICAL addresses
    e1000_write_reg(dev, E1000_TDBAH, 0);
    e1000_write_reg(dev, E1000_TDBAL, (uint32_t)(uintptr_t)tx_desc_phys);
    e1000_write_reg(dev, E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write_reg(dev, E1000_TDH, 0);
    e1000_write_reg(dev, E1000_TDT, 0);
    
    // Configure TX control
    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP | 
                    (0x10 << 4) | // Collision Threshold
                    (0x40 << 12); // Collision Distance
    e1000_write_reg(dev, E1000_TCTL, tctl);
    
    return 0;
}

int e1000_send_packet(network_interface_t *iface, void *data, size_t len) {
    (void)iface; // Unused - uses global e1000_dev
    
    if (!e1000_initialized || !data || len == 0 || len > E1000_BUFFER_SIZE) {
        DEBUG_WARN("E1000 TX: Invalid params (init=%d, data=%p, len=%zu)\n", 
                   e1000_initialized, data, len);
        return -1;
    }
    
    e1000_device_t *dev = &e1000_dev;
    
    // Check if current descriptor is available
    if (!(dev->tx_desc[dev->tx_cur].status & E1000_TXD_STAT_DD)) {
        DEBUG_WARN("E1000 TX: Ring full at cur=%d\n", dev->tx_cur);
        return -1; // TX ring full
    }
    
    // Copy data to TX buffer
    memcpy(dev->tx_buffers[dev->tx_cur], data, len);
    
    // Set up descriptor
    dev->tx_desc[dev->tx_cur].length = len;
    dev->tx_desc[dev->tx_cur].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    dev->tx_desc[dev->tx_cur].status = 0; // Clear status
    
    // Update tail pointer to start transmission
    uint16_t old_cur = dev->tx_cur;
    dev->tx_cur = (dev->tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write_reg(dev, E1000_TDT, dev->tx_cur);
    
    DEBUG_INFO("E1000 TX: Sent packet len=%zu cur=%d->%d\n", len, old_cur, dev->tx_cur);
    
    return len;
}

int e1000_receive_packet(network_interface_t *iface, void *buffer, size_t max_len) {
    (void)iface; // Unused - uses global e1000_dev
    
    if (!e1000_initialized || !buffer || max_len == 0) {
        return 0;
    }
    
    e1000_device_t *dev = &e1000_dev;
    
    // Check if there's a packet available
    if (!(dev->rx_desc[dev->rx_cur].status & E1000_RXD_STAT_DD)) {
        return 0; // No packet available
    }
    
    // We have a packet!
    uint16_t len = dev->rx_desc[dev->rx_cur].length;
    DEBUG_INFO("E1000: Received packet! len=%d cur=%d\n", len, dev->rx_cur);
    
    if (len > max_len) {
        len = max_len;
    }
    
    // Copy packet data from the virtual address
    memcpy(buffer, dev->rx_buffers[dev->rx_cur], len);
    
    // Reset descriptor
    dev->rx_desc[dev->rx_cur].status = 0;
    
    // Update tail pointer
    e1000_write_reg(dev, E1000_RDT, dev->rx_cur);
    dev->rx_cur = (dev->rx_cur + 1) % E1000_NUM_RX_DESC;
    
    return len;
}

void e1000_enable_interrupts(e1000_device_t *dev) {
    // Enable interrupts
    e1000_write_reg(dev, E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_TXDW | E1000_ICR_LSC);
}

void e1000_disable_interrupts(e1000_device_t *dev) {
    // Disable all interrupts
    e1000_write_reg(dev, E1000_IMC, 0xFFFFFFFF);
}

int e1000_probe(pci_device_t *pci_dev) {
    if (!pci_dev) {
        return -1;
    }
    
    // Verify this is an E1000 device
    if (pci_dev->vendor_id != E1000_VENDOR_ID) {
        return -1;
    }
    
    // Check for supported device IDs
    if (pci_dev->device_id != E1000_DEVICE_ID_82540EM &&
        pci_dev->device_id != E1000_DEVICE_ID_82545EM &&
        pci_dev->device_id != E1000_DEVICE_ID_82574L) {
        return -1;
    }
    
    e1000_dev.pci_dev = pci_dev;
    
    // Enable PCI bus mastering and memory access
    uint16_t command = pci_config_read16(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_config_write16(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_COMMAND, command);
    
    // Get MMIO base address from BAR0
    uint32_t bar0 = pci_dev->bar[0];
    if (!(bar0 & 0x1)) { // Memory-mapped I/O
        uintptr_t physical_base = (uintptr_t)(bar0 & 0xFFFFFFF0);
        
        // Validate MMIO address
        if (physical_base == 0) {
            return -1;
        }
        
        // Test if MMIO is accessible by trying to map it
        void *mapped = map_physical_memory(physical_base, 0x20000);
        if (!mapped) {
            // MMIO mapping failed - no virtual memory management available
            DEBUG_ERROR("Failed to map MMIO region\n");
            e1000_dev.mmio_base = 0;
            return -1;
        }
        
        // CRITICAL: Store the VIRTUAL address, not physical!
        e1000_dev.mmio_base = (uintptr_t)mapped;
        DEBUG_INFO("E1000 MMIO base (virtual): 0x%lx\n", e1000_dev.mmio_base);
        
        // Try to read the status register to verify MMIO access
        uint32_t status = e1000_read_reg(&e1000_dev, E1000_STATUS);
        DEBUG_INFO("E1000 STATUS register: 0x%08x\n", status);
        if (status == 0xFFFFFFFF) {
            // MMIO access failed
            DEBUG_ERROR("E1000 STATUS read failed (0xFFFFFFFF)\n");
            e1000_dev.mmio_base = 0;
            return -1;
        }
    } else {
        DEBUG_ERROR("E1000 BAR0 is I/O mapped, not supported\n");
        return -1; // We don't support I/O port mapped devices
    }
    
    // Reset the device
    e1000_reset(&e1000_dev);
    
    DEBUG_INFO("E1000: Reading MAC address...\n");
    
    // Read MAC address
    e1000_read_mac_address(&e1000_dev);
    
    DEBUG_INFO("E1000: Initializing RX ring...\n");
    
    // Initialize RX and TX rings
    if (e1000_rx_init(&e1000_dev) != 0) {
        DEBUG_ERROR("E1000: RX init failed\n");
        return -1;
    }
    
    DEBUG_INFO("E1000: Initializing TX ring...\n");
    
    if (e1000_tx_init(&e1000_dev) != 0) {
        DEBUG_ERROR("E1000: TX init failed\n");
        return -1;
    }
    
    DEBUG_INFO("E1000: Setting link up...\n");
    
    // Set link up
    uint32_t ctrl = e1000_read_reg(&e1000_dev, E1000_CTRL);
    ctrl |= E1000_CTRL_SLU;
    e1000_write_reg(&e1000_dev, E1000_CTRL, ctrl);
    
    e1000_initialized = true;
    
    DEBUG_INFO("E1000 MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
               e1000_dev.mac_address[0], e1000_dev.mac_address[1],
               e1000_dev.mac_address[2], e1000_dev.mac_address[3],
               e1000_dev.mac_address[4], e1000_dev.mac_address[5]);
    DEBUG_INFO("E1000 driver initialized successfully\n");
    
    return 0;
}

int e1000_init(void) {
    DEBUG_INFO("Scanning for E1000 NIC...\n");
    // Scan for E1000 devices
    pci_device_t *pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID_82540EM);
    if (!pci_dev) {
        pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID_82545EM);
    }
    if (!pci_dev) {
        pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID_82574L);
    }
    
    if (!pci_dev) {
        DEBUG_WARN("No E1000 NIC found\n");
        return -1; // No E1000 device found
    }
    
    DEBUG_INFO("Found E1000 NIC at %d:%d.%d\n", 
               pci_dev->bus, pci_dev->device, pci_dev->function);
    
    if (e1000_probe(pci_dev) != 0) {
        DEBUG_ERROR("E1000 probe failed\n");
        return -1;
    }
    
    return 0;
}

// Network interface operations for E1000
static netdev_ops_t e1000_netdev_ops = {
    .send = e1000_send_packet,
    .receive = e1000_receive_packet,
    .start = NULL,
    .stop = NULL,
    .init = NULL,
    .set_mac = NULL,
    .get_mac = NULL
};

int e1000_register_netdev(void) {
    if (!e1000_initialized) {
        return -1;
    }
    
    return netdev_register("eth0", &e1000_netdev_ops, e1000_dev.mac_address,
                          0x00000000, // IP will be set by DHCP
                          0x00000000, // Netmask will be set by DHCP
                          0x00000000  // Gateway will be set by DHCP
                          );
}
