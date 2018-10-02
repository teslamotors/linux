#ifndef __ACRN_VHM_EVENTFD_H__
#define __ACRN_VHM_EVENTFD_H__

/* ioeventfd APIs */
struct acrn_ioeventfd;
int acrn_ioeventfd_init(int vmid);
int acrn_ioeventfd(int vmid, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(int vmid);

/* irqfd APIs */
struct acrn_irqfd;
int acrn_irqfd_init(uint16_t vmid);
int acrn_irqfd(uint16_t vmid, struct acrn_irqfd *args);
void acrn_irqfd_deinit(uint16_t vmid);

#endif
