/*
 * drivers/serial/ambarella_uart.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2009/01/04 - [Anthony Ginger] Port to 2.6.28
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#if defined(CONFIG_SERIAL_AMBARELLA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>

#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>

#include <mach/hardware.h>
#include <plat/uart.h>

static void serial_ambarella_enable_ms(struct uart_port *port)
{
	disable_irq(port->irq);
	amba_setbitsl(port->membase + UART_IE_OFFSET, UART_IE_EDSSI);
	enable_irq(port->irq);
}

static void serial_ambarella_disable_ms(struct uart_port *port)
{
	disable_irq(port->irq);
	amba_clrbitsl(port->membase + UART_IE_OFFSET, UART_IE_EDSSI);
	enable_irq(port->irq);
}

static void serial_ambarella_start_tx(struct uart_port *port)
{
	disable_irq(port->irq);
	amba_setbitsl(port->membase + UART_IE_OFFSET, UART_IE_ETBEI);
	enable_irq(port->irq);
}

static void serial_ambarella_stop_tx(struct uart_port *port)
{
	disable_irq(port->irq);
	amba_clrbitsl(port->membase + UART_IE_OFFSET, UART_IE_ETBEI);
	enable_irq(port->irq);
}

static void serial_ambarella_stop_rx(struct uart_port *port)
{
	disable_irq(port->irq);
	amba_clrbitsl(port->membase + UART_IE_OFFSET, UART_IE_ERBFI);
	enable_irq(port->irq);
}

static inline void receive_chars(struct uart_port *port, u32 *status)
{
	struct tty_struct			*tty = port->state->port.tty;
	u32					ch;
	u32					flag;
	int					max_count;

	max_count = port->fifosize;
	do {
		flag = TTY_NORMAL;
		if (unlikely(*status & (UART_LS_BI | UART_LS_PE |
					UART_LS_FE | UART_LS_OE))) {
			if (*status & UART_LS_BI) {
				*status &= ~(UART_LS_FE | UART_LS_PE);
				port->icount.brk++;

				if (uart_handle_break(port))
					goto ignore_char;
			}
			if (*status & UART_LS_FE)
				port->icount.frame++;
			if (*status & UART_LS_PE)
				port->icount.parity++;
			if (*status & UART_LS_OE)
				port->icount.overrun++;

			*status &= port->read_status_mask;

			if (*status & UART_LS_BI)
				flag = TTY_BREAK;
			else if (*status & UART_LS_FE)
				flag = TTY_FRAME;
			else if (*status & UART_LS_PE)
				flag = TTY_PARITY;
			else if ((*status & UART_LS_OE) &&
				!(*status & UART_LS_DR))
				flag = TTY_OVERRUN;

			if (*status & UART_LS_OE)
				dev_err(port->dev, "OVERFLOW\n");
		}

		if (likely(*status & UART_LS_DR)) {
			ch = amba_readl(port->membase + UART_RB_OFFSET);
			port->icount.rx++;

			if (uart_handle_sysrq_char(port, ch))
				goto ignore_char;

			uart_insert_char(port, *status, UART_LS_OE, ch, flag);
		} else if (unlikely(flag != TTY_NORMAL)) {
			tty_insert_flip_char(tty, 0, flag);
		}

	ignore_char:
		*status = amba_readl(port->membase + UART_LS_OFFSET);
	} while ((*status & UART_LS_DR) && (max_count-- > 0));

	tty_flip_buffer_push(tty);
}

static void transmit_chars(struct uart_port *port)
{
	struct circ_buf				*xmit = &port->state->xmit;
	int					count;

	if (port->x_char) {
		amba_writeb(port->membase + UART_TH_OFFSET, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	count = port->fifosize;
	while (count-- > 0) {
		if (uart_circ_empty(xmit)) {
			amba_clrbitsl(port->membase + UART_IE_OFFSET,
				UART_IE_ETBEI);
			break;
		}
		amba_writeb(port->membase + UART_TH_OFFSET,
			xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static inline void check_modem_status(struct uart_port *port)
{
	struct ambarella_uart_port_info		*port_info;
	u8					ms;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	if (port_info->flow_control) {
		ms = amba_readb(port->membase + UART_MS_OFFSET);

		if (ms & UART_MS_RI)
			port->icount.rng++;

		if (ms & UART_MS_DSR)
			port->icount.dsr++;

		if (ms & UART_MS_DCTS)
			uart_handle_cts_change(port, (ms & UART_MS_CTS));

		if (ms & UART_MS_DDCD)
			uart_handle_dcd_change(port, (ms & UART_MS_DCD));
	}

	wake_up_interruptible(&port->state->port.delta_msr_wait);
}

static irqreturn_t serial_ambarella_irq(int irq, void *dev_id)
{
	struct uart_port			*port = dev_id;
	int					rval = IRQ_HANDLED;
	u32					ii, ls;

	amba_setbitsl(port->membase + UART_IE_OFFSET, UART_IE_PTIME);
	ii = amba_readl(port->membase + UART_II_OFFSET);
	ls = amba_readl(port->membase + UART_LS_OFFSET);

	switch (ii & 0x0F) {
	case UART_II_MODEM_STATUS_CHANGED:
		check_modem_status(port);
		break;

	case UART_II_THR_EMPTY:
		transmit_chars(port);
		break;

	case UART_II_RCV_STATUS:
	case UART_II_RCV_DATA_AVAIL:
	case UART_II_CHAR_TIMEOUT:
		receive_chars(port, &ls);
		break;

	case UART_II_NO_INT_PENDING:
	default:
		rval = IRQ_NONE;
		break;
	}

	amba_clrbitsl(port->membase + UART_IE_OFFSET, UART_IE_PTIME);

	return rval;
}

static unsigned int serial_ambarella_tx_empty(struct uart_port *port)
{
	return amba_tstbitsl(port->membase + UART_LS_OFFSET, UART_LS_TEMT) ?
		TIOCSER_TEMT : 0;
}

static unsigned int serial_ambarella_get_mctrl(struct uart_port *port)
{
	unsigned int				mctrl = 0;
	struct ambarella_uart_port_info		*port_info;
	u8					ms;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	if (port_info->flow_control) {
		ms = amba_readb(port->membase + UART_MS_OFFSET);

		if (ms & UART_MS_CTS)
			mctrl |= TIOCM_CTS;

		if (ms & UART_MS_DSR)
			mctrl |= TIOCM_DSR;

		if (ms & UART_MS_RI)
			mctrl |= TIOCM_RI;

		if (ms & UART_MS_DCD)
			mctrl |= TIOCM_CD;
	}

	return mctrl;
}

static void serial_ambarella_set_mctrl(struct uart_port *port,
	unsigned int mctrl)
{
	struct ambarella_uart_port_info		*port_info;
	u8					mcr;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	if (port_info->flow_control) {
		mcr = amba_readb(port->membase + UART_MC_OFFSET);

		if (mctrl & TIOCM_DTR)
			mcr |= UART_MC_DTR;
		else
			mcr &= ~UART_MC_DTR;

		if (mctrl & TIOCM_RTS)
			mcr |= UART_MC_RTS;
		else
			mcr &= ~UART_MC_RTS;

		if (mctrl & TIOCM_OUT1)
			mcr |= UART_MC_OUT1;
		else
			mcr &= ~UART_MC_OUT1;

		if (mctrl & TIOCM_OUT2)
			mcr |= UART_MC_OUT2;
		else
			mcr &= ~UART_MC_OUT2;

		if (mctrl & TIOCM_LOOP)
			mcr |= UART_MC_LB;
		else
			mcr &= ~UART_MC_LB;

		mcr &= ~UART_MC_AFCE;
		mcr |= port_info->mcr;

		amba_writeb(port->membase + UART_MC_OFFSET, mcr);
	}
}

static void serial_ambarella_break_ctl(struct uart_port *port, int break_state)
{
	if (break_state != 0)
		amba_setbitsl(port->membase + UART_LC_OFFSET, UART_LC_BRK);
	else
		amba_clrbitsl(port->membase + UART_LC_OFFSET, UART_LC_BRK);
}

static int serial_ambarella_startup(struct uart_port *port)
{
	int					retval = 0;
	struct ambarella_uart_port_info		*port_info;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	amba_writel(port->membase + UART_FC_OFFSET,
		(UART_FC_FIFOE | UART_FC_RX_2_TO_FULL | UART_FC_TX_EMPTY |
		UART_FC_XMITR | UART_FC_RCVRR));

	retval = request_irq(port->irq, serial_ambarella_irq,
		IRQF_TRIGGER_HIGH, dev_name(port->dev), port);
	if (retval)
		goto serial_ambarella_startup_exit;

	amba_setbitsl(port->membase + UART_IE_OFFSET,
		(UART_IE_ELSI | UART_IE_ERBFI | UART_IE_ETOI));

serial_ambarella_startup_exit:
	return retval;
}

static void serial_ambarella_shutdown(struct uart_port *port)
{
	u8					lc;

	free_irq(port->irq, port);

	amba_writel(port->membase + UART_IE_OFFSET, 0x0);
	lc = amba_readb(port->membase + UART_LC_OFFSET);
	lc &= ~UART_LC_BRK;
	amba_writeb(port->membase + UART_LC_OFFSET, lc);
}

static void serial_ambarella_set_termios(struct uart_port *port,
	struct ktermios *termios, struct ktermios *old)
{
	struct ambarella_uart_port_info		*port_info;
	unsigned long				flags;
	unsigned int				baud, quot;
	u8					lc = 0x0;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lc |= UART_LC_CLS_5_BITS;
		break;
	case CS6:
		lc |= UART_LC_CLS_6_BITS;
		break;
	case CS7:
		lc |= UART_LC_CLS_7_BITS;
		break;
	case CS8:
	default:
		lc |= UART_LC_CLS_8_BITS;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		lc |= UART_LC_STOP_2BIT;
	else
		lc |= UART_LC_STOP_1BIT;

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			lc |= (UART_LC_EPS | UART_LC_ODD_PARITY);
		else
			lc |= (UART_LC_EPS | UART_LC_EVEN_PARITY);
	}

	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk / 16);
	quot = uart_get_divisor(port, baud);

	spin_lock_irqsave(&port->lock, flags);
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= UART_LSR_BI;

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UART_LSR_BI;

		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= UART_LSR_OE;
	}

	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_LSR_DR;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);
	port_info->mcr = (termios->c_cflag & CRTSCTS) ? UART_MC_AFCE : 0;

	amba_writeb(port->membase + UART_LC_OFFSET, UART_LC_DLAB);
	amba_writeb(port->membase + UART_DLL_OFFSET, quot & 0xff);
	amba_writeb(port->membase + UART_DLH_OFFSET, (quot >> 8) & 0xff);
	amba_writeb(port->membase + UART_LC_OFFSET, lc);

	if (UART_ENABLE_MS(port, termios->c_cflag))
		serial_ambarella_enable_ms(port);
	else
		serial_ambarella_disable_ms(port);

	serial_ambarella_set_mctrl(port, port->mctrl);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void serial_ambarella_pm(struct uart_port *port,
	unsigned int state, unsigned int oldstate)
{
	/* TODO: Use RCT to turn off or scale back UART PLL */
}

static void serial_ambarella_release_port(struct uart_port *port)
{
}

static int serial_ambarella_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_ambarella_config_port(struct uart_port *port, int flags)
{
}

static int serial_ambarella_verify_port(struct uart_port *port,
					struct serial_struct *ser)
{
	int					retval = 0;

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_UART00)
		retval = -EINVAL;
	if (port->irq != ser->irq)
		retval = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		retval = -EINVAL;

	return retval;
}

static const char *serial_ambarella_type(struct uart_port *port)
{
	struct ambarella_uart_port_info		*port_info;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	return port_info->name;
}

static inline void wait_for_tx(struct uart_port *port)
{
	u8					ls;

	ls = amba_readb(port->membase + UART_LS_OFFSET);
	while ((ls & UART_LS_TEMT) != UART_LS_TEMT) {
		cpu_relax();
		ls = amba_readb(port->membase + UART_LS_OFFSET);
	}
}

static inline void wait_for_rx(struct uart_port *port)
{
	u8					ls;

	ls = amba_readb(port->membase + UART_LS_OFFSET);
	while ((ls & UART_LS_DR) != UART_LS_DR) {
		cpu_relax();
		ls = amba_readb(port->membase + UART_LS_OFFSET);
	}
}

#ifdef CONFIG_CONSOLE_POLL
static void serial_ambarella_poll_put_char(struct uart_port *port,
	unsigned char chr)
{
	struct ambarella_uart_port_info		*port_info;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	wait_for_tx(port);
	amba_writeb(port->membase + UART_TH_OFFSET, chr);
}

static int serial_ambarella_poll_get_char(struct uart_port *port)
{
	struct ambarella_uart_port_info		*port_info;

	port_info = (struct ambarella_uart_port_info *)(port->private_data);

	wait_for_rx(port);
	return amba_readl(port->membase + UART_RB_OFFSET);
}
#endif

struct uart_ops serial_ambarella_pops = {
	.tx_empty	= serial_ambarella_tx_empty,
	.set_mctrl	= serial_ambarella_set_mctrl,
	.get_mctrl	= serial_ambarella_get_mctrl,
	.stop_tx	= serial_ambarella_stop_tx,
	.start_tx	= serial_ambarella_start_tx,
	.stop_rx	= serial_ambarella_stop_rx,
	.enable_ms	= serial_ambarella_enable_ms,
	.break_ctl	= serial_ambarella_break_ctl,
	.startup	= serial_ambarella_startup,
	.shutdown	= serial_ambarella_shutdown,
	.set_termios	= serial_ambarella_set_termios,
	.pm		= serial_ambarella_pm,
	.type		= serial_ambarella_type,
	.release_port	= serial_ambarella_release_port,
	.request_port	= serial_ambarella_request_port,
	.config_port	= serial_ambarella_config_port,
	.verify_port	= serial_ambarella_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_put_char	= serial_ambarella_poll_put_char,
	.poll_get_char	= serial_ambarella_poll_get_char,
#endif
};

#if defined(CONFIG_SERIAL_AMBARELLA_CONSOLE) && defined(CONFIG_SERIAL_AMBARELLA)
static struct uart_driver serial_ambarella_reg;

static void serial_ambarella_console_putchar(struct uart_port *port, int ch)
{
	wait_for_tx(port);
	amba_writeb(port->membase + UART_TH_OFFSET, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void serial_ambarella_console_write(struct console *co,
	const char *s, unsigned int count)
{
	u32					ie;
	struct uart_port			*port;

	port = (struct uart_port *)(ambarella_uart_ports.amba_port[co->index].port);

	disable_irq(port->irq);
	ie = amba_readl(port->membase + UART_IE_OFFSET);
	amba_writel(port->membase + UART_IE_OFFSET, ie & ~UART_IE_ETBEI);

	uart_console_write(port, s, count, serial_ambarella_console_putchar);

	wait_for_tx(port);
	amba_writel(port->membase + UART_IE_OFFSET, ie);
	enable_irq(port->irq);
}

static int __init serial_ambarella_console_setup(struct console *co,
	char *options)
{
	struct uart_port			*port;
	int					baud = 115200;
	int					bits = 8;
	int					parity = 'n';
	int					flow = 'n';

	if (co->index < 0 || co->index >= ambarella_uart_ports.total_port_num)
		co->index = 0;
	port = (struct uart_port *)(ambarella_uart_ports.amba_port[co->index].port);
	ambarella_uart_ports.set_pll();
	port->uartclk = ambarella_uart_ports.get_pll();
	port->ops = &serial_ambarella_pops;
	port->private_data = &(ambarella_uart_ports.amba_port[co->index]);
	port->line = co->index;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console serial_ambarella_console = {
	.name		= "ttyS",
	.write		= serial_ambarella_console_write,
	.device		= uart_console_device,
	.setup		= serial_ambarella_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &serial_ambarella_reg,
};

static int __init serial_ambarella_console_init(void)
{
	if (serial_ambarella_reg.nr == -1)
		serial_ambarella_reg.nr = ambarella_uart_ports.total_port_num;

	register_console(&serial_ambarella_console);

	return 0;
}

console_initcall(serial_ambarella_console_init);

#define AMBARELLA_CONSOLE	&serial_ambarella_console
#else
#define AMBARELLA_CONSOLE	NULL
#endif

static struct uart_driver serial_ambarella_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "ambarella-uart",
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= -1,
	.cons		= AMBARELLA_CONSOLE,
};

static int __devinit serial_ambarella_probe(struct platform_device *pdev)
{
	int					retval = 0;
	struct ambarella_uart_platform_info	*pinfo;
	struct uart_port			*port;

	pinfo = (struct ambarella_uart_platform_info *)pdev->dev.platform_data;
	if (pinfo == NULL) {
		dev_err(&pdev->dev, "Can't get UART platform data!\n");
		retval = -ENXIO;
		goto serial_ambarella_probe_exit;
	}

	if ((pdev->id < 0) || (pdev->id >= pinfo->total_port_num)) {
		dev_err(&pdev->dev, "Wrong UART ID %d!\n", pdev->id);
		retval = -ENXIO;
		goto serial_ambarella_probe_exit;
	}

	port = (struct uart_port *)(pinfo->amba_port[pdev->id].port);
	ambarella_uart_ports.set_pll();
	port->uartclk = ambarella_uart_ports.get_pll();
	port->ops = &serial_ambarella_pops;
	port->dev = &pdev->dev;
	port->private_data = &(pinfo->amba_port[pdev->id]);
	port->line = pdev->id;

	if (pinfo->registed_port_num == 0) {
		if (serial_ambarella_reg.nr != pinfo->total_port_num)
			serial_ambarella_reg.nr = pinfo->total_port_num;

		retval = uart_register_driver(&serial_ambarella_reg);
		if (retval) {
			dev_err(&pdev->dev,
				"uart_register_driver fail %d!\n",
				retval);
			goto serial_ambarella_probe_exit;
		}
	}

	retval = uart_add_one_port(&serial_ambarella_reg, port);
	if (retval) {
		dev_err(&pdev->dev,
			"uart_add_one_port %d fail %d!\n",
			pdev->id, retval);
		goto serial_ambarella_probe_unregister_driver;
	} else {
		pinfo->registed_port_num++;
	}

	goto serial_ambarella_probe_exit;

serial_ambarella_probe_unregister_driver:
	if (pinfo->registed_port_num == 0)
		uart_unregister_driver(&serial_ambarella_reg);

serial_ambarella_probe_exit:
	return retval;
}

static int __devexit serial_ambarella_remove(struct platform_device *pdev)
{
	int					retval = 0;
	struct ambarella_uart_platform_info	*pinfo;
	struct uart_port			*port;

	pinfo = (struct ambarella_uart_platform_info *)pdev->dev.platform_data;

	if (pinfo) {
		port = (struct uart_port *)(pinfo->amba_port[pdev->id].port);
		retval = uart_remove_one_port(&serial_ambarella_reg, port);
		if (retval) {
			dev_err(&pdev->dev,
				"uart_remove_one_port %d fail %d!\n",
				pdev->id, retval);
		} else {
			pinfo->registed_port_num--;
		}

		if (pinfo->registed_port_num == 0)
			uart_unregister_driver(&serial_ambarella_reg);

		dev_notice(&pdev->dev,
			"Remove Ambarella Media Processor UART.\n");
	}

	return retval;
}

#ifdef CONFIG_PM
static int serial_ambarella_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					retval = 0;
	struct ambarella_uart_platform_info	*pinfo;
	struct uart_port			*port;

	pinfo = (struct ambarella_uart_platform_info *)pdev->dev.platform_data;

	if (pinfo) {
		port = (struct uart_port *)(pinfo->amba_port[pdev->id].port);
		retval = uart_suspend_port(&serial_ambarella_reg, port);
	}
	dev_dbg(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, retval, state.event);

	return retval;
}

static int serial_ambarella_resume(struct platform_device *pdev)
{
	int					retval = 0;
	struct ambarella_uart_platform_info	*pinfo;
	struct uart_port			*port;

	pinfo = (struct ambarella_uart_platform_info *)pdev->dev.platform_data;

	if (pinfo) {
		port = (struct uart_port *)(pinfo->amba_port[pdev->id].port);
		retval = uart_resume_port(&serial_ambarella_reg, port);
	}
	dev_dbg(&pdev->dev, "%s exit with %d\n", __func__, retval);

	return retval;
}
#endif

static struct platform_driver serial_ambarella_driver = {
	.probe		= serial_ambarella_probe,
	.remove		= __devexit_p(serial_ambarella_remove),
#ifdef CONFIG_PM
	.suspend	= serial_ambarella_suspend,
	.resume		= serial_ambarella_resume,
#endif
	.driver		= {
		.name	= "ambarella-uart",
	},
};

int __init serial_ambarella_init(void)
{
	return platform_driver_register(&serial_ambarella_driver);
}

void __exit serial_ambarella_exit(void)
{
	platform_driver_unregister(&serial_ambarella_driver);
}

module_init(serial_ambarella_init);
module_exit(serial_ambarella_exit);

MODULE_AUTHOR("Charles Chiou");
MODULE_DESCRIPTION("Ambarella Media Processor UART driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ambarella-uart");

