#pragma once

#include "common.h"

#define VIRTIO_BLK_PADDR 0x10001000

#define VIRTIO_REG_MAGIC     0x00
#define VIRTIO_REG_VERSION   0x04
#define VIRTIO_REG_DEVICE_ID 0x08
#define VIRTIO_REG_QUEUE_NOTIFY  0x50

#define VIRTIO_DEVICE_GPU 16

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO 0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101
#define VIRTIO_GPU_CMD_SET_SCANOUT 0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH 0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

#define VIRTIO_GPU_RESP_OK_NODATA 0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101

uint32_t virtio_reg_read32(unsigned int offset);
uint64_t virtio_reg_read64(unsigned int offset);
void virtio_reg_write32(unsigned int offset, uint32_t value);
void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value);

#define VIRTQ_ENTRY_NUM 16
#define VIRTIO_REG_QUEUE_SEL 0x30
#define VIRTIO_REG_QUEUE_NUM     0x38
#define VIRTIO_REG_QUEUE_ALIGN   0x3c
#define VIRTIO_REG_QUEUE_PFN     0x40

#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

struct virtio_virtq {
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
} __attribute__((packed));

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
};

struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

#define VIRTIO_GPU_MAX_SCANOUTS 16

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one {
        struct virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
};

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
};

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
};

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
};

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
};

union virtio_gpu_req
{
    struct {
        struct virtio_gpu_ctrl_hdr req;
        struct virtio_gpu_resp_display_info resp;
    } cmd_display_info;
    struct {
        struct virtio_gpu_resource_create_2d req;
        struct virtio_gpu_ctrl_hdr resp;
    } cmd_create_2d;
    struct {
        struct virtio_gpu_resource_attach_backing req;
        struct virtio_gpu_mem_entry entry;
        struct virtio_gpu_ctrl_hdr resp;
    } cmd_attach;
    struct {
        struct virtio_gpu_set_scanout req;
        struct virtio_gpu_ctrl_hdr resp;
    } cmd_scanout;
    struct {
        struct virtio_gpu_transfer_to_host_2d req;
        struct virtio_gpu_ctrl_hdr resp;
    } cmd_transfer;
    struct {
        struct virtio_gpu_resource_flush req;
        struct virtio_gpu_ctrl_hdr resp;
    } cmd_flush;
};

void virtio_init(void);