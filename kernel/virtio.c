#include "virtio.h"
#include "kernel.h"

uint32_t virtio_reg_read32(unsigned int offset) {
    return *((volatile uint32_t*) (VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned int offset) {
    return *((volatile uint64_t*) (VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned int offset, uint32_t value) {
    *((volatile uint32_t*) (VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq *virtq_init(unsigned index) {
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}

void virtq_kick(struct virtio_virtq* vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

bool virtq_is_busy(struct virtio_virtq* vq) {
    return vq->last_used_index != *vq->used_index;
}

static struct virtio_virtq* gpu_request_vq;
static union virtio_gpu_req* gpu_req;
static paddr_t gpu_req_paddr;

void virtio_init(void) {
    printf(
        "magic=%x, version=%d, device=%d\n",
        virtio_reg_read32(VIRTIO_REG_MAGIC),
        virtio_reg_read32(VIRTIO_REG_VERSION),
        virtio_reg_read32(VIRTIO_REG_DEVICE_ID)
    );

    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976) {
        PANIC("virtio: invalid magic value");
    }
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1) {
        PANIC("virtio: invalid version");
    }
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_GPU) {
        PANIC("virtio: invalid device id");
    }

    gpu_request_vq = virtq_init(0);

    gpu_req_paddr = alloc_pages(align_up(sizeof(*gpu_req), PAGE_SIZE) / PAGE_SIZE);
    gpu_req = (union virtio_gpu_req*) gpu_req_paddr;

    gpu_req->cmd_display_info.req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    gpu_req->cmd_display_info.req.flags = 0;
    gpu_req->cmd_display_info.req.fence_id = 0;
    gpu_req->cmd_display_info.req.ctx_id = 0;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_display_info.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_display_info.resp);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_resp_display_info);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[1].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_display_info.resp.hdr.type !=
            VIRTIO_GPU_RESP_OK_DISPLAY_INFO
    ) {
        PANIC("unable to get display info\n");
    }

    int disp_idx;
    for (disp_idx = 0; disp_idx < VIRTIO_GPU_MAX_SCANOUTS; disp_idx++) {
        if (gpu_req->cmd_display_info.resp.pmodes[disp_idx].enabled) {
            break;
        }
    }
    if (!gpu_req->cmd_display_info.resp.pmodes[disp_idx].enabled) {
        PANIC("no display found\n");
    }

    printf(
        "disp_idx=%d, enabled=%d, x=%d, y=%d, w=%d, h=%d\n",
        disp_idx,
        gpu_req->cmd_display_info.resp.pmodes[disp_idx].enabled,
        gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.x,
        gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.y,
        gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.width,
        gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.height
    );

    int w = gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.width;
    int h = gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.height;

    gpu_req->cmd_create_2d.req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    gpu_req->cmd_create_2d.req.hdr.flags = 0;
    gpu_req->cmd_create_2d.req.hdr.fence_id = 0;
    gpu_req->cmd_create_2d.req.hdr.ctx_id = 0;

    gpu_req->cmd_create_2d.req.format = 1;
    gpu_req->cmd_create_2d.req.resource_id = 42;
    gpu_req->cmd_create_2d.req.width = gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.width;
    gpu_req->cmd_create_2d.req.height = gpu_req->cmd_display_info.resp.pmodes[disp_idx].r.height;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_create_2d.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_resource_create_2d);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_create_2d.resp);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[1].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_create_2d.resp.type !=
            VIRTIO_GPU_RESP_OK_NODATA
    ) {
        PANIC("unable to create 2d resource\n");
    }

    int fbsize = 4 * w * h;
    paddr_t fb = alloc_pages(fbsize / PAGE_SIZE);

    gpu_req->cmd_attach.req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    gpu_req->cmd_attach.req.hdr.flags = 0;
    gpu_req->cmd_attach.req.hdr.fence_id = 0;
    gpu_req->cmd_attach.req.hdr.ctx_id = 0;

    gpu_req->cmd_attach.req.nr_entries = 1;
    gpu_req->cmd_attach.req.resource_id = 42;

    gpu_req->cmd_attach.entry.addr = fb;
    gpu_req->cmd_attach.entry.length = fbsize;
    gpu_req->cmd_attach.entry.padding = 0;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_attach.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_resource_attach_backing);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_attach.entry);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_mem_entry);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[1].next = 2;

    gpu_request_vq->descs[2].addr = (uint64_t) &(gpu_req->cmd_attach.resp);
    gpu_request_vq->descs[2].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[2].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[2].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_attach.resp.type !=
            VIRTIO_GPU_RESP_OK_NODATA
    ) {
        PANIC("unable to attach fb\n");
    }

    gpu_req->cmd_scanout.req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    gpu_req->cmd_scanout.req.hdr.flags = 0;
    gpu_req->cmd_scanout.req.hdr.fence_id = 0;
    gpu_req->cmd_scanout.req.hdr.ctx_id = 0;

    gpu_req->cmd_scanout.req.r.x = 0;
    gpu_req->cmd_scanout.req.r.y = 0;
    gpu_req->cmd_scanout.req.r.width = w;
    gpu_req->cmd_scanout.req.r.height = h;
    gpu_req->cmd_scanout.req.scanout_id = disp_idx;
    gpu_req->cmd_scanout.req.resource_id = 42;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_scanout.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_set_scanout);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_scanout.resp);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[1].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_scanout.resp.type !=
            VIRTIO_GPU_RESP_OK_NODATA
    ) {
        PANIC("unable to set scanout\n");
    }

    const uint32_t colors[] = {
        0x808080, /* grey */
        0xFFFF00, /* yellow */
        0x00FFFF, /* cyan */
        0x00FF00, /* green */
        0xFF00FF, /* magenta */
        0xFF0000, /* red */
        0x0000FF, /* blue */
    };
    int num_colors = sizeof(colors) / sizeof(colors[0]);
    int strip_width = w / num_colors;
    for (int i = 0; i < num_colors; i++) {
        for (int x = i * strip_width; x < (i + 1) * strip_width; x++) {
            for (int y = 0; y < h; y++) {
                ((uint32_t*) fb)[y*w+x] = colors[i];
            }
        }
    }

    gpu_req->cmd_transfer.req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    gpu_req->cmd_transfer.req.hdr.flags = 0;
    gpu_req->cmd_transfer.req.hdr.fence_id = 0;
    gpu_req->cmd_transfer.req.hdr.ctx_id = 0;

    gpu_req->cmd_transfer.req.r.x = 0;
    gpu_req->cmd_transfer.req.r.y = 0;
    gpu_req->cmd_transfer.req.r.width = w;
    gpu_req->cmd_transfer.req.r.height = h;
    gpu_req->cmd_transfer.req.padding = 0;
    gpu_req->cmd_transfer.req.resource_id = 42;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_transfer.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_transfer_to_host_2d);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_transfer.resp);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[1].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_transfer.resp.type !=
            VIRTIO_GPU_RESP_OK_NODATA
    ) {
        PANIC("unable to transfer fb\n");
    }

    gpu_req->cmd_flush.req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    gpu_req->cmd_flush.req.hdr.flags = 0;
    gpu_req->cmd_flush.req.hdr.fence_id = 0;
    gpu_req->cmd_flush.req.hdr.ctx_id = 0;

    gpu_req->cmd_flush.req.r.x = 0;
    gpu_req->cmd_flush.req.r.y = 0;
    gpu_req->cmd_flush.req.r.width = w;
    gpu_req->cmd_flush.req.r.height = h;
    gpu_req->cmd_flush.req.padding = 0;
    gpu_req->cmd_flush.req.resource_id = 42;

    gpu_request_vq->descs[0].addr = (uint64_t) &(gpu_req->cmd_flush.req);
    gpu_request_vq->descs[0].len = sizeof(struct virtio_gpu_resource_flush);
    gpu_request_vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    gpu_request_vq->descs[0].next = 1;

    gpu_request_vq->descs[1].addr = (uint64_t) &(gpu_req->cmd_flush.resp);
    gpu_request_vq->descs[1].len = sizeof(struct virtio_gpu_ctrl_hdr);
    gpu_request_vq->descs[1].flags = VIRTQ_DESC_F_WRITE;
    gpu_request_vq->descs[1].next = 0;

    virtq_kick(gpu_request_vq, 0);
    while (virtq_is_busy(gpu_request_vq));

    if (
        gpu_req->cmd_flush.resp.type !=
            VIRTIO_GPU_RESP_OK_NODATA
    ) {
        PANIC("unable to flush fb\n");
    }
}