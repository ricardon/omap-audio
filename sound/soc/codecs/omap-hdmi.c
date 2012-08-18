/*
 * ALSA SoC codec driver for HDMI audio on OMAP processors.
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Ricardo Neri <ricardo.neri@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <linux/module.h>
#include <sound/soc.h>
#include <video/omapdss.h>
#include <linux/gpio.h>

#define DRV_NAME "hdmi-audio-codec"

struct hdmi_priv {
	struct omap_dss_device *dssdev;
	struct notifier_block events_notifier;
	struct snd_soc_jack jack;
};

static struct snd_soc_dai_driver omap_hdmi_codec_dai = {
	.name = "omap-hdmi-hifi",
	.playback = {
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE,
	},
};

int static omap_hdmi_dai_notifier_call(struct notifier_block *nb,
				       unsigned long v, void *ptr)
{
	struct omap_dss_audio *dss_audio = (struct omap_dss_audio *)ptr;
	struct snd_pcm_substream *substream;

	 /*
	  * Audio has not been configured yet. Then, we are sure we are not
	  * playing.
	  */
	if (!dss_audio)
		return NOTIFY_DONE;

	substream = (struct snd_pcm_substream *)dss_audio->private_data;

	switch (v) {
	case OMAP_DSS_DISPLAY_DISABLED:
		printk(KERN_ERR "DISABLED");
		if (substream) /* stop only is the substream is valid */
			snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		gpio_set_value(50, 0);
		break;
	case OMAP_DSS_DISPLAY_ACTIVE:
		printk(KERN_ERR "ACTIVE");
		gpio_set_value(50, 1);
		break;
	case OMAP_DSS_DISPLAY_SUSPENDED:
		printk(KERN_ERR "sSUSPENDED");
		if (substream) /* stop only is the substream is valid */
			snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		gpio_set_value(50, 0);
		break;
	default:
		printk(KERN_ERR "sUNKNOWN");
		gpio_set_value(50, 0);
	}

	return NOTIFY_OK;
}

int static omap_hdmi_probe(struct snd_soc_codec *codec)
{
	struct hdmi_priv *hdmi_data;
	bool hdmi_dev_found = false;
	struct platform_device *pdev = container_of(codec->dev,
						   struct platform_device, dev);
	int ret;
	/*
	 * TODO: We assume that there is only one DSS HDMI device. Future
	 * OMAP implementations may support more than one HDMI devices and
	 * we should provided separate audio support for all of them.
	 */
	/* Find an HDMI device. */

	hdmi_data = devm_kzalloc(&pdev->dev, sizeof(*hdmi_data), GFP_KERNEL);

	for_each_dss_dev(hdmi_data->dssdev) {
		omap_dss_get_device(hdmi_data->dssdev);

		if (!hdmi_data->dssdev->driver) {
			omap_dss_put_device(hdmi_data->dssdev);
			continue;
		}

		if (hdmi_data->dssdev->type == OMAP_DISPLAY_TYPE_HDMI) {
			hdmi_dev_found = true;
			break;
		}
	}

	hdmi_data->events_notifier.notifier_call = omap_hdmi_dai_notifier_call;
	ret = omap_dss_register_notifier(hdmi_data->dssdev->driver,
					 &hdmi_data->events_notifier);

	if (ret) {
		dev_err(&pdev->dev, "could not register event notifier\n");
		omap_dss_put_device(hdmi_data->dssdev);
		return -EPERM;
	}

	gpio_request(50, NULL);
	gpio_direction_output(50, 1);

	dev_set_drvdata(&pdev->dev, hdmi_data);

	return 0;
}

static struct snd_soc_codec_driver omap_hdmi_codec = {
	.probe = omap_hdmi_probe,
};

static __devinit int omap_hdmi_codec_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &omap_hdmi_codec,
			&omap_hdmi_codec_dai, 1);
}

static __devexit int omap_hdmi_codec_remove(struct platform_device *pdev)
{
	struct hdmi_priv *hdmi_data = dev_get_drvdata(&pdev->dev);

	gpio_free(50);
	snd_soc_unregister_codec(&pdev->dev);

	omap_dss_unregister_notifier(hdmi_data->dssdev->driver,
				     &hdmi_data->events_notifier);

	omap_dss_put_device(hdmi_data->dssdev);

	return 0;
}

static struct platform_driver omap_hdmi_codec_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},

	.probe		= omap_hdmi_codec_probe,
	.remove		= __devexit_p(omap_hdmi_codec_remove),
};

module_platform_driver(omap_hdmi_codec_driver);

MODULE_AUTHOR("Ricardo Neri <ricardo.neri@ti.com>");
MODULE_DESCRIPTION("ASoC OMAP HDMI codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
