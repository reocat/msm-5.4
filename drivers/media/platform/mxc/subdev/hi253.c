/*
 * drivers/media/video/Hi253.c
 *
 * Hi253 sensor driver
 *
 * Leveraged code from the hi253.c 
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 * modified :daqing
 */

#define DEBUG 1

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>

#include <linux/clk.h>
#include <linux/of_gpio.h>

#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>//linux-3.0
//#include "v4l2-int-device.h"
//#include "mxc_v4l2_capture.h"

#define HI253_I2C_NAME  "Hi253"
#define I2C_M_WR 0
#define __YUV_MODE 1

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

//#define HI253_XCLK_MIN 24000000
#define HI253_XCLK_MIN 24000000
#define HI253_XCLK_MAX 40000000

enum hi253_mode {
    hi253_mode_MIN = 0,
    hi253_mode_UXGA_1600_1200 = 0,
    hi253_mode_VGA_640_480 = 1,
    hi253_mode_QSVGA_320_240 = 2,
    hi253_mode_MAX = 2
};
enum hi253_frame_rate {
    hi253_15_fps,
    hi253_30_fps
};

///////

struct hi253 {
	struct v4l2_subdev		subdev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct ov5640_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	int hflip;
	int vflip;

	u32 mclk;
	u8 mclk_source;
	struct clk *sensor_clk;
	int csi;

	void (*io_init)(void);
};
static struct hi253 hi253_data;

/////////////////////

//struct sensor_data hi253_data;


static struct fsl_mxc_camera_platform_data *camera_plat;

struct reg_value {
    u8 u8RegAddr;
    u8 u8Val;
};

static int pwn_gpio, rst_gpio;
static int cam18_gpio, cam28_gpio;

struct hi253_mode_info {
    enum hi253_mode mode;
    u32 width;
    u32 height;
    struct reg_value *init_data_ptr;
    u32 init_data_size;
};


////////////////////////////////////

/*
 * Store information about the video data format. 
 */
static struct sensor_format_struct {
	__u8 *desc;
	//__u32 pixelformat;
	u32 mbus_code;//linux-3.0
	//struct regval_list *regs;
	//int	regs_size;
	int bpp;   /* Bytes per pixel */
} sensor_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		//.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,//linux-3.0
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,//linux-3.0
		//.regs 		= sensor_fmt_yuv422_yuyv,
		//.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yuyv),
		.bpp		= 2,
	},
#if 0
	{
		.desc		= "YVYU 4:2:2",
		.mbus_code	= MEDIA_BUS_FMT_YVYU8_2X8,//linux-3.0
		//.regs 		= sensor_fmt_yuv422_yvyu,
		//.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yvyu),
		.bpp		= 2,
	},
	{
		.desc		= "UYVY 4:2:2",
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,//linux-3.0
		//.regs 		= sensor_fmt_yuv422_uyvy,
		//.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_uyvy),
		.bpp		= 2,
	},
	{
		.desc		= "VYUY 4:2:2",
		.mbus_code	= MEDIA_BUS_FMT_VYUY8_2X8,//linux-3.0
		//.regs 		= sensor_fmt_yuv422_vyuy,
		//.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_vyuy),
		.bpp		= 2,
	},
//	{
//		.desc		= "Raw RGB Bayer",
//		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,//linux-3.0
//		.regs 		= sensor_fmt_raw,
//		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
//		.bpp		= 1
//	},
#endif
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

static struct hi253_mode_info hi253_mode_info_data[2][hi253_mode_MAX + 1];

static int hi253_change_mode(enum hi253_frame_rate frame_rate,
            enum hi253_mode new_mode,  enum hi253_mode orig_mode);

static int hi253_init_mode(enum hi253_frame_rate frame_rate,
            enum hi253_mode mode);

static int Optimus_set_AE_AWB(void);
static int Optimus_get_AE_AWB(void);
static struct hi253 *to_hi253(const struct i2c_client *client);
static inline void hi253_power_down(int enable);

static int sensor_try_fmt(struct v4l2_subdev *sd, 
             struct v4l2_mbus_framefmt *fmt);//linux-3.0

static int hi253_g_vflip(struct v4l2_subdev *s,
            struct v4l2_control *vc);

static int hi253_g_hflip(struct v4l2_subdev *s,
            struct v4l2_control *vc);

static int hi253_s_vflip(struct v4l2_subdev *s,
            struct v4l2_control *vc);

static int hi253_s_hflip(struct v4l2_subdev *s,
            struct v4l2_control *vc);

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
#if 0
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
	struct sensor_info *info = to_state(sd);
	char csi_reset_str[32];
  
	if(info->ccm_info->iocfg == 0) {
		strcpy(csi_reset_str,"csi_reset");
	} else if(info->ccm_info->iocfg == 1) {
	  strcpy(csi_reset_str,"csi_reset_b");
	}
	
	switch(val)
	{
		case CSI_SUBDEV_RST_OFF:
			csi_dev_dbg("CSI_SUBDEV_RST_OFF\n");
			gpio_write_one_pin_value(dev->csi_pin_hd,CSI_RST_OFF,csi_reset_str);
			msleep(10);
			break;
		case CSI_SUBDEV_RST_ON:
			csi_dev_dbg("CSI_SUBDEV_RST_ON\n");
			gpio_write_one_pin_value(dev->csi_pin_hd,CSI_RST_ON,csi_reset_str);
			msleep(10);
			break;
		case CSI_SUBDEV_RST_PUL:
			csi_dev_dbg("CSI_SUBDEV_RST_PUL\n");
			gpio_write_one_pin_value(dev->csi_pin_hd,CSI_RST_OFF,csi_reset_str);
			msleep(10);
			gpio_write_one_pin_value(dev->csi_pin_hd,CSI_RST_ON,csi_reset_str);
			msleep(100);
			gpio_write_one_pin_value(dev->csi_pin_hd,CSI_RST_OFF,csi_reset_str);
			msleep(10);
			break;
		default:
			return -EINVAL;
	}
#endif
		
	return 0;
}

static int sensor_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	/* Fill in min, max, step and default value for these controls. */
	/* see include/linux/videodev2.h for details */
	/* see sensor_s_parm and sensor_g_parm for the meaning of value */
	
#if 0
	switch (qc->id) {
	case V4L2_CID_BRIGHTNESS:
		return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
	case V4L2_CID_CONTRAST:
		return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
	case V4L2_CID_SATURATION:
		return v4l2_ctrl_query_fill(qc, -4, 4, 1, 1);
//	case V4L2_CID_HUE:
//		return v4l2_ctrl_query_fill(qc, -180, 180, 5, 0);
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
//	case V4L2_CID_GAIN:
//		return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
//	case V4L2_CID_AUTOGAIN:
//		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, -4, 4, 1, 0);
	case V4L2_CID_EXPOSURE_AUTO:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_DO_WHITE_BALANCE:
		return v4l2_ctrl_query_fill(qc, 0, 5, 1, 0);
	case V4L2_CID_AUTO_WHITE_BALANCE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	case V4L2_CID_COLORFX:
		return v4l2_ctrl_query_fill(qc, 0, 9, 1, 0);
	case V4L2_CID_CAMERA_FLASH_MODE:
	  return v4l2_ctrl_query_fill(qc, 0, 4, 1, 0);			
	}
#endif
	return -EINVAL;
}

static int sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	pr_debug("hi253 g_ctrl %d\n",ctrl->id);
	printk("hi253 g_ctrl %d\n",ctrl->id);
	int ret = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		//return sensor_g_brightness(sd, &ctrl->value);
		ctrl->value = sensor->brightness;
		break;
	case V4L2_CID_CONTRAST:
		//return sensor_g_contrast(sd, &ctrl->value);
		ctrl->value = sensor->contrast;
		break;
	case V4L2_CID_SATURATION:
		//return sensor_g_saturation(sd, &ctrl->value);
		ctrl->value = sensor->saturation;
		break;
	case V4L2_CID_HUE:
		//return sensor_g_hue(sd, &ctrl->value);	
		ctrl->value = sensor->hue;
		break;
        case V4L2_CID_RED_BALANCE:
          	ctrl->value = sensor->red;
          	break;
        case V4L2_CID_BLUE_BALANCE:
          	ctrl->value = sensor->blue;
          	break;
        case V4L2_CID_EXPOSURE:
          	ctrl->value = sensor->ae_mode;
          	break;
        case V4L2_CID_HFLIP:
          	hi253_g_hflip(sd,ctrl);         
          	break;
        case V4L2_CID_VFLIP:
          	hi253_g_vflip(sd,ctrl);         
          	break;
        default:
          	ret = -EINVAL;
	}

	return ret;
}

static int sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	pr_debug("hi253 s_ctrl %d\n",ctrl->id);
	int retval = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

    switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
          break;
        case V4L2_CID_CONTRAST:
          break;
        case V4L2_CID_SATURATION:
          break;
        case V4L2_CID_HUE:
          break;
        case V4L2_CID_AUTO_WHITE_BALANCE:
          break;
        case V4L2_CID_DO_WHITE_BALANCE:
          break;
        case V4L2_CID_RED_BALANCE:
          break;
        case V4L2_CID_BLUE_BALANCE:
          break;
        case V4L2_CID_GAMMA:
          break;
        case V4L2_CID_EXPOSURE:
          break;
        case V4L2_CID_AUTOGAIN:
          break;
        case V4L2_CID_GAIN:
          break;
        case V4L2_CID_HFLIP:
          retval = hi253_s_hflip(sd,ctrl);
          break;
        case V4L2_CID_VFLIP:
          retval = hi253_s_vflip(sd,ctrl);
          break;
#if 0
        case V4L2_CID_MXC_ROT:
        case V4L2_CID_MXC_VF_ROT:
          if(ctrl->value == V4L2_MXC_ROTATE_VERT_FLIP)
          {
            retval = hi253_s_vflip(sd,ctrl);
          }
          else if(ctrl->value == V4L2_MXC_ROTATE_VERT_FLIP)
          {
            retval = hi253_s_vflip(sd,ctrl);
          }
#endif
          break;
        default:
          retval = -EPERM;
          break;
    }

    return retval;

#if 0
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return sensor_s_brightness(sd, ctrl->value);
	case V4L2_CID_CONTRAST:
		return sensor_s_contrast(sd, ctrl->value);
	case V4L2_CID_SATURATION:
		return sensor_s_saturation(sd, ctrl->value);
	case V4L2_CID_HUE:
		return sensor_s_hue(sd, ctrl->value);		
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->value);
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->value);
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->value);
	case V4L2_CID_AUTOGAIN:
		return sensor_s_autogain(sd, ctrl->value);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->value);
	case V4L2_CID_EXPOSURE_AUTO:
		return sensor_s_autoexp(sd,
				(enum v4l2_exposure_auto_type) ctrl->value);
	case V4L2_CID_DO_WHITE_BALANCE:
		return sensor_s_wb(sd,
				(enum v4l2_whiteblance) ctrl->value);	
	case V4L2_CID_AUTO_WHITE_BALANCE:
		return sensor_s_autowb(sd, ctrl->value);
	case V4L2_CID_COLORFX:
		return sensor_s_colorfx(sd,
				(enum v4l2_colorfx) ctrl->value);
	case V4L2_CID_CAMERA_FLASH_MODE:
	  return sensor_s_flash_mode(sd,
	      (enum v4l2_flash_mode) ctrl->value);
	}

	return -EINVAL;
#endif
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	printk("senor_init\n");
#if 0
	int ret;
	csi_dev_dbg("sensor_init\n");
	/*Make sure it is a target sensor*/
	ret = sensor_detect(sd);
	if (ret) {
		csi_dev_err("chip found is not an target chip.\n");
		return ret;
	}
	return sensor_write_array(sd, sensor_default_regs , ARRAY_SIZE(sensor_default_regs));
#endif
	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	printk("hi253 sensor_power %d\n",on);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

    if (on && !sensor->on) {
        /* Make sure power on */
	hi253_power_down(0);
    } else if (!on && sensor->on) {
        Optimus_get_AE_AWB();  
        Optimus_set_AE_AWB();
	hi253_power_down(1);
    }
    sensor->on = on;
    return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret=0;
	
#if 0
	switch(cmd){
		case CSI_SUBDEV_CMD_GET_INFO: 
		{
			struct sensor_info *info = to_state(sd);
			__csi_subdev_info_t *ccm_info = arg;
			
			csi_dev_dbg("CSI_SUBDEV_CMD_GET_INFO\n");
			
			ccm_info->mclk 	=	info->ccm_info->mclk ;
			ccm_info->vref 	=	info->ccm_info->vref ;
			ccm_info->href 	=	info->ccm_info->href ;
			ccm_info->clock	=	info->ccm_info->clock;
			ccm_info->iocfg	=	info->ccm_info->iocfg;
			
			csi_dev_dbg("ccm_info.mclk=%x\n ",info->ccm_info->mclk);
			csi_dev_dbg("ccm_info.vref=%x\n ",info->ccm_info->vref);
			csi_dev_dbg("ccm_info.href=%x\n ",info->ccm_info->href);
			csi_dev_dbg("ccm_info.clock=%x\n ",info->ccm_info->clock);
			csi_dev_dbg("ccm_info.iocfg=%x\n ",info->ccm_info->iocfg);
			break;
		}
		case CSI_SUBDEV_CMD_SET_INFO:
		{
			struct sensor_info *info = to_state(sd);
			__csi_subdev_info_t *ccm_info = arg;
			
			csi_dev_dbg("CSI_SUBDEV_CMD_SET_INFO\n");
			
			info->ccm_info->mclk 	=	ccm_info->mclk 	;
			info->ccm_info->vref 	=	ccm_info->vref 	;
			info->ccm_info->href 	=	ccm_info->href 	;
			info->ccm_info->clock	=	ccm_info->clock	;
			info->ccm_info->iocfg	=	ccm_info->iocfg	;
			
			csi_dev_dbg("ccm_info.mclk=%x\n ",info->ccm_info->mclk);
			csi_dev_dbg("ccm_info.vref=%x\n ",info->ccm_info->vref);
			csi_dev_dbg("ccm_info.href=%x\n ",info->ccm_info->href);
			csi_dev_dbg("ccm_info.clock=%x\n ",info->ccm_info->clock);
			csi_dev_dbg("ccm_info.iocfg=%x\n ",info->ccm_info->iocfg);
			
			break;
		}
		default:
			return -EINVAL;
	}		
#endif

		return ret;
}


static int sensor_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	printk("g_chip_ident\n");
#if 0
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SENSOR, 0);
#endif
	return 0;
}

static int sensor_enum_fmt(struct v4l2_subdev *sd, unsigned index,
                 u32 *code)//linux-3.0
                 //enum v4l2_mbus_pixelcode *code)//linux-3.0
{
	printk("sensor enum fmt index=%d\n",index);
	if (index >= N_FMTS)//linux-3.0
		return -EINVAL;

	*code = sensor_formats[index].mbus_code;//linux-3.0
	return 0;
}

static int sensor_enum_mbus_code(struct v4l2_subdev *sd,
        struct v4l2_subdev_pad_config *cfg,
        struct v4l2_subdev_mbus_code_enum *code)
{
	pr_err("sensor enum fmt index=%d\n",code->index);
	if (code->index >= N_FMTS)//linux-3.0
		return -EINVAL;

	code->code = sensor_formats[code->index].mbus_code;//linux-3.0
	return 0;
}

#if 0
static int sensor_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,//linux-3.0
		struct sensor_format_struct **ret_fmt,
		struct sensor_win_size **ret_wsize)
{
	printk(" don't use hi253 try_fmt_internal\n");

	int index;
	struct sensor_win_size *wsize;
//	struct v4l2_pix_format *pix = &fmt->fmt.pix;//linux-3.0
	csi_dev_dbg("sensor_try_fmt_internal\n");
	for (index = 0; index < N_FMTS; index++)
		if (sensor_formats[index].mbus_code == fmt->code)//linux-3.0
			break;
	
	if (index >= N_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sensor_formats[0].mbus_code;//linux-3.0
	}
	
	if (ret_fmt != NULL)
		*ret_fmt = sensor_formats + index;
		
	/*
	 * Fields: the sensor devices claim to be progressive.
	 */
	fmt->field = V4L2_FIELD_NONE;//linux-3.0
	
	
	/*
	 * Round requested image size down to the nearest
	 * we support, but not below the smallest.
	 */
	for (wsize = sensor_win_sizes; wsize < sensor_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)//linux-3.0
			break;
	
	if (wsize >= sensor_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	/*
	 * Note the size we'll actually handle.
	 */
	fmt->width = wsize->width;//linux-3.0
	fmt->height = wsize->height;//linux-3.0
	//pix->bytesperline = pix->width*sensor_formats[index].bpp;//linux-3.0
	//pix->sizeimage = pix->height*pix->bytesperline;//linux-3.0
	

	return 0;
}
#endif

static struct hi253 *to_hi253(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct hi253, subdev);
}

static int sensor_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	printk("hi253 sensor_g_fmt\n");
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);
#if 0
	const struct hi253_datafmt *fmt = sensor->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
#endif

	return 0;
}

static int sensor_get_fmt(struct v4l2_subdev *sd,
        struct v4l2_subdev_pad_config *cfg,
        struct v4l2_subdev_format *format)
{
	printk("hi253 sensor_g_fmt\n");
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);
#if 0
	const struct hi253_datafmt *fmt = sensor->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
#endif

	return 0;
}

static int sensor_s_fmt(struct v4l2_subdev *sd, 
             struct v4l2_mbus_framefmt *fmt)//linux-3.0
{
	printk("hi253 sensor_s_fmt\n");

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

	sensor_try_fmt(sd,fmt);
	printk("hi253 w=%d,h=%d\n",fmt->width,fmt->height);

	int ret;
#if 0
	struct sensor_format_struct *sensor_fmt;
	struct sensor_win_size *wsize;
	struct sensor_info *info = to_state(sd);
	csi_dev_dbg("sensor_s_fmt\n");
	ret = sensor_try_fmt_internal(sd, fmt, &sensor_fmt, &wsize);
	if (ret)
		return ret;
	
		
	sensor_write_array(sd, sensor_fmt->regs , sensor_fmt->regs_size);
	
	ret = 0;
	if (wsize->regs)
	{
		ret = sensor_write_array(sd, wsize->regs , wsize->regs_size);
		if (ret < 0)
			return ret;
	}
	
	if (wsize->set_size)
	{
		ret = wsize->set_size(sd);
		if (ret < 0)
			return ret;
	}
	
	info->fmt = sensor_fmt;
	info->width = wsize->width;
	info->height = wsize->height;
#endif
	return 0;
}

static int sensor_set_fmt(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg,
    struct v4l2_subdev_format *format)
{
	printk("hi253 set_fmt\n");

    struct v4l2_mbus_framefmt *fmt = &format->format;
    
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

	sensor_try_fmt(sd, fmt);
	printk("hi253 w=%d,h=%d\n",fmt->width,fmt->height);

	int ret;
}


/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	printk("hi253 sensor_g_parm\n");

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);
	struct v4l2_captureparm *cparm = &parms->parm.capture;
	int ret = 0;

    switch (parms->type) {
        /* This is the only case currently handled. */
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          memset(parms, 0, sizeof(*parms));
          parms->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          cparm->capability = sensor->streamcap.capability;
          cparm->timeperframe = sensor->streamcap.timeperframe;
          cparm->capturemode = sensor->streamcap.capturemode;
          ret = 0;
          break;

          /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        case V4L2_BUF_TYPE_VBI_CAPTURE:
        case V4L2_BUF_TYPE_VBI_OUTPUT:
        case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          ret = -EINVAL;
          break;

        default:
          pr_debug("   type is unknown - %d\n", parms->type);
          ret = -EINVAL;
          break;
    }

	return ret;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	printk("hi253 sensor s_parm");

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hi253 *sensor = to_hi253(client);

    struct v4l2_fract *timeperframe = &parms->parm.capture.timeperframe;
    u32 tgt_fps;	/* target frames per secound */
    enum hi253_frame_rate frame_rate;
    int ret = 0;

    /* Make sure power on */
    //if (camera_plat->pwdn)
     // camera_plat->pwdn(0);
	hi253_power_down(0);

    switch (parms->type) {
        /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          /* Check that the new frame rate is allowed. */
          if ((timeperframe->numerator == 0) ||
                      (timeperframe->denominator == 0)) {
              timeperframe->denominator = DEFAULT_FPS;
              timeperframe->numerator = 1;
          }

          tgt_fps = timeperframe->denominator /
              timeperframe->numerator;

          if (tgt_fps > MAX_FPS) {
              timeperframe->denominator = MAX_FPS;
              timeperframe->numerator = 1;
          } else if (tgt_fps < MIN_FPS) {
              timeperframe->denominator = MIN_FPS;
              timeperframe->numerator = 1;
          }

          /* Actual frame rate we use */
          tgt_fps = timeperframe->denominator /
              timeperframe->numerator;

          if (tgt_fps == 15)
            frame_rate = hi253_15_fps;
          else if (tgt_fps == 30)
            frame_rate = hi253_30_fps;
          else {
              pr_err(" The camera frame rate is not supported!\n");
              return -EINVAL;
          }

          ret =  hi253_change_mode(frame_rate, parms->parm.capture.capturemode, sensor->streamcap.capturemode);
          sensor->streamcap.timeperframe = *timeperframe;
          sensor->streamcap.capturemode =
              (u32)parms->parm.capture.capturemode;
          break;
          /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        case V4L2_BUF_TYPE_VBI_CAPTURE:
        case V4L2_BUF_TYPE_VBI_OUTPUT:
        case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          pr_debug("   type is not " \
                      "V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
                      parms->type);
          ret = -EINVAL;
          break;

        default:
          pr_debug("   type is unknown - %d\n", parms->type);
          ret = -EINVAL;
          break;
    }

    return ret;
}


/*!
 * hi253_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int hi253_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
    if (fse->index > hi253_mode_MAX)
      return -EINVAL;

    //fse->pixel_format = hi253_data.pix.pixelformat;

    fse->max_width =
        max(hi253_mode_info_data[0][fse->index].width,
                    hi253_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;

    fse->max_height =
        max(hi253_mode_info_data[0][fse->index].height,
                    hi253_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;

    return 0;


#if 0
	if (fse->index > ov5640_mode_MAX)
		return -EINVAL;

	fse->max_width =
			max(ov5640_mode_info_data[0][fse->index].width,
			    ov5640_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;
	fse->max_height =
			max(ov5640_mode_info_data[0][fse->index].height,
			    ov5640_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;

	return 0;
#endif

}

/*!
 * hi253_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int hi253_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{

#if 0
	int i, j, count;

	if (fie->index < 0 || fie->index > ov5640_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		pr_warning("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(ov5640_mode_info_data); i++) {
		for (j = 0; j < (ov5640_mode_MAX + 1); j++) {
			if (fie->width == ov5640_mode_info_data[i][j].width
			 && fie->height == ov5640_mode_info_data[i][j].height
			 && ov5640_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->interval.denominator =
						ov5640_framerates[i];
				return 0;
			}
		}
	}
#endif
	return -EINVAL;
}

////////////////////////////////////








#if 0
static int Optimus_preset(struct v4l2_int_device *s,
            struct v4l2_control *vc);
#endif

static int Optimus_get_AE_AWB(void);
static s32 hi253_write_reg(u8 reg, u8 val);
static u8 hi253_read_reg(u8 reg);

static u8 hi253_read_reg(u8 reg)
{
    u8 reg_a = reg;
    u8 buf;

    if (i2c_master_send(hi253_data.i2c_client, &reg_a, 1) < 0) {
        pr_err("%s:write reg error:reg=%x\n",
                    __func__, reg_a);
        return -1;
    }

    if (i2c_master_recv(hi253_data.i2c_client, &buf, 1) < 0) {
        pr_err("%s:read reg error:reg=%x\n",
                    __func__, reg_a);
        return -1;
    }

    return buf;
}

static struct reg_value Hi253_ae_special_1[]=
{
    {0x83, 0x01}, //EXP Normal 33.33 fps 
    {0x84, 0x5b}, 
    {0x85, 0xd6}, 
};

static struct reg_value Hi253_ae_special_2[]=
{
    {0xb0, 0x30},
};

static struct reg_value Hi253_awb_special[]=
{
    {0x80, 0x3c}, //3e
    {0x81, 0x20},
    {0x82, 0x34},
};

const static struct reg_value Hi253_common_svgabase_1[] = {
    /////// Start Sleep ///////
    {0x01, 0xf9}, //sleep on
    {0x08, 0x0f}, //Hi-Z on
    {0x01, 0xf8}, //sleep off
    
    {0x03, 0x00}, // Dummy 750us START
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00}, // Dummy 750us END
    
    {0x0e, 0x03}, //PLL On
    {0x0e, 0x77}, //PLLx4 
    
    {0x03, 0x00}, // Dummy 750us START
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00}, // Dummy 750us END
    
    {0x0e, 0x00}, //PLL off
    {0x01, 0xf1}, //sleep on
    {0x08, 0x00}, //Hi-Z off
    
    {0x01, 0xf3},
    {0x01, 0xf1},
    
    // PAGE 20
    {0x03, 0x20}, //page 20
    {0x10, 0x1c}, //ae off
    
    // PAGE 22
    {0x03, 0x22}, //page 22
    {0x10, 0x69}, //awb off
    
    
    //Initial Start
    /////// PAGE 0 START ///////
    {0x03, 0x00},
    {0x10, 0x00}, // Sub = UVGA 1 
    {0x11, 0x93},//TODO
    {0x12, 0x04},// old version 
    
    {0x0b, 0xaa}, // ESD Check Register
    {0x0c, 0xaa}, // ESD Check Register
    {0x0d, 0xaa}, // ESD Check Register
    
    {0x20, 0x00}, // Windowing start point Y
    {0x21, 0x04},
    {0x22, 0x00}, // Windowing start point X
    {0x23, 0x07},
    
    {0x24, 0x04},
    {0x25, 0xb0},
    {0x26, 0x06},
    {0x27, 0x40}, // WINROW END
    
    {0x40, 0x01}, //Hblank 408
    {0x41, 0x98}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14},
    
    {0x45, 0x04},
    {0x46, 0x18},
    {0x47, 0xd8},
    
    //BLC
    {0x80, 0x2e},
    {0x81, 0x7e},
    {0x82, 0x90},
    {0x83, 0x00},
    {0x84, 0x0c},
    {0x85, 0x00},
    {0x90, 0x0a}, //BLC_TIME_TH_ON
    {0x91, 0x0a}, //BLC_TIME_TH_OFF 
    {0x92, 0xd8}, //BLC_AG_TH_ON
    {0x93, 0xd0}, //BLC_AG_TH_OFF
    {0x94, 0x75},
    {0x95, 0x70},
    {0x96, 0xdc},
    {0x97, 0xfe},
    {0x98, 0x38},
    
    //OutDoor  BLC
    {0x99, 0x43},
    {0x9a, 0x43},
    {0x9b, 0x43},
    {0x9c, 0x43},
    
    //Dark BLC
    {0xa0, 0x11},//00
    {0xa2, 0x11},
    {0xa4, 0x11},
    {0xa6, 0x11},
    
    //Normal BLC
    {0xa8, 0x43},//43
    {0xaa, 0x43},
    {0xac, 0x43},
    {0xae, 0x43},
    
    /////// PAGE 2 START ///////
    {0x03, 0x02},
    {0x12, 0x03},
    {0x13, 0x03},
    {0x16, 0x00},
    {0x17, 0x8C},
    {0x18, 0x4c}, //Double_AG off
    {0x19, 0x00},
    {0x1a, 0x39}, //ADC400->560
    {0x1c, 0x09},
    {0x1d, 0x40},
    {0x1e, 0x30},
    {0x1f, 0x10},
    
    {0x20, 0x77},
    {0x21, 0xde},
    {0x22, 0xa7},
    {0x23, 0x30}, //CLAMP
    {0x27, 0x3c},
    {0x2b, 0x80},
    {0x2e, 0x11},
    {0x2f, 0xa1},
    {0x30, 0x05}, //For Hi-253 never no change 0x05
    
    {0x50, 0x20},
    {0x52, 0x01},
    {0x53, 0xc1}, //
    {0x55, 0x1c},
    {0x56, 0x11},
    {0x5d, 0xa2},
    {0x5e, 0x5a},
    
    {0x60, 0x87},
    {0x61, 0x99},
    {0x62, 0x88},
    {0x63, 0x97},
    {0x64, 0x88},
    {0x65, 0x97},
    
    {0x67, 0x0c},
    {0x68, 0x0c},
    {0x69, 0x0c},
    
    {0x72, 0x89},
    {0x73, 0x96},
    {0x74, 0x89},
    {0x75, 0x96},
    {0x76, 0x89},
    {0x77, 0x96},
    
    {0x7c, 0x85},
    {0x7d, 0xaf},
    {0x80, 0x01},
    {0x81, 0x7f},
    {0x82, 0x13},
    
    {0x83, 0x24},
    {0x84, 0x7d},
    {0x85, 0x81},
    {0x86, 0x7d},
    {0x87, 0x81},
    
    {0x92, 0x48},
    {0x93, 0x54},
    {0x94, 0x7d},
    {0x95, 0x81},
    {0x96, 0x7d},
    {0x97, 0x81},
    
    {0xa0, 0x02},
    {0xa1, 0x7b},
    {0xa2, 0x02},
    {0xa3, 0x7b},
    {0xa4, 0x7b},
    {0xa5, 0x02},
    {0xa6, 0x7b},
    {0xa7, 0x02},
    
    {0xa8, 0x85},
    {0xa9, 0x8c},
    {0xaa, 0x85},
    {0xab, 0x8c},
    {0xac, 0x10},
    {0xad, 0x16},
    {0xae, 0x10},
    {0xaf, 0x16},
    
    {0xb0, 0x99},
    {0xb1, 0xa3},
    {0xb2, 0xa4},
    {0xb3, 0xae},
    {0xb4, 0x9b},
    {0xb5, 0xa2},
    {0xb6, 0xa6},
    {0xb7, 0xac},
    {0xb8, 0x9b},
    {0xb9, 0x9f},
    {0xba, 0xa6},
    {0xbb, 0xaa},
    {0xbc, 0x9b},
    {0xbd, 0x9f},
    {0xbe, 0xa6},
    {0xbf, 0xaa},
    
    {0xc4, 0x2c},
    {0xc5, 0x43},
    {0xc6, 0x63},
    {0xc7, 0x79},
    
    {0xc8, 0x2d},
    {0xc9, 0x42},
    {0xca, 0x2d},
    {0xcb, 0x42},
    {0xcc, 0x64},
    {0xcd, 0x78},
    {0xce, 0x64},
    {0xcf, 0x78},
    
    {0xd0, 0x0a},
    {0xd1, 0x09},
    {0xd4, 0x0c}, //DCDC_TIME_TH_ON//0a
    {0xd5, 0x0c}, //DCDC_TIME_TH_OFF //0a
    {0xd6, 0x78}, //DCDC_AG_TH_ON  98
    {0xd7, 0x70}, //DCDC_AG_TH_OFF  90
    {0xe0, 0xc4},
    {0xe1, 0xc4},
    {0xe2, 0xc4},
    {0xe3, 0xc4},
    {0xe4, 0x00},
    {0xe8, 0x80},
    {0xe9, 0x40},
    {0xea, 0x7f},
    
    {0xf0, 0x01},  //   sram1_cfg     
    {0xf1, 0x01},  //   sram2_cfg     
    {0xf2, 0x01},  //   sram3_cfg     
    {0xf3, 0x01},  //   sram4_cfg     
    {0xf4, 0x01},  //   sram5_cfg     
    
    
    /////// PAGE 3 ///////
    {0x03, 0x03},
    {0x10, 0x10},
    
    /////// PAGE 10 START ///////
    {0x03, 0x10},
    {0x10, 0x03}, // CrYCbY // For Demoset 0x03
    {0x12, 0x30},
    {0x13, 0x0a}, // contrast on
    {0x20, 0x00},
    
    {0x30, 0x00},
    {0x31, 0x00},
    {0x32, 0x00},
    {0x33, 0x00},
    
    {0x34, 0x30},
    {0x35, 0x00},
    {0x36, 0x00},
    {0x38, 0x00},
    {0x3e, 0x58},
    {0x3f, 0x00},
    
    {0x40, 0x90}, // YOFS  80
    {0x41, 0x05}, // DYOFS   05
    {0x48, 0x90}, // Contrast  80
    
    {0x60, 0x67},
    {0x61, 0x7c}, //7e //8e //88 //80
    {0x62, 0x7c}, //7e //8e //88 //80
    {0x63, 0x50}, //Double_AG 50->30
    {0x64, 0x41},
    
    {0x66, 0x42},
    {0x67, 0x20},
    
    {0x6a, 0x70}, //8a
    {0x6b, 0x84}, //74
    {0x6c, 0x80}, //7e //7a
    {0x6d, 0x80}, //8e
    
    //Don't touch//////////////////////////
    //{0x72, 0x84},
    //{0x76, 0x19},
    //{0x73, 0x70},
    //{0x74, 0x68},
    //{0x75, 0x60}, // white protection ON
    //{0x77, 0x0e}, //08 //0a
    //{0x78, 0x2a}, //20
    //{0x79, 0x08},
    ////////////////////////////////////////
    
    /////// PAGE 11 START ///////
    {0x03, 0x11},
    {0x10, 0x7f},
    {0x11, 0x40},
    {0x12, 0x0a}, // Blue Max-Filter Delete
    {0x13, 0xbb},
    
    {0x26, 0x31}, // Double_AG 31->20
    {0x27, 0x34}, // Double_AG 34->22
    {0x28, 0x0f},
    {0x29, 0x10},
    {0x2b, 0x30},
    {0x2c, 0x32},
    
    //Out2 D-LPF th
    {0x30, 0x70},
    {0x31, 0x10},
    {0x32, 0x58},
    {0x33, 0x09},
    {0x34, 0x06},
    {0x35, 0x03},
    
    //Out1 D-LPF th
    {0x36, 0x70},
    {0x37, 0x18},
    {0x38, 0x58},
    {0x39, 0x09},
    {0x3a, 0x06},
    {0x3b, 0x03},
    
    //Indoor D-LPF th
    {0x3c, 0x80},
    {0x3d, 0x18},
    {0x3e, 0xa0}, //80
    {0x3f, 0x0c},
    {0x40, 0x09},
    {0x41, 0x06},
    
    {0x42, 0x80},
    {0x43, 0x18},
    {0x44, 0xa0}, //80
    {0x45, 0x12},
    {0x46, 0x10},
    {0x47, 0x10},
    
    {0x48, 0x90},
    {0x49, 0x40},
    {0x4a, 0x80},
    {0x4b, 0x13},
    {0x4c, 0x10},
    {0x4d, 0x11},
    
    {0x4e, 0x80},
    {0x4f, 0x30},
    {0x50, 0x80},
    {0x51, 0x13},
    {0x52, 0x10},
    {0x53, 0x13},
    
    {0x54, 0x11},
    {0x55, 0x17},
    {0x56, 0x20},
    {0x57, 0x01},
    {0x58, 0x00},
    {0x59, 0x00},
    
    {0x5a, 0x1f}, //18
    {0x5b, 0x00},
    {0x5c, 0x00},
    
    {0x60, 0x3f},
    {0x62, 0x60},
    {0x70, 0x06},
    
    /////// PAGE 12 START ///////
    {0x03, 0x12},
    {0x20, 0x0f},
    {0x21, 0x0f},
    
    {0x25, 0x00}, //0x30
    
    {0x28, 0x00},
    {0x29, 0x00},
    {0x2a, 0x00},
    
    {0x30, 0x50},
    {0x31, 0x18},
    {0x32, 0x32},
    {0x33, 0x40},
    {0x34, 0x50},
    {0x35, 0x70},
    {0x36, 0xa0},
    
    //Out2 th
    {0x40, 0xa0},
    {0x41, 0x40},
    {0x42, 0xa0},
    {0x43, 0x90},
    {0x44, 0x90},
    {0x45, 0x80},
    
    //Out1 th
    {0x46, 0xb0},
    {0x47, 0x55},
    {0x48, 0xa0},
    {0x49, 0x90},
    {0x4a, 0x90},
    {0x4b, 0x80},
    
    //Indoor th
    {0x4c, 0xb0},
    {0x4d, 0x40},
    {0x4e, 0x90},
    {0x4f, 0x90},
    {0x50, 0xa0},
    {0x51, 0xff},//80
    
    //Dark1 th
    {0x52, 0xb0},
    {0x53, 0x60},
    {0x54, 0xc0},
    {0x55, 0xc0},
    {0x56, 0xc0},
    {0x57, 0x80},
    
    //Dark2 th
    {0x58, 0x90},
    {0x59, 0x40},
    {0x5a, 0xd0},
    {0x5b, 0xd0},
    {0x5c, 0xe0},
    {0x5d, 0x80},
    
    //Dark3 th
    {0x5e, 0x88},
    {0x5f, 0x40},
    {0x60, 0xe0},
    {0x61, 0xe0},
    {0x62, 0xe0},
    {0x63, 0x80},
    
    {0x70, 0x15},
    {0x71, 0x01}, //Don't Touch register
    
    {0x72, 0x18},
    {0x73, 0x01}, //Don't Touch register
    {0x90, 0x5d}, //DPC
    {0x91, 0x88},       
    {0x98, 0x7d},       
    {0x99, 0x28},       
    {0x9A, 0x14},       
    {0x9B, 0xc8},       
    {0x9C, 0x02},       
    {0x9D, 0x1e},       
    {0x9E, 0x28},       
    {0x9F, 0x07},       
    {0xA0, 0x32},       
    {0xA4, 0x04},       
    {0xA5, 0x0e},       
    {0xA6, 0x0c},       
    {0xA7, 0x04},       
    {0xA8, 0x3c},       
    
    {0xAA, 0x14},       
    {0xAB, 0x11},       
    {0xAC, 0x0f},       
    {0xAD, 0x16},       
    {0xAE, 0x15},       
    {0xAF, 0x14},       
    
    {0xB1, 0xaa},       
    {0xB2, 0x96},       
    {0xB3, 0x28},       
    //{0xB6,read}, only//dpc_flat_thres
    //{0xB7,read}, only//dpc_grad_cnt
    {0xB8, 0x78},       
    {0xB9, 0xa0},       
    {0xBA, 0xb4},       
    {0xBB, 0x14},       
    {0xBC, 0x14},       
    {0xBD, 0x14},       
    {0xBE, 0x64},       
    {0xBF, 0x64},       
    {0xC0, 0x64},       
    {0xC1, 0x64},       
    {0xC2, 0x04},       
    {0xC3, 0x03},       
    {0xC4, 0x0c},       
    {0xC5, 0x30},       
    {0xC6, 0x2a},       
    {0xD0, 0x0c}, //CI Option/CI DPC
    {0xD1, 0x80},       
    {0xD2, 0x67},       
    {0xD3, 0x00},       
    {0xD4, 0x00},       
    {0xD5, 0x02},       
    {0xD6, 0xff},       
    {0xD7, 0x18},   
    
    /////// PAGE 13 START ///////
    {0x03, 0x13},
    //Edge
    {0x10, 0xcb},
    {0x11, 0x7b},
    {0x12, 0x07},
    {0x14, 0x00},
    
    {0x20, 0x15},
    {0x21, 0x13},
    {0x22, 0x33},
    {0x23, 0x05},
    {0x24, 0x09},
    
    {0x25, 0x0a},
    
    {0x26, 0x18},
    {0x27, 0x30},
    {0x29, 0x12},
    {0x2a, 0x50},
    
    //Low clip th
    {0x2b, 0x00}, //Out2 02
    {0x2c, 0x00}, //Out1 02 //01
    {0x25, 0x06},
    {0x2d, 0x0c},
    {0x2e, 0x12},
    {0x2f, 0x12},
    
    //Out2 Edge
    {0x50, 0x18}, //0x10 //0x16
    {0x51, 0x1c}, //0x14 //0x1a
    {0x52, 0x1a}, //0x12 //0x18
    {0x53, 0x14}, //0x0c //0x12
    {0x54, 0x17}, //0x0f //0x15
    {0x55, 0x14}, //0x0c //0x12
    
    //Out1 Edge          //Edge
    {0x56, 0x18}, //0x10 //0x16
    {0x57, 0x1c}, //0x13 //0x1a
    {0x58, 0x1a}, //0x12 //0x18
    {0x59, 0x14}, //0x0c //0x12
    {0x5a, 0x17}, //0x0f //0x15
    {0x5b, 0x14}, //0x0c //0x12
    
    //Indoor Edge
    {0x5c, 0x0a},
    {0x5d, 0x0b},
    {0x5e, 0x0a},
    {0x5f, 0x08},
    {0x60, 0x09},
    {0x61, 0x08},
    
    //Dark1 Edge
    {0x62, 0x08},
    {0x63, 0x08},
    {0x64, 0x08},
    {0x65, 0x06},
    {0x66, 0x06},
    {0x67, 0x06},
    
    //Dark2 Edge
    {0x68, 0x07},
    {0x69, 0x07},
    {0x6a, 0x07},
    {0x6b, 0x05},
    {0x6c, 0x05},
    {0x6d, 0x05},
    
    //Dark3 Edge
    {0x6e, 0x07},
    {0x6f, 0x07},
    {0x70, 0x07},
    {0x71, 0x05},
    {0x72, 0x05},
    {0x73, 0x05},
    
    //2DY
    {0x80, 0xfd},
    {0x81, 0x1f},
    {0x82, 0x05},
    {0x83, 0x31},
    
    {0x90, 0x05},
    {0x91, 0x05},
    {0x92, 0x33},
    {0x93, 0x30},
    {0x94, 0x03},
    {0x95, 0x14},
    {0x97, 0x20},
    {0x99, 0x20},
    
    {0xa0, 0x01},
    {0xa1, 0x02},
    {0xa2, 0x01},
    {0xa3, 0x02},
    {0xa4, 0x05},
    {0xa5, 0x05},
    {0xa6, 0x07},
    {0xa7, 0x08},
    {0xa8, 0x07},
    {0xa9, 0x08},
    {0xaa, 0x07},
    {0xab, 0x08},
    
    //Out2 
    {0xb0, 0x22},
    {0xb1, 0x2a},
    {0xb2, 0x28},
    {0xb3, 0x22},
    {0xb4, 0x2a},
    {0xb5, 0x28},
    
    //Out1 
    {0xb6, 0x22},
    {0xb7, 0x2a},
    {0xb8, 0x28},
    {0xb9, 0x22},
    {0xba, 0x2a},
    {0xbb, 0x28},
    
    //Indoor 
    {0xbc, 0x25},
    {0xbd, 0x2a},
    {0xbe, 0x27},
    {0xbf, 0x25},
    {0xc0, 0x2a},
    {0xc1, 0x27},
    
    //Dark1
    {0xc2, 0x1e},
    {0xc3, 0x24},
    {0xc4, 0x20},
    {0xc5, 0x1e},
    {0xc6, 0x24},
    {0xc7, 0x20},
    
    //Dark2
    {0xc8, 0x18},
    {0xc9, 0x20},
    {0xca, 0x1e},
    {0xcb, 0x18},
    {0xcc, 0x20},
    {0xcd, 0x1e},
    
    //Dark3 
    {0xce, 0x18},
    {0xcf, 0x20},
    {0xd0, 0x1e},
    {0xd1, 0x18},
    {0xd2, 0x20},
    {0xd3, 0x1e},
    
    /////// PAGE 14 START ///////
    {0x03, 0x14},
    {0x10, 0x11},
    
    {0x14, 0x80}, // GX
    {0x15, 0x80}, // GY
    {0x16, 0x80}, // RX
    {0x17, 0x80}, // RY
    {0x18, 0x80}, // BX
    {0x19, 0x80}, // BY
    
    {0x20, 0x66}, //X 60 //a0
    {0x21, 0xa0}, //Y
    
    {0x22, 0xa7},
    {0x23, 0xaa},
    {0x24, 0xce},
    
    {0x30, 0xc8},
    {0x31, 0x2b},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x90},
    
    {0x40, 0x48}, //31
    {0x50, 0x34}, //23 //32
    {0x60, 0x29}, //1a //27
    {0x70, 0x34}, //23 //32
    
    /////// PAGE 15 START ///////
    {0x03, 0x15},
    {0x10, 0x0f},
    
    //Rstep H 16
    //Rstep L 14
    {0x14, 0x42}, //CMCOFSGH_Day //4c
    {0x15, 0x32}, //CMCOFSGM_CWF //3c
    {0x16, 0x24}, //CMCOFSGL_A //2e
    {0x17, 0x2f}, //CMC SIGN
    
    //CMC_Default_CWF
    {0x30, 0x8f},
    {0x31, 0x59},
    {0x32, 0x0a},
    {0x33, 0x15},
    {0x34, 0x5b},
    {0x35, 0x06},
    {0x36, 0x07},
    {0x37, 0x40},
    {0x38, 0x87}, //86
    
    //CMC OFS L_A
    {0x40, 0x92},
    {0x41, 0x1b},
    {0x42, 0x89},
    {0x43, 0x81},
    {0x44, 0x00},
    {0x45, 0x01},
    {0x46, 0x89},
    {0x47, 0x9e},
    {0x48, 0x28},
    //CMC POFS H_DAY
    {0x50, 0x02},
    {0x51, 0x82},
    {0x52, 0x00},
    {0x53, 0x07},
    {0x54, 0x11},
    {0x55, 0x98},
    {0x56, 0x00},
    {0x57, 0x0b},
    {0x58, 0x8b},
    
    {0x80, 0x03},
    {0x85, 0x40},
    {0x87, 0x02},
    {0x88, 0x00},
    {0x89, 0x00},
    {0x8a, 0x00},
    
    /////// PAGE 16 START ///////
    {0x03, 0x16},
    {0x10, 0x31},
    {0x18, 0x5e},// Double_AG 5e->37
    {0x19, 0x5d},// Double_AG 5e->36
    {0x1a, 0x0e},
    {0x1b, 0x01},
    {0x1c, 0xdc},
    {0x1d, 0xfe},
    
    //GMA Default
    {0x30, 0x00},
    {0x31, 0x0a},
    {0x32, 0x1f},
    {0x33, 0x33},
    {0x34, 0x53},
    {0x35, 0x6c},
    {0x36, 0x81},
    {0x37, 0x94},
    {0x38, 0xa4},
    {0x39, 0xb3},
    {0x3a, 0xc0},
    {0x3b, 0xcb},
    {0x3c, 0xd5},
    {0x3d, 0xde},
    {0x3e, 0xe6},
    {0x3f, 0xee},
    {0x40, 0xf5},
    {0x41, 0xfc},
    {0x42, 0xff},
    
    {0x50, 0x00},
    {0x51, 0x09},
    {0x52, 0x1f},
    {0x53, 0x37},
    {0x54, 0x5b},
    {0x55, 0x76},
    {0x56, 0x8d},
    {0x57, 0xa1},
    {0x58, 0xb2},
    {0x59, 0xbe},
    {0x5a, 0xc9},
    {0x5b, 0xd2},
    {0x5c, 0xdb},
    {0x5d, 0xe3},
    {0x5e, 0xeb},
    {0x5f, 0xf0},
    {0x60, 0xf5},
    {0x61, 0xf7},
    {0x62, 0xf8},
    
    {0x70, 0x00},
    {0x71, 0x08},
    {0x72, 0x17},
    {0x73, 0x2f},
    {0x74, 0x53},
    {0x75, 0x6c},
    {0x76, 0x81},
    {0x77, 0x94},
    {0x78, 0xa4},
    {0x79, 0xb3},
    {0x7a, 0xc0},
    {0x7b, 0xcb},
    {0x7c, 0xd5},
    {0x7d, 0xde},
    {0x7e, 0xe6},
    {0x7f, 0xee},
    {0x80, 0xf4},
    {0x81, 0xfa},
    {0x82, 0xff},
    
    /////// PAGE 17 START ///////
    {0x03, 0x17},
    {0x10, 0xf7},
    
    /////// PAGE 20 START ///////
    {0x03, 0x20},
    {0x11, 0x1c},
    {0x18, 0x30},
    {0x1a, 0x08},
    {0x20, 0x01},
    {0x21, 0x30},
    {0x22, 0x10},
    {0x23, 0x00},
    {0x24, 0x00},
    
    {0x28, 0xe7},
    {0x29, 0x0d},
    
                                     
    {0x2a, 0xff},
    {0x2b, 0x04}, //f4->Adaptive off
    {0x2c, 0xc2},
    {0x2d, 0xcf},
    {0x2e, 0x33},
    {0x30, 0x78},
    {0x32, 0x03},
    {0x33, 0x2e},
    {0x34, 0x30},
    {0x35, 0xd4},
    {0x36, 0xfe},
    {0x37, 0x32},
    {0x38, 0x04},
    {0x39, 0x22},
    {0x3a, 0xde},
    {0x3b, 0x22},
    {0x3c, 0xde},
    
                                     
    {0x50, 0x45},
    {0x51, 0x88},
    
    {0x56, 0x03},
    {0x57, 0xf7},
    {0x58, 0x14},
    {0x59, 0x88},
    {0x5a, 0x04},
    
    {0x60, 0x55},
    {0x61, 0x55},
    {0x62, 0x6a},
    {0x63, 0xa9},
    {0x64, 0x6a},
    {0x65, 0xa9},
    {0x66, 0x6a},
    {0x67, 0xa9},
    {0x68, 0x6b},
    {0x69, 0xe9},
    {0x6a, 0x6a},
    {0x6b, 0xa9},
    {0x6c, 0x6a},
    {0x6d, 0xa9},
    {0x6e, 0x55},
    {0x6f, 0x55},
    {0x70, 0x76}, //6e
    {0x71, 0x00}, //82(+8)->+0

                                 
    {0x76, 0x43},
    {0x77, 0xe2},
    {0x78, 0x23},
    {0x79, 0x42}, //Yth2

    {0x7a, 0x23},
    {0x7b, 0x22},
    {0x7d, 0x23},
};    
const static struct reg_value Hi253_common_svgabase_2[] = {
    {0x86, 0x02}, //EXPMin 5859.38 fps
    {0x87, 0x00}, 
    
    {0x88, 0x05}, //EXP Max 10.00 fps 
    {0x89, 0x7c}, 
    {0x8a, 0x00}, 
    
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x00}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0x00}, 
    
    {0x9c, 0x18}, //EXP Limit 488.28 fps 
    {0x9d, 0x00}, 
    {0x9e, 0x02}, //EXP Unit 
    {0x9f, 0x00}, 
};    
const static struct reg_value Hi253_common_svgabase_3[] = {
    {0xb1, 0x14}, //ADC 400->560
    {0xb2, 0x80}, //d0   e0
    {0xb3, 0x18},
    {0xb4, 0x1a},
    {0xb5, 0x44},
    {0xb6, 0x2f},
    {0xb7, 0x28},
    {0xb8, 0x25},
    {0xb9, 0x22},
    {0xba, 0x21},
    {0xbb, 0x20},
    {0xbc, 0x1f},
    {0xbd, 0x1f},
    
    //AE_Adaptive Time option
    //{0xc0, 0x10},
    //{0xc1, 0x2b},
    //{0xc2, 0x2b},
    //{0xc3, 0x2b},
    //{0xc4, 0x08},
    
    {0xc8, 0x80},
    {0xc9, 0x40},
    
    /////// PAGE 22 START ///////
    {0x03, 0x22},
    {0x10, 0xfd},
    {0x11, 0x2e},
    {0x19, 0x01}, // Low On //
    {0x20, 0x30},
    {0x21, 0x80},
    {0x24, 0x01},
    //{0x25, 0x00}, //7f New Lock Cond & New light stable
    
    {0x30, 0x80},
    {0x31, 0x80},
    {0x38, 0x11},
    {0x39, 0x34},
    
    {0x40, 0xf4},
    {0x41, 0x55}, //44
    {0x42, 0x33}, //43
    
    {0x43, 0xf6},
    {0x44, 0x55}, //44
    {0x45, 0x44}, //33
    
    {0x46, 0x00},
    {0x50, 0xb2},
    {0x51, 0x81},
    {0x52, 0x98},
};    
const static struct reg_value Hi253_common_svgabase_4[] = {
    //{0x80, 0x3c}, //3e
    //{0x81, 0x20},
    //{0x82, 0x34},
    
    {0x83, 0x5e}, //5e
    {0x84, 0x0e}, //24
    {0x85, 0x5e}, //54 //56 //5a
    {0x86, 0x22}, //24 //22
    
    {0x87, 0x40},
    {0x88, 0x30},
    {0x89, 0x37}, //38
    {0x8a, 0x28}, //2a
    
    {0x8b, 0x40}, //47
    {0x8c, 0x33}, 
    {0x8d, 0x39}, 
    {0x8e, 0x30}, //2c
    
    {0x8f, 0x53}, //4e
    {0x90, 0x52}, //4d
    {0x91, 0x51}, //4c
    {0x92, 0x4e}, //4a
    {0x93, 0x4a}, //46
    {0x94, 0x45},
    {0x95, 0x3d},
    {0x96, 0x31},
    {0x97, 0x28},
    {0x98, 0x24},
    {0x99, 0x20},
    {0x9a, 0x20},
    
    {0x9b, 0x88},
    {0x9c, 0x88},
    {0x9d, 0x48},
    {0x9e, 0x38},
    {0x9f, 0x30},
    
    {0xa0, 0x60},
    {0xa1, 0x34},
    {0xa2, 0x6f},
    {0xa3, 0xff},
    
    {0xa4, 0x14}, //1500fps
    {0xa5, 0x2c}, // 700fps
    {0xa6, 0xcf},
    
    {0xad, 0x40},
    {0xae, 0x4a},
    
    {0xaf, 0x28},  // low temp Rgain
    {0xb0, 0x26},  // low temp Rgain
    
    {0xb1, 0x00}, //0x20 -> 0x00 0405 modify
    {0xb4, 0xea},
    {0xb8, 0xa0}, //a2: b-2, R+2  //b4 B-3, R+4 lowtemp
    {0xb9, 0x00},
    // PAGE 20
    {0x03, 0x20}, //page 20
    {0x10, 0x9c}, //ae off
    
    // PAGE 22
    {0x03, 0x22}, //page 22
    {0x10, 0xe9}, //awb off
    
    // PAGE 0
    {0x03, 0x00},
    {0x0e, 0x03}, //PLL On
    {0x0e, 0x77}, //PLL*4 
    
    {0x03, 0x00}, // Dummy 750us
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    {0x03, 0x00},
    
    {0x03, 0x00}, // Page 0
    {0x01, 0xf0}, // Sleep Off
    
    {0xff, 0xff}
};
static struct reg_value hi253_setting_15fps_UXGA_1600_1200[] = {
    {0x03, 0x00},//Page 00
    {0x10, 0x00},//Fullsize
    {0x11, 0x93},//Close fixed function//TODO
    /////////////////********Scaler Function Start********/////////////////
    {0x03, 0x18},
    {0x10, 0x00},//Disable Scaler Function
    {0x03, 0x00}, //Page 0
    {0x40, 0x01}, //Hblank 360
    {0x41, 0x68}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14}, 
    {0x0e, 0x03}, //PLL On
    {0x0e, 0x73}, //PLLx2

    {0x03, 0x20}, //Page 20
    {0x83, 0x04}, //EXP Normal 10.00 fps 
    {0x84, 0x93}, 
    {0x85, 0xe0}, 
    {0x86, 0x01}, //EXPMin 6000.00 fps
    {0x87, 0xf4}, 
    {0x88, 0x09}, //EXP Max 5.00 fps 
    {0x89, 0x27}, 
    {0x8a, 0xc0}, 
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x30}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0xa8}, 
    {0x9c, 0x17}, //EXP Limit 500.00 fps 
    {0x9d, 0x70}, 
    {0x9e, 0x01}, //EXP Unit 
    {0x9f, 0xf4}, 
    {0xa3, 0x00}, //Outdoor Int 
    {0xa4, 0x61}, 
    
    //AntiBand Unlock
    {0x03, 0x20}, //Page 20 
    {0x2b, 0x34}, 
    {0x30, 0x78}, 
    
    //BLC 
    {0x03, 0x00}, //PAGE 0
    {0x90, 0x14}, //BLC_TIME_TH_ON
    {0x91, 0x14}, //BLC_TIME_TH_OFF 
    {0x92, 0x78}, //BLC_AG_TH_ON
    {0x93, 0x70}, //BLC_AG_TH_OFF
    
    //DCDC 
    {0x03, 0x02}, //PAGE 2
    {0xd4, 0x14}, //DCDC_TIME_TH_ON
    {0xd5, 0x14}, //DCDC_TIME_TH_OFF 
    {0xd6, 0x78}, //DCDC_AG_TH_ON
    {0xd7, 0x70}, //DCDC_AG_TH_OFF

    {0xff, 0xff}
};

static struct reg_value hi253_setting_15fps_VGA_640_480[] = {
    {0x03, 0x00}, //Page 00
    {0x10, 0x11},//1/2 screen
    {0x11, 0x93}, 
 
    {0x0e, 0x03}, //PLL On
    {0x0e, 0x73}, //PLLx2

    {0x03, 0x00}, //Page 0
    {0x40, 0x01}, //Hblank 360
    {0x41, 0x68}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14}, 
    
    {0x03, 0x20}, //Page 20
    //{0x83, 0x03}, //EXP Normal 14.29 fps 
    //{0x84, 0x34}, 
    //{0x85, 0x50}, 
    {0x86, 0x01}, //EXPMin 6000.00 fps
    {0x87, 0xf4}, 
    {0x88, 0x05}, //EXP Max 8.33 fps 
    {0x89, 0x7e}, 
    {0x8a, 0x40}, 
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x30}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0xa8}, 
    {0x9c, 0x17}, //EXP Limit 500.00 fps 
    {0x9d, 0x70}, 
    {0x9e, 0x01}, //EXP Unit 
    {0x9f, 0xf4}, 
    {0xa3, 0x00}, //Outdoor Int 
    {0xa4, 0x61}, 
    
    //AntiBand Unlock
    {0x03, 0x20}, //Page 20 
    {0x2b, 0x34}, 
    {0x30, 0x78}, 
    
    //BLC 
    {0x03, 0x00}, //PAGE 0
    {0x90, 0x0c}, //BLC_TIME_TH_ON
    {0x91, 0x0c}, //BLC_TIME_TH_OFF 
    {0x92, 0x78}, //BLC_AG_TH_ON
    {0x93, 0x70}, //BLC_AG_TH_OFF
    
    //DCDC 
    {0x03, 0x02}, //PAGE 2
    {0xd4, 0x0c}, //DCDC_TIME_TH_ON
    {0xd5, 0x0c}, //DCDC_TIME_TH_OFF 
    {0xd6, 0x78}, //DCDC_AG_TH_ON
    {0xd7, 0x70}, //DCDC_AG_TH_OFF
    
    {0x03, 0x18},
    {0x12, 0x20},
    {0x10, 0x07},
    {0x11, 0x00},
    {0x20, 0x05},
    {0x21, 0x00},
    {0x22, 0x01},
    {0x23, 0xe0},
    {0x24, 0x00},
    {0x25, 0x00},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x05},
    {0x29, 0x00},
    {0x2a, 0x01},
    {0x2b, 0xe0},
    {0x2c, 0x0a},
    {0x2d, 0x00},
    {0x2e, 0x0a},
    {0x2f, 0x00},
    {0x30, 0x44},

    {0xff, 0xff}
};
static struct reg_value hi253_setting_15fps_QSVGA_320_240[] = {
    {0x03, 0x00}, //Page 00
    {0x10, 0x11},//1/2 screen
    {0x11, 0x93}, 
    {0x0e, 0x03}, //PLL On
    {0x0e, 0x73}, //PLLx2

    {0x03, 0x00}, //Page 0
    {0x40, 0x01}, //Hblank 360
    {0x41, 0x68}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14}, 

    {0x03, 0x20}, //Page 20
    //{0x83, 0x02}, //EXP Normal 16.67 fps 
    //{0x84, 0xbf}, 
    //{0x85, 0x20}, 
    {0x86, 0x01}, //EXPMin 6000.00 fps
    {0x87, 0xf4}, 
    //{0x88, 0x04}, //EXP Max 10.00 fps 
    {0x88, 0x08}, //EXP Max 10.00 fps 
    {0x89, 0x93}, 
    {0x8a, 0xe0}, 
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x30}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0xa8}, 
    {0x9c, 0x17}, //EXP Limit 500.00 fps 
    {0x9d, 0x70}, 
    {0x9e, 0x01}, //EXP Unit 
    {0x9f, 0xf4}, 
    {0xa3, 0x00}, //Outdoor Int 
    {0xa4, 0x61}, 
    
    //AntiBand Unlock
    {0x03, 0x20}, //Page 20 
    {0x2b, 0x34}, 
    {0x30, 0x78}, 
    
    //BLC 
    {0x03, 0x00}, //PAGE 0
    {0x90, 0x0a}, //BLC_TIME_TH_ON
    {0x91, 0x0a}, //BLC_TIME_TH_OFF 
    {0x92, 0x78}, //BLC_AG_TH_ON
    {0x93, 0x70}, //BLC_AG_TH_OFF
    
    //DCDC 
    {0x03, 0x02}, //PAGE 2
    {0xd4, 0x0a}, //DCDC_TIME_TH_ON
    {0xd5, 0x0a}, //DCDC_TIME_TH_OFF 
    {0xd6, 0x78}, //DCDC_AG_TH_ON
    {0xd7, 0x70}, //DCDC_AG_TH_OFF
    
    {0x03, 0x18},
    {0x12, 0x20},
    {0x10, 0x07},
    {0x11, 0x00},
    {0x20, 0x02},
    {0x21, 0x80},
    {0x22, 0x00},
    {0x23, 0xf0},
    {0x24, 0x00},
    {0x25, 0x00},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x02},
    {0x29, 0x80},
    {0x2a, 0x00},
    {0x2b, 0xf0},
    {0x2c, 0x14},
    {0x2d, 0x00},
    {0x2e, 0x14},
    {0x2f, 0x00},
    {0x30, 0x64},
    
    {0xff, 0xff}
};

static struct reg_value hi253_setting_30fps_UXGA_1600_1200[] = {
    {0x03, 0x00},//Page 00
    {0x10, 0x00},//1/2 screen
    {0x11, 0x93},//Close fixed function//TODO
    /////////////////********Scaler Function Start********/////////////////
    {0x03, 0x18},
    {0x10, 0x00},//Disable Scaler Function
    {0x03, 0x00}, //Page 0
    {0x40, 0x01}, //Hblank 360
    {0x41, 0x68}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14}, 

    {0x0e, 0x03}, //PLL On
    {0x0e, 0x73}, //PLLx2

    {0x03, 0x20}, //Page 20
    {0x83, 0x04}, //EXP Normal 10.00 fps 
    {0x84, 0x93}, 
    {0x85, 0xe0}, 
    {0x86, 0x01}, //EXPMin 6000.00 fps
    {0x87, 0xf4}, 
    {0x88, 0x09}, //EXP Max 5.00 fps 
    {0x89, 0x27}, 
    {0x8a, 0xc0}, 
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x30}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0xa8}, 
    {0x9c, 0x17}, //EXP Limit 500.00 fps 
    {0x9d, 0x70}, 
    {0x9e, 0x01}, //EXP Unit 
    {0x9f, 0xf4}, 
    {0xa3, 0x00}, //Outdoor Int 
    {0xa4, 0x61}, 
    
    //AntiBand Unlock
    {0x03, 0x20}, //Page 20 
    {0x2b, 0x34}, 
    {0x30, 0x78}, 
    
    //BLC 
    {0x03, 0x00}, //PAGE 0
    {0x90, 0x14}, //BLC_TIME_TH_ON
    {0x91, 0x14}, //BLC_TIME_TH_OFF 
    {0x92, 0x78}, //BLC_AG_TH_ON
    {0x93, 0x70}, //BLC_AG_TH_OFF
    
    //DCDC 
    {0x03, 0x02}, //PAGE 2
    {0xd4, 0x14}, //DCDC_TIME_TH_ON
    {0xd5, 0x14}, //DCDC_TIME_TH_OFF 
    {0xd6, 0x78}, //DCDC_AG_TH_ON
    {0xd7, 0x70}, //DCDC_AG_TH_OFF

    {0xff, 0xff}
};
static struct reg_value hi253_setting_30fps_VGA_640_480[] = { 
    {0x03, 0x00}, //Page 00
    {0x10, 0x11},//1/2 screen
    {0x11, 0x93},

    {0x40, 0x01}, //Hblank 408
    {0x41, 0x98}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14},

    {0x90, 0x0a}, //BLC_TIME_TH_ON
    {0x91, 0x0a}, //BLC_TIME_TH_OFF 
    {0x92, 0xd8}, //BLC_AG_TH_ON
    {0x93, 0xd0}, //BLC_AG_TH_OFF

    {0x03,0x02},
    {0xd4, 0x0c}, //DCDC_TIME_TH_ON//0a
    {0xd5, 0x0c}, //DCDC_TIME_TH_OFF //0a
    {0xd6, 0x78}, //DCDC_AG_TH_ON  98
    {0xd7, 0x70}, //DCDC_AG_TH_OFF  90

    {0x03, 0x18},
    {0x12, 0x20},
    {0x10, 0x07},
    {0x11, 0x00},
    {0x20, 0x05},
    {0x21, 0x00},
    {0x22, 0x01},
    {0x23, 0xe0},
    {0x24, 0x00},
    {0x25, 0x00},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x05},
    {0x29, 0x00},
    {0x2a, 0x01},
    {0x2b, 0xe0},
    {0x2c, 0x0a},
    {0x2d, 0x00},
    {0x2e, 0x0a},
    {0x2f, 0x00},
    {0x30, 0x44},
    
    {0x03, 0x20},
    {0x86, 0x02}, //EXPMin 5859.38 fps
    {0x87, 0x00}, 
    
    //{0x88, 0x05}, //EXP Max 10.00 fps 
    {0x88, 0x01}, //EXP Max 10.00 fps 
    {0x89, 0x7c}, 
    {0x8a, 0x00}, 
    
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x00}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0x00}, 
    
    {0x9c, 0x18}, //EXP Limit 488.28 fps 
    {0x9d, 0x00}, 
    {0x9e, 0x02}, //EXP Unit 
    {0x9f, 0x00}, 

    {0xff, 0xff}

};
static struct reg_value hi253_setting_30fps_QSVGA_320_240[] = {
	{0x03, 0x00}, //Page 00
    {0x10, 0x11},//1/2 screen
    {0x11, 0x93},

    {0x40, 0x01}, //Hblank 408
    {0x41, 0x98}, 
    {0x42, 0x00}, //Vblank 20
    {0x43, 0x14},

    {0x90, 0x0a}, //BLC_TIME_TH_ON
    {0x91, 0x0a}, //BLC_TIME_TH_OFF 
    {0x92, 0xd8}, //BLC_AG_TH_ON
    {0x93, 0xd0}, //BLC_AG_TH_OFF

	{0x0e, 0x03}, //PLL On
    {0x0e, 0x73}, //PLLx2

    {0x03,0x02},
    {0xd4, 0x0c}, //DCDC_TIME_TH_ON//0a
    {0xd5, 0x0c}, //DCDC_TIME_TH_OFF //0a
    {0xd6, 0x78}, //DCDC_AG_TH_ON  98
    {0xd7, 0x70}, //DCDC_AG_TH_OFF  90

    {0x03, 0x18},
    {0x12, 0x20},
    {0x10, 0x07},
    {0x11, 0x00},
    {0x20, 0x02},
    {0x21, 0x80},
    {0x22, 0x00},
    {0x23, 0xf0},
    {0x24, 0x00},
    {0x25, 0x00},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x02},
    {0x29, 0x80},
    {0x2a, 0x00},
    {0x2b, 0xf0},
    {0x2c, 0x14},
    {0x2d, 0x00},
    {0x2e, 0x14},
    {0x2f, 0x00},
    {0x30, 0x64},
    
    {0x03, 0x20},
    {0x86, 0x02}, //EXPMin 5859.38 fps
    {0x87, 0x00}, 
    //{0x88, 0x05}, //EXP Max 10.00 fps 
    {0x88, 0x01}, //EXP Max 10.00 fps 
    {0x89, 0x7c}, 
    {0x8a, 0x00}, 
    
    {0x8B, 0x75}, //EXP100 
    {0x8C, 0x00}, 
    {0x8D, 0x61}, //EXP120 
    {0x8E, 0x00}, 
    
    {0x9c, 0x18}, //EXP Limit 488.28 fps 
    {0x9d, 0x00}, 
    {0x9e, 0x02}, //EXP Unit 
    {0x9f, 0x00}, 
    {0xff, 0xff},
};

static struct hi253_mode_info hi253_mode_info_data[2][hi253_mode_MAX + 1] = {
    {
        {hi253_mode_UXGA_1600_1200,    1600,  1200,
            hi253_setting_15fps_UXGA_1600_1200,
            ARRAY_SIZE(hi253_setting_15fps_UXGA_1600_1200)},
        {hi253_mode_VGA_640_480,    640,  480,
            hi253_setting_15fps_VGA_640_480,
            ARRAY_SIZE(hi253_setting_15fps_VGA_640_480)},
        {hi253_mode_QSVGA_320_240,   320,  240,
            hi253_setting_15fps_QSVGA_320_240,
            ARRAY_SIZE(hi253_setting_15fps_QSVGA_320_240)},
    },
    {
        {hi253_mode_UXGA_1600_1200,    1600,  1200,
            hi253_setting_30fps_UXGA_1600_1200,
            ARRAY_SIZE(hi253_setting_30fps_UXGA_1600_1200)},
        {hi253_mode_VGA_640_480,    640,  480,
            hi253_setting_30fps_VGA_640_480,
            ARRAY_SIZE(hi253_setting_30fps_VGA_640_480)},
        {hi253_mode_QSVGA_320_240,   320,  240,
            hi253_setting_30fps_QSVGA_320_240,
            ARRAY_SIZE(hi253_setting_30fps_QSVGA_320_240)},
    },
};

static int sensor_try_fmt(struct v4l2_subdev *sd, 
             struct v4l2_mbus_framefmt *fmt)//linux-3.0
{
    pr_debug("hi253 try fmt (w:%d,h:%d) code=%d\n",fmt->width,fmt->height,fmt->code);

    if (fmt->width < hi253_mode_info_data[0][hi253_mode_MAX].width)
        fmt->width = hi253_mode_info_data[0][hi253_mode_MAX].width;
    if (fmt->height < hi253_mode_info_data[0][hi253_mode_MAX].height)
        fmt->height = hi253_mode_info_data[0][hi253_mode_MAX].height;

    fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;
    fmt->field = V4L2_FIELD_NONE;
    switch (fmt->code) {
        default:
            fmt->colorspace = V4L2_COLORSPACE_JPEG;
            break;
    }
    return 0;

}


static int hi253_dump_reg(void)
{
    int retval = 0;
    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 

	printk("page 0-------------\r\n");
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x00;//PAGE mode 0x00

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) 
	{
        pr_err("write reg error addr=0x%x\r\n", w_param_ptr->u8RegAddr);
        return -EINVAL;
    	}

    w_param_regs.u8RegAddr = 0x0e;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x00
	printk("0x0e=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x12;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x12=%x\r\n",w_param_regs.u8Val);


	printk("page 20-------------\r\n");
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x20;//PAGE mode 0x20

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) 
	{
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return -EINVAL;
    	}

    w_param_regs.u8RegAddr = 0x10;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x10=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x11;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x11=%x\r\n",w_param_regs.u8Val);

	printk("EXPINT-\r\n");
    w_param_regs.u8RegAddr = 0x80;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x80=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x81;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x81=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x82;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x82=%x\r\n",w_param_regs.u8Val);

	printk("EXPTIM-\r\n");
    w_param_regs.u8RegAddr = 0x83;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x83=%x\r\n",w_param_regs.u8Val);


    w_param_regs.u8RegAddr = 0x84;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x84=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x85;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x85=%x\r\n",w_param_regs.u8Val);

	printk("EXPMAX-\r\n");
    w_param_regs.u8RegAddr = 0x88;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x88=%x\r\n",w_param_regs.u8Val);


    w_param_regs.u8RegAddr = 0x89;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x89=%x\r\n",w_param_regs.u8Val);

    w_param_regs.u8RegAddr = 0x8a;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x20
	printk("0x8a=%x\r\n",w_param_regs.u8Val);

}

static int hi253_s_hflip(struct v4l2_subdev *s,
            struct v4l2_control *vc)
{

    int retval = 0;

	struct i2c_client *client = v4l2_get_subdevdata(s);
	struct hi253 *sensor = to_hi253(client);

    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 
    //struct sensor_data*sensor = s->priv;
    
    if(vc->value < 0 || vc->value > 1)
    {
        printk("out of range !\n");
        return -EINVAL;
    }
    
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x00;//PAGE mode 0x00
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return -EINVAL;
    }
    
    w_param_regs.u8RegAddr = 0x11;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x00
    
    switch(vc->value)
    {
        case 0:
            w_param_regs.u8Val |= 0x02;
            break;
        case 1:
            w_param_regs.u8Val &= 0xfd;
            break;
        default:
            return -EINVAL;
    }
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return -EINVAL;
    }
    
    sensor->hflip = vc->value;
    mdelay(100);


    return 0;
}

static int hi253_g_hflip(struct v4l2_subdev *s,
            struct v4l2_control *vc)
{
    int retval = 0;

	struct i2c_client *client = v4l2_get_subdevdata(s);
	struct hi253 *sensor = to_hi253(client);

    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 
    //struct sensor_data*sensor = s->priv;
    u8 temp = 0;
    
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x00;//PAGE mode 0x00
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return retval;
    }
    
    w_param_regs.u8RegAddr = 0x11;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x00
   
    temp = w_param_regs.u8Val & 0x02;

    switch(temp)
    {
        case 0:
            vc->value = 1;
            break;
        case 0x2:
            vc->value = 0;
            break;
        default:
            return -EINVAL;
    }
    sensor->hflip = vc->value;
    return 0;
}
static int hi253_s_vflip(struct v4l2_subdev *s,
            struct v4l2_control *vc)
{
    int retval = 0;
    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 
    //struct sensor_data*sensor = s->priv;
	struct i2c_client *client = v4l2_get_subdevdata(s);
	struct hi253 *sensor = to_hi253(client);
    
    if(vc->value < 0 || vc->value > 1)
    {
        printk("out of range !\n");
        return -EINVAL;
    }

    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x00;//PAGE mode 0x00
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
            return -EINVAL;
    }
    
    w_param_regs.u8RegAddr = 0x11;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x00
    
    switch(vc->value)
    {
        case 0:
            w_param_regs.u8Val |= 0x01;
            break;
        case 1:
            w_param_regs.u8Val &= 0xfe;
            break;
        default:
            return -EINVAL;
    }
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
            return -EINVAL;
    }
    
    mdelay(100);
    sensor->hflip = vc->value;
    return 0;
}

static int hi253_g_vflip(struct v4l2_subdev *s,
            struct v4l2_control *vc)
{
    int retval = 0;
    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 
    //struct sensor_data*sensor = s->priv;
	struct i2c_client *client = v4l2_get_subdevdata(s);
	struct hi253 *sensor = to_hi253(client);
    u8 temp = 0;
    
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x00;//PAGE mode 0x00
    
	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
            return -EINVAL;
    }
    
    w_param_regs.u8RegAddr = 0x11;
    w_param_regs.u8Val = hi253_read_reg(w_param_regs.u8RegAddr);//PAGE mode 0x00
   
    temp = w_param_regs.u8Val & 0x01;

    switch(temp)
    {
        case 0:
            vc->value = 1;
            break;
        case 0x2:
            vc->value = 0;
            break;
        default:
            return -EINVAL;
    }
    sensor->vflip = vc->value;
    return 0;
}



static s32 hi253_write_reg(u8 reg, u8 val)
{
    u8 au8Buf[2] = {0};

    au8Buf[0] = reg;
    au8Buf[1] = val;

    if (i2c_master_send(hi253_data.i2c_client, au8Buf, 2) < 0) {
        pr_err("%s: write reg error: reg=%x,val=%x\n",
                    __func__, reg, val);
        return -1;
    }

    return 0;
}

static int hi253_init_mode(enum hi253_frame_rate frame_rate,
            enum hi253_mode mode)
{
	printk("hi253_init_mode frame_rate=%d,mode=%d\n",frame_rate,mode);

    struct reg_value *pModeSetting = NULL;
    s32 i = 0;
    s32 iModeSettingArySize = 0;
    register u8 RegAddr = 0;
    register u8 Val = 0;
    int retval = 0;

    if (mode > hi253_mode_MAX || mode < hi253_mode_MIN) {
        pr_err("Wrong hi253 mode detected!\n");
        return -1;
    }

    pModeSetting = hi253_mode_info_data[frame_rate][mode].init_data_ptr;
    iModeSettingArySize =
        hi253_mode_info_data[frame_rate][mode].init_data_size;

    hi253_data.pix.width = hi253_mode_info_data[frame_rate][mode].width;
    hi253_data.pix.height = hi253_mode_info_data[frame_rate][mode].height;

    if (hi253_data.pix.width == 0 || hi253_data.pix.height == 0 ||
                pModeSetting == NULL || iModeSettingArySize == 0)
      return -EINVAL;

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;

        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0) {
            pr_err("write reg error addr=0x%x\n", RegAddr);
            goto err;
        }
    }

	hi253_dump_reg();

err:
    return retval;
}

static int hi253_change_mode(enum hi253_frame_rate frame_rate,
            enum hi253_mode new_mode,  enum hi253_mode orig_mode)
{
    int retval = 0;

    if (new_mode > hi253_mode_MAX || new_mode < hi253_mode_MIN) {
        pr_err("Wrong hi253 mode detected!\n");
        return -1;
    }

    retval = hi253_init_mode(frame_rate, new_mode);
    return retval;
}

#if 0
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
    if (s == NULL) {
        pr_err("   ERROR!! no slave device set!\n");
        return -1;
    }

    memset(p, 0, sizeof(*p));
    p->u.bt656.clock_curr = hi253_data.mclk;
    pr_debug("   clock_curr=mclk=%d\n", hi253_data.mclk);
    p->if_type = V4L2_IF_TYPE_BT656;
    p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
    p->u.bt656.clock_min = HI253_XCLK_MIN;
    p->u.bt656.clock_max = HI253_XCLK_MAX;
    p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

    return 0;
}


/*
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */

static int ioctl_g_ctrl(struct v4l2_int_device *s,
            struct v4l2_control *vc)
{
    int ret = 0;
#if 0
    switch (vc->id) {
        case V4L2_CID_BRIGHTNESS:
          vc->value = hi253_data.brightness;
          break;
        case V4L2_CID_HUE:
          vc->value = hi253_data.hue;
          break;
        case V4L2_CID_CONTRAST:
          vc->value = hi253_data.contrast;
          break;
        case V4L2_CID_SATURATION:
          vc->value = hi253_data.saturation;
          break;
        case V4L2_CID_RED_BALANCE:
          vc->value = hi253_data.red;
          break;
        case V4L2_CID_BLUE_BALANCE:
          vc->value = hi253_data.blue;
          break;
        case V4L2_CID_EXPOSURE:
          vc->value = hi253_data.ae_mode;
          break;
        case V4L2_CID_HFLIP:
          hi253_g_hflip(s,vc);         
          break;
        case V4L2_CID_VFLIP:
          hi253_g_vflip(s,vc);         
          break;
        default:
          ret = -EINVAL;
    }
#endif
    return ret;
}
/*
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s,
            struct v4l2_control *vc)
{
    int retval = 0;

    pr_debug("In hi253:ioctl_s_ctrl %d\n",
                vc->id);

    switch (vc->id) {
        case V4L2_CID_BRIGHTNESS:
          break;
        case V4L2_CID_CONTRAST:
          break;
        case V4L2_CID_SATURATION:
          break;
        case V4L2_CID_HUE:
          break;
        case V4L2_CID_AUTO_WHITE_BALANCE:
          break;
        case V4L2_CID_DO_WHITE_BALANCE:
          break;
        case V4L2_CID_RED_BALANCE:
          break;
        case V4L2_CID_BLUE_BALANCE:
          break;
        case V4L2_CID_GAMMA:
          break;
        case V4L2_CID_EXPOSURE:
          break;
        case V4L2_CID_AUTOGAIN:
          break;
        case V4L2_CID_GAIN:
          break;
        case V4L2_CID_HFLIP:
          retval = hi253_s_hflip(s,vc);
          break;
        case V4L2_CID_VFLIP:
          retval = hi253_s_vflip(s,vc);
          break;
        case V4L2_CID_MXC_ROT:
        case V4L2_CID_MXC_VF_ROT:
          if(vc->value == V4L2_MXC_ROTATE_VERT_FLIP)
          {
            retval = hi253_s_vflip(s,vc);
          }
          else if(vc->value == V4L2_MXC_ROTATE_VERT_FLIP)
          {
            retval = hi253_s_vflip(s,vc);
          }
          break;
        default:
          retval = -EPERM;
          break;
    }

    return retval;
}

/*
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
            struct v4l2_fmtdesc *fmt)
{
    if (fmt->index > hi253_mode_MAX)
      return -EINVAL;
    fmt->pixelformat = hi253_data.pix.pixelformat;
    return 0;
}


/*
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */

static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
        struct v4l2_format *f)
{
    int ifmt;
    struct v4l2_pix_format *pix = &f->fmt.pix;


    pr_debug("v4l2_pix_format(w:%d,h:%d)\n",pix->width,pix->height);

    if (pix->width > hi253_mode_info_data[0][hi253_mode_MAX].width)
        pix->width = hi253_mode_info_data[0][hi253_mode_MAX].width;
    if (pix->height > hi253_mode_info_data[0][hi253_mode_MAX].height)
        pix->height = hi253_mode_info_data[0][hi253_mode_MAX].height;

    pix->pixelformat = V4L2_PIX_FMT_YUYV;
    pix->field = V4L2_FIELD_NONE;
    pix->bytesperline = pix->width*2;
    pix->sizeimage = pix->bytesperline*pix->height;
    pix->priv = 0;
    switch (pix->pixelformat) {
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        default:
            pix->colorspace = V4L2_COLORSPACE_JPEG;
            break;
        case V4L2_PIX_FMT_SGRBG10:
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_RGB555:
        case V4L2_PIX_FMT_RGB555X:
            pix->colorspace = V4L2_COLORSPACE_SRGB;
            break;
    }
    return 0;
}



/*
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s,
            struct v4l2_format *f)
{
	printk("Hi253 g_fmt_cap\n");
    struct sensor_data *sensor = s->priv;
    f->fmt.pix = sensor->pix;

    return 0;
}

/*
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s,
            struct v4l2_streamparm *a)
{
    struct sensor_data *sensor = s->priv;
    struct v4l2_captureparm *cparm = &a->parm.capture;
    int ret = 0;

    switch (a->type) {
        /* This is the only case currently handled. */
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          memset(a, 0, sizeof(*a));
          a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          cparm->capability = sensor->streamcap.capability;
          cparm->timeperframe = sensor->streamcap.timeperframe;
          cparm->capturemode = sensor->streamcap.capturemode;
          ret = 0;
          break;

          /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        case V4L2_BUF_TYPE_VBI_CAPTURE:
        case V4L2_BUF_TYPE_VBI_OUTPUT:
        case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          ret = -EINVAL;
          break;

        default:
          pr_debug("   type is unknown - %d\n", a->type);
          ret = -EINVAL;
          break;
    }

    return ret;
}

/*
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s,
            struct v4l2_streamparm *a)
{
    struct sensor_data *sensor = s->priv;
    struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
    u32 tgt_fps;	/* target frames per secound */
    enum hi253_frame_rate frame_rate;
    int ret = 0;

    /* Make sure power on */
    if (camera_plat->pwdn)
      camera_plat->pwdn(0);

    switch (a->type) {
        /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          /* Check that the new frame rate is allowed. */
          if ((timeperframe->numerator == 0) ||
                      (timeperframe->denominator == 0)) {
              timeperframe->denominator = DEFAULT_FPS;
              timeperframe->numerator = 1;
          }

          tgt_fps = timeperframe->denominator /
              timeperframe->numerator;

          if (tgt_fps > MAX_FPS) {
              timeperframe->denominator = MAX_FPS;
              timeperframe->numerator = 1;
          } else if (tgt_fps < MIN_FPS) {
              timeperframe->denominator = MIN_FPS;
              timeperframe->numerator = 1;
          }

          /* Actual frame rate we use */
          tgt_fps = timeperframe->denominator /
              timeperframe->numerator;

          if (tgt_fps == 15)
            frame_rate = hi253_15_fps;
          else if (tgt_fps == 30)
            frame_rate = hi253_30_fps;
          else {
              pr_err(" The camera frame rate is not supported!\n");
              return -EINVAL;
          }
	printk("frame_rate = %d\n",frame_rate);

          ret =  hi253_change_mode(frame_rate, a->parm.capture.capturemode, sensor->streamcap.capturemode);
          sensor->streamcap.timeperframe = *timeperframe;
          sensor->streamcap.capturemode =
              (u32)a->parm.capture.capturemode;
          break;
          /* These are all the possible cases. */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        case V4L2_BUF_TYPE_VBI_CAPTURE:
        case V4L2_BUF_TYPE_VBI_OUTPUT:
        case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          pr_debug("   type is not " \
                      "V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
                      a->type);
          ret = -EINVAL;
          break;

        default:
          pr_debug("   type is unknown - %d\n", a->type);
          ret = -EINVAL;
          break;
    }

    return ret;
}
#endif

static int Optimus_get_AE_AWB(void)
{
    struct reg_value *pModeSetting = NULL;
    s32 i = 0;
    s32 iModeSettingArySize = 0;
    register u8 RegAddr = 0;
    register u8 Val = 0;
    int retval = 0;
 
    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 

    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x20;//PAGE mode 0x00

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return retval;
    }
    
    pModeSetting = Hi253_ae_special_1;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_1);
    
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr-3;
        Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }

    pModeSetting = Hi253_ae_special_2;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_2);
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }
// white balance
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x22;//PAGE mode 0x00

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return retval;
    }
    pModeSetting = Hi253_awb_special;
    iModeSettingArySize = ARRAY_SIZE(Hi253_awb_special);
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }

err:
    return retval;
}

static int Optimus_set_AE_AWB(void)
{
    struct reg_value *pModeSetting = NULL;
    s32 i = 0;
    s32 iModeSettingArySize = 0;
    register u8 RegAddr = 0;
    register u8 Val = 0;
    int retval = 0;
 
    struct reg_value w_param_regs;
    struct reg_value* w_param_ptr = &w_param_regs; 

    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x20;//PAGE mode 0x00

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return retval;
    }
    
    pModeSetting = Hi253_ae_special_1;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_1);
    
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr-3;
        Val = pModeSetting->u8Val;
        pModeSetting->u8Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }

    pModeSetting = Hi253_ae_special_2;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_2);
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        pModeSetting->u8Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }
// white balance
    w_param_regs.u8RegAddr = 0x03;
    w_param_regs.u8Val = 0x22;//PAGE mode 0x00

	retval = hi253_write_reg(w_param_ptr->u8RegAddr,w_param_ptr->u8Val);
	if (retval < 0) {
        pr_err("write reg error addr=0x%x\n", w_param_ptr->u8RegAddr);
        return retval;
    }
    pModeSetting = Hi253_awb_special;
    iModeSettingArySize = ARRAY_SIZE(Hi253_awb_special);
    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        pModeSetting->u8Val = hi253_read_reg(RegAddr);
        if (pModeSetting->u8Val<0)
          goto err;
    }

err:
    return retval;
}

#if 0
static int Optimus_preset(struct v4l2_int_device *s,
            struct v4l2_control *vc)
{
    int retval =0 ;
    if(vc->value == 1)
    {
        mdelay(3000);
        retval = Optimus_set_AE_AWB();
    }
    return retval;
}


/*
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
    struct sensor_data *sensor = s->priv;

    if (on && !sensor->on) {
        /* Make sure power on */
        if (camera_plat->pwdn)
          camera_plat->pwdn(0);
    } else if (!on && sensor->on) {
        if (camera_plat->pwdn)
        {
            Optimus_get_AE_AWB();  
            Optimus_set_AE_AWB();
          camera_plat->pwdn(1);
    
        }
    }
    sensor->on = on;

    return 0;
}


/*
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call Hi253_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
    return 0;
}

/**
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the dev. at slave detach.  The complement of ioctl_dev_init.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
    return 0;
}
#endif

static inline void hi253_power_down(int enable)
{
	gpio_set_value_cansleep(pwn_gpio, enable);

	msleep(120);
}

static inline void hi253_reset(void)
{
	gpio_set_value_cansleep(rst_gpio, 0);
	msleep(1);
	gpio_set_value_cansleep(rst_gpio, 1);
	msleep(5);
}

static int hi253_set_clk_rate(void)
{
	u32 tgt_xclk;	/* target xclk */
	int ret;

	/* mclk */
	tgt_xclk = hi253_data.mclk;
    	tgt_xclk = min(tgt_xclk, (u32)HI253_XCLK_MAX);
    	tgt_xclk = max(tgt_xclk, (u32)HI253_XCLK_MIN);
	hi253_data.mclk = tgt_xclk;

	printk("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	//pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	ret = clk_set_rate(hi253_data.sensor_clk, hi253_data.mclk);
	if (ret < 0)
		printk("set rate filed, rate=%d\n", hi253_data.mclk);
	return ret;
}

/**
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.  Returns 0 if
 * Hi253 device could be found, otherwise returns appropriate error.
 */
//static int ioctl_dev_init(struct v4l2_int_device *s)
//static int ioctl_dev_init()
static int dev_init(void)
{

    struct reg_value *pModeSetting = NULL;
    s32 i = 0;
    s32 iModeSettingArySize = 0;
    register u8 RegAddr = 0;
    register u8 Val = 0;
    int retval = 0;


    //struct sensor_data *sensor = hi253_data;
    u32 tgt_xclk;	/* target xclk */
    u32 tgt_fps;	/* target frames per secound */
    enum hi253_frame_rate frame_rate;

    hi253_data.on = true;

    /* mclk */
    tgt_xclk = hi253_data.mclk;
    tgt_xclk = min(tgt_xclk, (u32)HI253_XCLK_MAX);
    tgt_xclk = max(tgt_xclk, (u32)HI253_XCLK_MIN);
    hi253_data.mclk = tgt_xclk;

    //pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
    //set_mclk_rate(&hi253_data.mclk, hi253_data.csi);


    /* Default camera frame rate is set in probe */
    tgt_fps = hi253_data.streamcap.timeperframe.denominator /
        hi253_data.streamcap.timeperframe.numerator;

    if (tgt_fps == 15)
      frame_rate = hi253_15_fps;
    else if (tgt_fps == 30)
      frame_rate = hi253_30_fps;
    else
      return -EINVAL; /* Only support 15fps or 30fps now. */

    pr_debug("hi253 frame_rate to %d\n",frame_rate);

    pModeSetting = Hi253_common_svgabase_1;
    iModeSettingArySize = ARRAY_SIZE(Hi253_common_svgabase_1);

	//printk("base_1 size=%d",iModeSettingArySize);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);

	//u8 reg_readback = hi253_read_reg(RegAddr);
	//printk("addr=%d,set=%d,readback=%d",RegAddr,Val,reg_readback);

        if (retval < 0)
          goto err;
    }

    pModeSetting = Hi253_ae_special_1;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_1);

	printk("special_1 size=%d\n",iModeSettingArySize);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);

	//u8 reg_readback = hi253_read_reg(RegAddr);
	//printk("addr=%d,set=%d,readback=%d",RegAddr,Val,reg_readback);

        if (retval < 0)
          goto err;
    }

    pModeSetting = Hi253_common_svgabase_2;
    iModeSettingArySize = ARRAY_SIZE(Hi253_common_svgabase_2);

	printk("base_2 size=%d\n",iModeSettingArySize);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0)
          goto err;
    }


    pModeSetting = Hi253_ae_special_2;
    iModeSettingArySize = ARRAY_SIZE(Hi253_ae_special_2);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0)
          goto err;
    }


    pModeSetting = Hi253_common_svgabase_3;
    iModeSettingArySize = ARRAY_SIZE(Hi253_common_svgabase_3);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0)
          goto err;
    }

    pModeSetting = Hi253_awb_special;
    iModeSettingArySize = ARRAY_SIZE(Hi253_awb_special);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0)
          goto err;
    }

    pModeSetting = Hi253_common_svgabase_4;
    iModeSettingArySize = ARRAY_SIZE(Hi253_common_svgabase_4);

    for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
        RegAddr = pModeSetting->u8RegAddr;
        Val = pModeSetting->u8Val;
        retval = hi253_write_reg(RegAddr, Val);
        if (retval < 0)
          goto err;
    }

	return 0;
err:
	printk("hi253 set register failed\n");
    return retval;
}

/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
            struct v4l2_frmsizeenum *fsize)
{
    if (fsize->index > hi253_mode_MAX)
      return -EINVAL;

    fsize->pixel_format = hi253_data.pix.pixelformat;
    fsize->discrete.width =
        max(hi253_mode_info_data[0][fsize->index].width,
                    hi253_mode_info_data[1][fsize->index].width);
    fsize->discrete.height =
        max(hi253_mode_info_data[0][fsize->index].height,
                    hi253_mode_info_data[1][fsize->index].height);
    return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
    ((struct v4l2_dbg_chip_ident *)id)->match.type =
        V4L2_CHIP_MATCH_I2C_DRIVER;
    strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "hi253_camera");

    return 0;
}

#if 0
static struct v4l2_int_ioctl_desc Hi253_ioctl_desc[] = {
    {vidioc_int_dev_init_num, (v4l2_int_ioctl_func *)ioctl_dev_init},
    {vidioc_int_dev_exit_num, ioctl_dev_exit},
    {vidioc_int_s_power_num, (v4l2_int_ioctl_func *)ioctl_s_power},
    {vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *)ioctl_g_ifparm},
    /*	{vidioc_int_g_needs_reset_num,
        (v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
    /*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
    {vidioc_int_init_num, (v4l2_int_ioctl_func *)ioctl_init},
    {vidioc_int_enum_fmt_cap_num,
        (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},
    /*	{vidioc_int_try_fmt_cap_num,
        (v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */
    {vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_g_fmt_cap},
    /*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_s_fmt_cap}, */
    {vidioc_int_g_parm_num, (v4l2_int_ioctl_func *)ioctl_g_parm},
    {vidioc_int_s_parm_num, (v4l2_int_ioctl_func *)ioctl_s_parm},
    /*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
    {vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *)ioctl_g_ctrl},
    {vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *)ioctl_s_ctrl},
    {vidioc_int_enum_framesizes_num,
        (v4l2_int_ioctl_func *)ioctl_enum_framesizes},
    {vidioc_int_g_chip_ident_num,
        (v4l2_int_ioctl_func *)ioctl_g_chip_ident},
};
#endif


/*
static struct v4l2_int_slave Hi253_slave = {
    .ioctls		= Hi253_ioctl_desc,
    .num_ioctls	= ARRAY_SIZE(Hi253_ioctl_desc),
};

static struct v4l2_int_device Hi253_int_device = {
    .module	= THIS_MODULE,
    .name	= HI253_I2C_NAME,
    .type	= v4l2_int_type_slave,
    .u	= {
        .slave = &Hi253_slave,
    },
};
*/

static void mx6s_sg_aio_csi0_io_init(void)
{
	/* camera reset */
	printk("sensor io_init\n");
#if 0
    int ret = 0;
    ret = gpio_request(MX6S_OPTIMUS_AIO_NPGATE_CAMERA, "camera_npgate");
    if (ret) {
        pr_err("failed to get GPIO MX6S_OPTIMUS_AIO_NPGATE_CAMERA: %d\n",ret);
        return;
    }
    gpio_direction_output(MX6S_OPTIMUS_AIO_NPGATE_CAMERA, 0);
    gpio_free(MX6S_OPTIMUS_AIO_NPGATE_CAMERA);
    udelay(50);
    if(is_protov2 == 0)
    {
        ret = gpio_request(MX6S_OPTIMUS_AIO_CAM_1_8V_EN, "cam_1_8v_en");
        if (ret) {
            pr_err("failed to get GPIO MX6S_OPTIMUS_AIO_CAM_1_8V_EN: %d\n",ret);
            return;
        }
        gpio_direction_output(MX6S_OPTIMUS_AIO_CAM_1_8V_EN, 1);
        gpio_free(MX6S_OPTIMUS_AIO_CAM_1_8V_EN);
    }
    else if(is_protov2 == 1)
    {
        ret = gpio_request(MX6S_OPTIMUS_AIO_CAM_2_8V_EN, "cam_2_8v_en");
        if (ret) {
            pr_err("failed to get GPIO MX6S_OPTIMUS_AIO_CAM_2_8V_EN: %d\n",ret);
            return;
        }
        gpio_direction_output(MX6S_OPTIMUS_AIO_CAM_2_8V_EN, 1);
        gpio_free(MX6S_OPTIMUS_AIO_CAM_2_8V_EN);
    }
    udelay(50);
    ret = gpio_request(MX6S_OPTIMUS_AIO_CSI0_PWDN, "csi0_pwdn");
    if (ret) {
        pr_err("failed to get GPIO MX6S_OPTIMUS_AIO_CSI0_PWDN: %d\n",ret);
        return;
    }
    gpio_direction_output(MX6S_OPTIMUS_AIO_CSI0_PWDN, 0);
    gpio_free(MX6S_OPTIMUS_AIO_CSI0_PWDN);
    ret = gpio_request(MX6S_OPTIMUS_AIO_CSI0_RST, "csi0_rst");
    if (ret) {
        pr_err("failed to get GPIO MX6S_OPTIMUS_AIO_CSI0_RST: %d\n",ret);
        return;
    }
    if(is_protov2==0)
    {
        gpio_direction_output(MX6S_OPTIMUS_AIO_CSI0_RST, 1);
        udelay(50);
        gpio_direction_output(MX6S_OPTIMUS_AIO_CSI0_RST, 0);
        gpio_free(MX6S_OPTIMUS_AIO_CSI0_RST);
    }
    else if(is_protov2==1)
    {
        gpio_direction_output(MX6S_OPTIMUS_AIO_CSI0_RST, 0);
        udelay(50);
        gpio_direction_output(MX6S_OPTIMUS_AIO_CSI0_RST, 1);
        gpio_free(MX6S_OPTIMUS_AIO_CSI0_RST);
    }
    udelay(50);

    mxc_iomux_set_gpr_register(13, 0, 3, 4);
#endif

}

static const struct v4l2_subdev_pad_ops sensor_subdev_pad_ops = {
	.enum_frame_size       = hi253_enum_framesizes,
	.enum_frame_interval   = hi253_enum_frameintervals,
    .set_fmt = sensor_set_fmt,
    .get_fmt = sensor_get_fmt,
    .enum_mbus_code = sensor_enum_mbus_code,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	//.g_chip_ident = sensor_g_chip_ident,
	//.g_ctrl = sensor_g_ctrl,
	//.s_ctrl = sensor_s_ctrl,
	//.queryctrl = sensor_queryctrl,
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	//.enum_mbus_fmt = sensor_enum_fmt,//linux-3.0
	//.try_mbus_fmt = sensor_try_fmt,//linux-3.0
	//.s_mbus_fmt = sensor_s_fmt,//linux-3.0
	//.g_mbus_fmt = sensor_g_fmt,
	.s_parm = sensor_s_parm,//linux-3.0
	.g_parm = sensor_g_parm,//linux-3.0
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad	= &sensor_subdev_pad_ops,
};

/*
 * Hi253_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int
Hi253_probe1(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	int retval;
	u8 chip_id_high, chip_id_low;

	/* hi253 pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}
	retval = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"hi253_pwdn");
	if (retval < 0)
		return retval;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_err(dev, "no sensor reset pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_LOW,
					"hi253_reset");
	if (retval < 0)
		return retval;

	/* request cam18v-en gpio pin */
	cam18_gpio = of_get_named_gpio(dev->of_node, "cam18v-en-gpios", 0);
	if (!gpio_is_valid(cam18_gpio)) {
		dev_err(dev, "no cam18-en-gpio pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, cam18_gpio, GPIOF_OUT_INIT_HIGH,
					"cam18_en");

	/*retval = devm_gpio_request_one(dev, 466, GPIOF_OUT_INIT_HIGH,
					"cam18_en");
	printk("retval-111111111=%d\r\n", retval);*/
	if (retval < 0)
		return retval;

	/* request cam28v-en gpio pin */
	cam28_gpio = of_get_named_gpio(dev->of_node, "cam28v-en-gpios", 0);
	if (!gpio_is_valid(cam28_gpio)) {
		dev_err(dev, "no cam28-en pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, cam28_gpio, GPIOF_OUT_INIT_HIGH,
					"cam28_en");
	/*retval = devm_gpio_request_one(dev, 467, GPIOF_OUT_INIT_HIGH,
					"cam28_en");*/
	if (retval < 0)
		return retval;


	/* Set initial values for the sensor struct. */
    	memset(&hi253_data, 0, sizeof(hi253_data));
	hi253_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(hi253_data.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(hi253_data.sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&hi253_data.mclk);
	if (retval) {
		dev_err(dev, "mclk frequency is invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(hi253_data.mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(hi253_data.csi));
	if (retval) {
		dev_err(dev, "csi_id invalid\n");
		return retval;
	}
	
	/////////////////////////////////////

    printk("hi253_proble\n");

    if (i2c_get_clientdata(client))
      return -EBUSY;

    printk("hi253_probea\n");

    /* Set initial values for the sensor struct. */
    hi253_data.io_init = mx6s_sg_aio_csi0_io_init; //plat_data->io_init;
    //hi253_data.v4l2_int_device = &Hi253_int_device;

    printk("hi253_probe1\n");

    hi253_data.i2c_client = client;
    hi253_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    hi253_data.pix.width = 800;
    hi253_data.pix.height = 600;
    hi253_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
        V4L2_CAP_TIMEPERFRAME;
    hi253_data.streamcap.capturemode = 1;
    hi253_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
    hi253_data.streamcap.timeperframe.numerator = 1;

    printk("hi253_probe2\n");

    //if (plat_data->io_init)
    //  plat_data->io_init();

    //camera_plat = plat_data;

    printk("hi253_probe3\n");

    //Hi253_int_device.priv = &hi253_data;

    //retval = v4l2_int_device_register(&Hi253_int_device);

	//v4l2_i2c_subdev_init(sd, client, &sensor_ops);

    printk("hi253_probe4\n");

    if(!retval){
        pr_err("hi253 probe OK\n");
    }else{
        pr_err("hi253 probe Failed\n");
    }
    return retval;
}


static int
Hi253_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	int retval;
	u8 chip_id_high, chip_id_low;

	/* hi253 pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request cam28v-en gpio pin */
	cam28_gpio = of_get_named_gpio(dev->of_node, "cam28v-en-gpios", 0);
	if (!gpio_is_valid(cam28_gpio)) {
		dev_err(dev, "no cam28-en pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, cam28_gpio, GPIOF_OUT_INIT_HIGH,
					"cam28_en");
	if (retval < 0)
		return retval;
	/*retval = devm_gpio_request_one(dev, 467, GPIOF_OUT_INIT_HIGH,
					"cam28_en");*/


	/* request cam18v-en gpio pin */
	cam18_gpio = of_get_named_gpio(dev->of_node, "cam18v-en-gpios", 0);
	if (!gpio_is_valid(cam18_gpio)) {
		dev_err(dev, "no cam18-en-gpio pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, cam18_gpio, GPIOF_OUT_INIT_HIGH,
					"cam18_en");
	if (retval < 0)
		return retval;
	/*retval = devm_gpio_request_one(dev, 466, GPIOF_OUT_INIT_HIGH,
					"cam18_en");*/

	/* Set initial values for the sensor struct. */
	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}
	retval = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"hi253_pwdn");
	if (retval < 0)
		return retval;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_err(dev, "no sensor reset pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_LOW,
					"hi253_reset");
	if (retval < 0)
		return retval;

	/* Set initial values for the sensor struct. */
    	memset(&hi253_data, 0, sizeof(hi253_data));
	hi253_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(hi253_data.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(hi253_data.sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&hi253_data.mclk);
	if (retval) {
		dev_err(dev, "mclk frequency is invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(hi253_data.mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(hi253_data.csi));
	if (retval) {
		dev_err(dev, "csi_id invalid\n");
		return retval;
	}
	
	/////////////////////////////////////

    printk("hi253_proble\n");

    if (i2c_get_clientdata(client))
      return -EBUSY;

    printk("hi253_probea\n");

    /* Set initial values for the sensor struct. */
    hi253_data.io_init = mx6s_sg_aio_csi0_io_init; //plat_data->io_init;
//    hi253_data.v4l2_int_device = &Hi253_int_device;

    printk("hi253_probe1\n");

	hi253_power_down(0);
	hi253_set_clk_rate();
	clk_prepare_enable(hi253_data.sensor_clk);

	hi253_reset();


    hi253_data.i2c_client = client;
    hi253_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    hi253_data.pix.width = 800;
    hi253_data.pix.height = 600;
    hi253_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
        V4L2_CAP_TIMEPERFRAME;
    hi253_data.streamcap.capturemode = 1;
    hi253_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
    hi253_data.streamcap.timeperframe.numerator = 1;

    printk("hi253_probe2\n");

	dev_init();
    //if (plat_data->io_init)
    //  plat_data->io_init();

    //camera_plat = plat_data;

    printk("hi253_probe3\n");

    //Hi253_int_device.priv = &hi253_data;
    //retval = v4l2_int_device_register(&Hi253_int_device);

////////
	//hi253_reset();
	retval = hi253_read_reg(0x04);
	printk("sensor id=%d\n",retval);

	v4l2_i2c_subdev_init(&hi253_data.subdev, client, &sensor_ops);
	retval = v4l2_async_register_subdev(&hi253_data.subdev);
	if (retval < 0)
		dev_err(&client->dev,
					"%s--Async register failed, ret=%d\n", __func__, retval);
///////

    printk("hi253_probe4\n");

    if(!retval){
        pr_err("hi253 probe OK\n");
    }else{
        pr_err("hi253 probe Failed\n");
    }
    return retval;
}



static int Hi253_remove(struct i2c_client *client)
{
    //v4l2_int_device_unregister(&Hi253_int_device);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	v4l2_async_unregister_subdev(sd);
	clk_unprepare(hi253_data.sensor_clk);
	hi253_power_down(1);
    return 0;
}

static const struct i2c_device_id Hi253_id[] = {
    { HI253_I2C_NAME, 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, Hi253_id);

static struct i2c_driver hi253_i2c_driver = {
    .driver = {
        .name	= HI253_I2C_NAME,
        .owner = THIS_MODULE,
    },
    .probe	= Hi253_probe,
    .remove	= Hi253_remove,
    .id_table = Hi253_id,
};
#include <linux/delay.h>
static __init int hi253_init(void)
{
    u8 err;

    printk("hi253_init");
///
/*	gpio_request(466, NULL);
	gpio_direction_output(466, 0);
	gpio_free(466);
	udelay(10);
	gpio_request(467, NULL);
	gpio_direction_output(467, 0);
	gpio_free(467);
	udelay(10);*/
///
    err = i2c_add_driver(&hi253_i2c_driver);
    if (err != 0)
      pr_err("%s:driver registration failed, error=%d \n",
                  __func__, err);

    return err;
}
static void __exit hi253_clean(void)
{
    i2c_del_driver(&hi253_i2c_driver);
}
module_init(hi253_init);
module_exit(hi253_clean);


MODULE_DESCRIPTION("HI253 Camera Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("CSI");
