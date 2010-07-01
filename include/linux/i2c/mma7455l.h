#ifndef __LINUX_I2C_MMA7455L_H
#define __LINUX_I2C_MMA7455L_H



#define GSEL_2 1
#define GSEL_4 2
#define GSEL_8 0


struct mma7455l_platform_data
{
int		g_select;
int		mode;
};

#endif /* __LINUX_I2C_MMA7455L_H */