#ifndef __HYPER_DMABUF_QUERY_H__
#define __HYPER_DMABUF_QUERY_H__

int hyper_dmabuf_query_imported(struct imported_sgt_info *imported,
				int query, unsigned long *info);

int hyper_dmabuf_query_exported(struct exported_sgt_info *exported,
				int query, unsigned long *info);

#endif // __HYPER_DMABUF_QUERY_H__
