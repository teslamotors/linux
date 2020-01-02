/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_HVM_HVM_OP_H__
#define __XEN_PUBLIC_HVM_HVM_OP_H__

#include "../event_channel.h"

/* Get/set subcommands: the second argument of the hypercall is a
 * pointer to a xen_hvm_param struct. */
#define HVMOP_set_param           0
#define HVMOP_get_param           1
struct xen_hvm_param {
    domid_t  domid;    /* IN */
    uint32_t index;    /* IN */
    uint64_t value;    /* IN/OUT */
};
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_param);

/* Hint from PV drivers for pagetable destruction. */
#define HVMOP_pagetable_dying       9
struct xen_hvm_pagetable_dying {
    /* Domain with a pagetable about to be destroyed. */
    domid_t  domid;
    /* guest physical address of the toplevel pagetable dying */
    aligned_u64 gpa;
};
typedef struct xen_hvm_pagetable_dying xen_hvm_pagetable_dying_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_pagetable_dying_t);

/* MSI injection for emulated devices */
#define HVMOP_inject_msi         16
struct xen_hvm_inject_msi {
    /* Domain to be injected */
    domid_t   domid;
    /* Data -- lower 32 bits */
    uint32_t  data;
    /* Address (0xfeexxxxx) */
    uint64_t  addr;
};
typedef struct xen_hvm_inject_msi xen_hvm_inject_msi_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_inject_msi_t);

enum hvmmem_type_t {
    HVMMEM_ram_rw,             /* Normal read/write guest RAM */
    HVMMEM_ram_ro,             /* Read-only; writes are discarded */
    HVMMEM_mmio_dm,            /* Reads and write go to the device model */
    HVMMEM_mmio_write_dm       /* Read-only; writes go to the device model */
};

#define HVMOP_set_mem_type    8
/* Notify that a region of memory is to be treated in a specific way. */
struct xen_hvm_set_mem_type {
	/* Domain to be updated. */
	domid_t domid;
	/* Memory type */
	uint16_t hvmmem_type;
	/* Number of pages. */
	uint32_t nr;
	/* First pfn. */
	uint64_t first_pfn;
};
typedef struct xen_hvm_set_mem_type xen_hvm_set_mem_type_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_set_mem_type_t);


#define HVMOP_get_mem_type    15
/* Return hvmmem_type_t for the specified pfn. */
struct xen_hvm_get_mem_type {
    /* Domain to be queried. */
    domid_t domid;
    /* OUT variable. */
    uint16_t mem_type;
    uint16_t pad[2]; /* align next field on 8-byte boundary */
    /* IN variable. */
    uint64_t pfn;
};
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_get_mem_type);

#define HVMOP_vgt_wp_pages         27  /* writeprotection to guest pages */
#define MAX_WP_BATCH_PAGES         128
struct xen_hvm_vgt_wp_pages {
	uint16_t domid;
	uint16_t set;            /* 1: set WP, 0: remove WP */
	uint16_t nr_pages;
	unsigned long  wp_pages[MAX_WP_BATCH_PAGES];
};
typedef struct xen_hvm_vgt_wp_pages xen_hvm_vgt_wp_pages_t;

/*
 * IOREQ Servers
 *
 * The interface between an I/O emulator an Xen is called an IOREQ Server.
 * A domain supports a single 'legacy' IOREQ Server which is instantiated if
 * parameter...
 *
 * HVM_PARAM_IOREQ_PFN is read (to get the gmfn containing the synchronous
 * ioreq structures), or...
 * HVM_PARAM_BUFIOREQ_PFN is read (to get the gmfn containing the buffered
 * ioreq ring), or...
 * HVM_PARAM_BUFIOREQ_EVTCHN is read (to get the event channel that Xen uses
 * to request buffered I/O emulation).
 *
 * The following hypercalls facilitate the creation of IOREQ Servers for
 * 'secondary' emulators which are invoked to implement port I/O, memory, or
 * PCI config space ranges which they explicitly register.
 */
typedef uint16_t ioservid_t;

/*
 * HVMOP_create_ioreq_server: Instantiate a new IOREQ Server for a secondary
 *                            emulator servicing domain <domid>.
 *
 * The <id> handed back is unique for <domid>. If <handle_bufioreq> is zero
 * the buffered ioreq ring will not be allocated and hence all emulation
 * requestes to this server will be synchronous.
 */
#define HVMOP_create_ioreq_server 17
struct xen_hvm_create_ioreq_server {
    domid_t domid;           /* IN - domain to be serviced */
    uint8_t handle_bufioreq; /* IN - should server handle buffered ioreqs */
    ioservid_t id;           /* OUT - server id */
};
typedef struct xen_hvm_create_ioreq_server xen_hvm_create_ioreq_server_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_create_ioreq_server_t);

/*
 * HVMOP_get_ioreq_server_info: Get all the information necessary to access
 *                              IOREQ Server <id>.
 *
 * The emulator needs to map the synchronous ioreq structures and buffered
 * ioreq ring (if it exists) that Xen uses to request emulation. These are
 * hosted in domain <domid>'s gmfns <ioreq_pfn> and <bufioreq_pfn>
 * respectively. In addition, if the IOREQ Server is handling buffered
 * emulation requests, the emulator needs to bind to event channel
 * <bufioreq_port> to listen for them. (The event channels used for
 * synchronous emulation requests are specified in the per-CPU ioreq
 * structures in <ioreq_pfn>).
 * If the IOREQ Server is not handling buffered emulation requests then the
 * values handed back in <bufioreq_pfn> and <bufioreq_port> will both be 0.
 */
#define HVMOP_get_ioreq_server_info 18
struct xen_hvm_get_ioreq_server_info {
    domid_t domid;                 /* IN - domain to be serviced */
    ioservid_t id;                 /* IN - server id */
    evtchn_port_t bufioreq_port;   /* OUT - buffered ioreq port */
    uint64_t ioreq_pfn;    /* OUT - sync ioreq pfn */
    uint64_t bufioreq_pfn; /* OUT - buffered ioreq pfn */
};
typedef struct xen_hvm_get_ioreq_server_info xen_hvm_get_ioreq_server_info_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_get_ioreq_server_info_t);

/*
 * HVM_map_io_range_to_ioreq_server: Register an I/O range of domain <domid>
 *                                   for emulation by the client of IOREQ
 *                                   Server <id>
 * HVM_unmap_io_range_from_ioreq_server: Deregister an I/O range of <domid>
 *                                       for emulation by the client of IOREQ
 *                                       Server <id>
 *
 * There are three types of I/O that can be emulated: port I/O, memory accesses
 * and PCI config space accesses. The <type> field denotes which type of range
 * the <start> and <end> (inclusive) fields are specifying.
 * PCI config space ranges are specified by segment/bus/device/function values
 * which should be encoded using the HVMOP_PCI_SBDF helper macro below.
 *
 * NOTE: unless an emulation request falls entirely within a range mapped
 * by a secondary emulator, it will not be passed to that emulator.
 */
#define HVMOP_map_io_range_to_ioreq_server 19
#define HVMOP_unmap_io_range_from_ioreq_server 20
struct xen_hvm_io_range {
    domid_t domid;               /* IN - domain to be serviced */
    ioservid_t id;               /* IN - server id */
    uint32_t type;               /* IN - type of range */
# define HVMOP_IO_RANGE_PORT   0 /* I/O port range */
# define HVMOP_IO_RANGE_MEMORY 1 /* MMIO range */
# define HVMOP_IO_RANGE_PCI    2 /* PCI segment/bus/dev/func range */
    uint64_t start, end; /* IN - inclusive start and end of range */
};
typedef struct xen_hvm_io_range xen_hvm_io_range_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_io_range_t);

#define HVMOP_PCI_SBDF(s,b,d,f)                 \
       ((((s) & 0xffff) << 16) |                   \
        (((b) & 0xff) << 8) |                      \
        (((d) & 0x1f) << 3) |                      \
        ((f) & 0x07))

/*
 * HVMOP_destroy_ioreq_server: Destroy the IOREQ Server <id> servicing domain
 *                             <domid>.
 *
 * Any registered I/O ranges will be automatically deregistered.
 */
#define HVMOP_destroy_ioreq_server 21
struct xen_hvm_destroy_ioreq_server {
    domid_t domid; /* IN - domain to be serviced */
    ioservid_t id; /* IN - server id */
};
typedef struct xen_hvm_destroy_ioreq_server xen_hvm_destroy_ioreq_server_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_destroy_ioreq_server_t);


/*
 * HVMOP_set_ioreq_server_state: Enable or disable the IOREQ Server <id> servicing
 *                               domain <domid>.
 *
 * The IOREQ Server will not be passed any emulation requests until it is in the
 * enabled state.
 * Note that the contents of the ioreq_pfn and bufioreq_fn (see
 * HVMOP_get_ioreq_server_info) are not meaningful until the IOREQ Server is in
 * the enabled state.
 */
#define HVMOP_set_ioreq_server_state 22
struct xen_hvm_set_ioreq_server_state {
    domid_t domid;   /* IN - domain to be serviced */
    ioservid_t id;   /* IN - server id */
    uint8_t enabled; /* IN - enabled? */
};
typedef struct xen_hvm_set_ioreq_server_state xen_hvm_set_ioreq_server_state_t;
DEFINE_GUEST_HANDLE_STRUCT(xen_hvm_set_ioreq_server_state_t);


#endif /* __XEN_PUBLIC_HVM_HVM_OP_H__ */
