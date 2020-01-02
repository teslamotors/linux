#ifndef _GVT_DEBUGFS_H_
#define _GVT_DEBUGFS_H_
struct intel_gvt;
struct intel_vgpu;

struct dentry *intel_gvt_init_debugfs(struct intel_gvt *gvt);
int intel_vgpu_create_debugfs(struct intel_vgpu *vgpu);
void intel_vgpu_destroy_debugfs(struct intel_vgpu *vgpu);
void intel_vgpu_cleanup_debug_statistics(struct intel_vgpu *vgpu);
#endif /* _GVT_DEBUGFS_H_ */
