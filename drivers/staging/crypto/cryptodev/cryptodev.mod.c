#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x903a5ec4, "module_layout" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xff964b25, "param_set_int" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0x4661e311, "__tracepoint_kmalloc" },
	{ 0x893412fe, "kmem_cache_alloc" },
	{ 0xadee93f3, "kmalloc_caches" },
	{ 0x4e1e7401, "crypto_alloc_base" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x236c8c64, "memcpy" },
	{ 0xbe499d81, "copy_to_user" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0x37a0cba, "kfree" },
	{ 0x3f1899f1, "up" },
	{ 0x9bf377f5, "crypto_destroy_tfm" },
	{ 0x9ced38aa, "down_trylock" },
	{ 0x945bc6a7, "copy_from_user" },
	{ 0x748caf40, "down" },
	{ 0x6729d3df, "__get_user_4" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xb2fd5ceb, "__put_user_4" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xa62a61be, "fd_install" },
	{ 0x99bfbe39, "get_unused_fd" },
	{ 0xd8778690, "per_cpu__current_task" },
	{ 0x54b8041c, "misc_register" },
	{ 0xea147363, "printk" },
	{ 0x11cef3ea, "misc_deregister" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "A0688FBD1488A8A38E2DF83");
