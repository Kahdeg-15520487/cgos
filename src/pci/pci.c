#include "pci.h"
#include "../memory/memory.h"
#include "../graphic/graphic.h"
#include "../debug/debug.h"

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

void pci_init(void) {
    pci_device_count = 0;
    pci_scan_devices();
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return (inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return (inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF;
}

void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_config_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    data &= ~(0xFFFF << ((offset & 2) * 8));
    data |= value << ((offset & 2) * 8);
    outl(PCI_CONFIG_DATA, data);
}

void pci_config_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    data &= ~(0xFF << ((offset & 3) * 8));
    data |= value << ((offset & 3) * 8);
    outl(PCI_CONFIG_DATA, data);
}

int pci_scan_devices(void) {
    pci_device_count = 0;
    DEBUG_INFO("Starting PCI bus scan...\n");
    
    for (int bus = 0; bus < 256; bus++) {
        for (int device = 0; device < 32; device++) {
            for (int function = 0; function < 8; function++) {
                uint16_t vendor_id = pci_config_read16(bus, device, function, PCI_VENDOR_ID);
                
                if (vendor_id == 0xFFFF) {
                    if (function == 0) break; // No device here, skip to next device
                    continue; // No function here, try next function
                }
                
                if (pci_device_count >= MAX_PCI_DEVICES) {
                    DEBUG_WARN("Maximum PCI devices reached (%d)\n", MAX_PCI_DEVICES);
                    return pci_device_count;
                }
                
                pci_device_t *dev = &pci_devices[pci_device_count];
                dev->bus = bus;
                dev->device = device;
                dev->function = function;
                dev->vendor_id = vendor_id;
                dev->device_id = pci_config_read16(bus, device, function, PCI_DEVICE_ID);
                dev->class_code = pci_config_read8(bus, device, function, PCI_CLASS_CODE);
                dev->subclass = pci_config_read8(bus, device, function, PCI_SUBCLASS);
                dev->prog_if = pci_config_read8(bus, device, function, PCI_PROG_IF);
                dev->revision_id = pci_config_read8(bus, device, function, PCI_REVISION_ID);
                dev->interrupt_line = pci_config_read8(bus, device, function, PCI_INTERRUPT_LINE);
                dev->interrupt_pin = pci_config_read8(bus, device, function, PCI_INTERRUPT_PIN);
                
                // Read BARs
                for (int i = 0; i < 6; i++) {
                    dev->bar[i] = pci_config_read32(bus, device, function, PCI_BAR0 + i * 4);
                }
                
                DEBUG_DEBUG("Found PCI device: %02x:%02x.%x - Vendor: %04x, Device: %04x, Class: %02x\n", 
                           bus, device, function, vendor_id, dev->device_id, dev->class_code);
                
                pci_device_count++;
                
                // If this is not a multi-function device, skip other functions
                if (function == 0) {
                    uint8_t header_type = pci_config_read8(bus, device, function, PCI_HEADER_TYPE);
                    if ((header_type & 0x80) == 0) {
                        break;
                    }
                }
            }
        }
    }
    
    DEBUG_INFO("PCI bus scan completed. Found %d devices\n", pci_device_count);
    return pci_device_count;
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}

int pci_get_device_count(void) {
    return pci_device_count;
}

void pci_print_devices(int start_x, int start_y) {
    kprintf(start_x, start_y, "PCI Devices Found: %d", pci_device_count);
    
    int y = start_y + 15;
    for (int i = 0; i < pci_device_count && i < 20; i++) { // Limit to 20 devices to avoid screen overflow
        pci_device_t *dev = &pci_devices[i];
        
        // Print basic device info
        kprintf(start_x, y, "Device %d: %x:%x.%x - Vendor: %x Device: %x", 
               i, dev->bus, dev->device, dev->function, dev->vendor_id, dev->device_id);
        y += 15;
        
        // Print class information
        const char *class_name = "Unknown";
        if (dev->class_code == 0x00) class_name = "Legacy";
        else if (dev->class_code == 0x01) class_name = "Storage";
        else if (dev->class_code == 0x02) class_name = "Network";
        else if (dev->class_code == 0x03) class_name = "Display";
        else if (dev->class_code == 0x04) class_name = "Multimedia";
        else if (dev->class_code == 0x05) class_name = "Memory";
        else if (dev->class_code == 0x06) class_name = "Bridge";
        else if (dev->class_code == 0x0C) class_name = "Serial Bus";
        
        kprintf(start_x + 20, y, "Class: %x (%s) Subclass: %x", 
               dev->class_code, class_name, dev->subclass);
        y += 15;
        
        // Print vendor specific info for known devices
        if (dev->vendor_id == 0x8086) { // Intel
            if (dev->device_id == 0x100E) {
                kprintf(start_x + 20, y, "Intel 82540EM Gigabit Ethernet");
            } else if (dev->device_id == 0x100F) {
                kprintf(start_x + 20, y, "Intel 82545EM Gigabit Ethernet");
            } else {
                kprintf(start_x + 20, y, "Intel Device");
            }
        } else if (dev->vendor_id == 0x1234) { // QEMU
            kprintf(start_x + 20, y, "QEMU Device");
        } else {
            kprintf(start_x + 20, y, "Unknown Vendor");
        }
        y += 20; // Extra space between devices
    }
    
    if (pci_device_count > 20) {
        kprintf(start_x, y, "... and %d more devices", pci_device_count - 20);
    }
}
