#ifndef __AMBARELLA_SMC__
#define __AMBARELLA_SMC__

/* cpufreq svc ID 0x1 */
#define AMBA_SCM_SVC_FREQ			0x1
#define AMBA_SCM_CNTFRQ_SETUP_CMD		0x1

#define AMBA_SCM_SVC_HIBERNATE			0x2
#define AMBA_SCM_HIBERNATE_GPIO_SETUP		0x1

/* XXX svc ID 0xff */
#define AMBA_SCM_SVC_QUERY			0xff
#define AMBA_SCM_QUERY_COUNT			0x00
#define AMBA_SCM_QUERY_UID			0x01
#define AMBA_SCM_QUERY_VERSION			0x03

#define	SVC_SCM_FN(s, f)			((((s) & 0xff) << 8) | ((f) & 0xff))

#define SVC_OF_SMC(c)				(((c) >> 8) & 0xff)
#define FNID_OF_SMC(c)				((c) & 0xff)
#endif
