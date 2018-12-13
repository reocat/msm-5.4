#ifndef __SMCCC_IO_H__
#define __SMCCC_IO_H__

#define readb_relaxed(c)	({ u8  __r = __smccc_readb(c); __r; })
#define readw_relaxed(c)	({ u16 __r = le16_to_cpu((__force __le16)__smccc_readw(c)); __r; })
#define readl_relaxed(c)	({ u32 __r = le32_to_cpu((__force __le32)__smccc_readl(c)); __r; })
#define readq_relaxed(c)	({ u64 __r = le64_to_cpu((__force __le64)__smccc_readq(c)); __r; })

#define writeb_relaxed(v,c)	((void)__smccc_writeb((v),(c)))
#define writew_relaxed(v,c)	((void)__smccc_writew((__force u16)cpu_to_le16(v),(c)))
#define writel_relaxed(v,c)	((void)__smccc_writel((__force u32)cpu_to_le32(v),(c)))
#define writeq_relaxed(v,c)	((void)__smccc_writeq((__force u64)cpu_to_le64(v),(c)))
#define ioremap(addr, size)	__smccc_ioremap((addr), (size))

#define SMCCC_WRITE(a,v,w)	{ if (!__smccc_write((a), (v), (w))) return;}
#define SMCCC_READ(a,w)		{ u64 __val; if (!__smccc_read((a), &__val, (w))) return __val;}

extern int __smccc_write(volatile void __iomem *addr, u64 val, u8 width);
extern int __smccc_read(const volatile void __iomem *addr, u64 *val, u8 width);
extern void __smccc_virt_hacker(phys_addr_t phys_addr,
		void __iomem *virt_addr, size_t length);

extern void __iomem *__ioremap(phys_addr_t phys_addr, size_t size, pgprot_t prot);

static inline u8 __smccc_readb(const volatile void __iomem *addr)
{
	SMCCC_READ(addr, 8);
	return __raw_readb(addr);
}

static inline u16 __smccc_readw(const volatile void __iomem *addr)
{
	SMCCC_READ(addr, 16);
	return __raw_readw(addr);
}

static inline u32 __smccc_readl(const volatile void __iomem *addr)
{
	SMCCC_READ(addr, 32);
	return __raw_readl(addr);
}

static inline u64 __smccc_readq(const volatile void __iomem *addr)
{
	SMCCC_READ(addr, 64);
	return __raw_readq(addr);
}

static inline void __smccc_writeb(u8 val, volatile void __iomem *addr)
{
	SMCCC_WRITE(addr, val, 8);
	__raw_writeb(val, addr);
}

static inline void __smccc_writew(u16 val, volatile void __iomem *addr)
{
	SMCCC_WRITE(addr, val, 16);
	__raw_writew(val, addr);
}

static inline void __smccc_writel(u32 val, volatile void __iomem *addr)
{
	SMCCC_WRITE(addr, val, 32);
	__raw_writel(val, addr);
}

static inline void __smccc_writeq(u64 val, volatile void __iomem *addr)
{
	SMCCC_WRITE(addr, val, 64);
	__raw_writeq(val, addr);
}
static inline void __iomem *__smccc_ioremap(phys_addr_t phys_addr, size_t size)
{
	void __iomem *virt;

	virt = __ioremap(phys_addr, size, __pgprot(PROT_DEVICE_nGnRE));
	__smccc_virt_hacker(phys_addr, virt, size);

	return virt;
}
#endif
