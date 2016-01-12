/*
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/of_address.h>

static void __init ambarella_div_clocks_init(struct device_node *np)
{
	struct clk *clk;
	void __iomem *reg;
	const char *name;
	u32 width, shift;

	if (of_clk_get_parent_count(np) < 1) {
		pr_err("%s: no parent found\n", np->name);
		return;
	}

	reg = of_iomap(np, 0);
	BUG_ON(!reg);

	if (of_property_read_string(np, "clock-output-names", &name))
		name = np->name;

	if (of_property_read_u32(np, "amb,div-width", &width))
		width = 24;

	if (of_property_read_u32(np, "amb,div-shift", &shift))
		shift = 0;

	clk = clk_register_divider(NULL, name, of_clk_get_parent_name(np, 0),
			0, reg, shift, width, CLK_DIVIDER_ONE_BASED, NULL);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register %s div clock (%ld)\n",
		       __func__, name, PTR_ERR(clk));
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
	clk_register_clkdev(clk, name, NULL);
}
CLK_OF_DECLARE(ambarella_clk_div, "ambarella,div-clock", ambarella_div_clocks_init);

