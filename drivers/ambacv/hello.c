
//ARCH=arm64 CROSS_COMPILE=~/ambalink/output/amba/host/usr/bin/aarch64-linux-gnu- make -C ~/ambalink/output/amba/build/linux-custom M=~/hello modules

#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");
static int hello_init(void)
{
        printk(KERN_INFO "Hello, world\n");
        return 0;
}
static void hello_exit(void)
{
        printk(KERN_INFO "Goodbye, cruel world\n");
}
module_init(hello_init);
module_exit(hello_exit);
