#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};


MODULE_INFO(depends, "v4l2-async,videodev,mc");

MODULE_ALIAS("i2c:gc2607");
MODULE_ALIAS("acpi*:GCTI2607:*");

MODULE_INFO(srcversion, "12881E458A219FB6376AAFF");
