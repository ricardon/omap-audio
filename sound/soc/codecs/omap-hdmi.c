/*
 * ALSA SoC HMDI codec driver for OMAP
 *
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <plat/omap_hwmod.h>
#include <video/omapdss.h>

#include "dss_features.h"
#include "dss.h"
#include "ti_hdmi_4xxx_ip.h"
#include "ti_hdmi.h"

#define DRV_NAME "hdmi-audio-codec"

#define HDMI_CORE_SYS		0x400
#define HDMI_CORE_AV		0x900

/* codec private data */
struct hdmi_priv {
	struct hdmi_audio_format audio_fmt;
	struct hdmi_audio_dma audio_dma;
	struct hdmi_core_audio_config audio_core_cfg;
	struct hdmi_core_infoframe_audio aud_if_cfg;
	struct hdmi_ip_data ip_data;
};

static int hdmi_compute_acr_params(u32 sample_freq, u32 pclk,
	u32 *n, u32 *cts)
{
	/*
	 * See deep color definition in
	 * HDMI 1.4 specification section 6.5.2
	 * TODO: add deep color support when available from DSS. For now
	 * define as no deep color.
	 */
	u32 deep_color = 100;

	if (n == NULL || cts == NULL)
		return -EINVAL;
	/*
	 * Color mode configuration is needed
	 * to calculate the TMDS clock based on the pixel clock.
	 */

	switch (sample_freq) {
	case 32000:
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			*n = 8192;
		else
			*n = 4096;
		break;
	case 44100:
		*n = 6272;
		break;
	case 48000:
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			*n = 8192;
		else
			*n = 6144;
		break;
	default:
		*n = 0;
		return -EINVAL;
	}

	/* Calculate CTS. See HDMI 1.3a or 1.4a specifications */
	*cts = pclk * (*n / 128) * deep_color / (sample_freq / 10);

	return 0;
}

static int hdmi_audio_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct platform_device *pdev = to_platform_device(codec->dev);
	struct hdmi_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct hdmi_audio_format *audio_format = &priv->audio_fmt;
	struct hdmi_audio_dma *audio_dma = &priv->audio_dma;
	struct hdmi_core_audio_config *core_cfg = &priv->audio_core_cfg;
	struct hdmi_core_infoframe_audio *aud_if_cfg = &priv->aud_if_cfg;
	struct omap_overlay_manager *mgr = NULL;
	enum hdmi_core_audio_sample_freq sample_freq;
	int err, n, cts, i;
	u32 pclk;

	/* Obtain pixel clock from DSS data */
	for (i = 0; i < omap_dss_get_num_overlay_managers(); i++) {
		mgr = omap_dss_get_overlay_manager(i);
		if (mgr && mgr->device
			&& mgr->device->type == OMAP_DISPLAY_TYPE_HDMI)
			break;
	}

	if (i == omap_dss_get_num_overlay_managers()) {
		dev_err(&pdev->dev, "HDMI display device not found!\n");
		return -ENODEV;
	}

	pclk = mgr->device->panel.timings.pixel_clock;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		core_cfg->i2s_cfg.word_max_length =
			HDMI_AUDIO_I2S_MAX_WORD_20BITS;
		core_cfg->i2s_cfg.word_length =
			HDMI_AUDIO_I2S_CHST_WORD_16_BITS;
		core_cfg->i2s_cfg.in_length_bits =
			HDMI_AUDIO_I2S_INPUT_LENGTH_16;
		core_cfg->i2s_cfg.justification = HDMI_AUDIO_JUSTIFY_LEFT;
		audio_format->samples_per_word = HDMI_AUDIO_ONEWORD_TWOSAMPLES;
		audio_format->sample_size = HDMI_AUDIO_SAMPLE_16BITS;
		audio_format->justification = HDMI_AUDIO_JUSTIFY_LEFT;
		audio_dma->transfer_size = 0x10;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		core_cfg->i2s_cfg.word_max_length =
			HDMI_AUDIO_I2S_MAX_WORD_24BITS;
		core_cfg->i2s_cfg.word_length =
			HDMI_AUDIO_I2S_CHST_WORD_24_BITS;
		core_cfg->i2s_cfg.in_length_bits =
			HDMI_AUDIO_I2S_INPUT_LENGTH_24;
		audio_format->samples_per_word = HDMI_AUDIO_ONEWORD_ONESAMPLE;
		audio_format->sample_size = HDMI_AUDIO_SAMPLE_24BITS;
		audio_format->justification = HDMI_AUDIO_JUSTIFY_RIGHT;
		core_cfg->i2s_cfg.justification = HDMI_AUDIO_JUSTIFY_RIGHT;
		audio_dma->transfer_size = 0x20;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 32000:
		sample_freq = HDMI_AUDIO_FS_32000;
		break;
	case 44100:
		sample_freq = HDMI_AUDIO_FS_44100;
		break;
	case 48000:
		sample_freq = HDMI_AUDIO_FS_48000;
		break;
	default:
		return -EINVAL;
	}

	err = hdmi_compute_acr_params(params_rate(params), pclk,
		&n, &cts);

	if (err < 0)
		return err;

	/* Audio wrapper config */
	audio_format->stereo_channels = HDMI_AUDIO_STEREO_ONECHANNEL;
	audio_format->active_chnnls_msk = 0x03;
	audio_format->type = HDMI_AUDIO_TYPE_LPCM;
	audio_format->sample_order = HDMI_AUDIO_SAMPLE_LEFT_FIRST;
	/* Disable start/stop signals of IEC 60958 blocks */
	audio_format->en_sig_blk_strt_end = HDMI_AUDIO_BLOCK_SIG_STARTEND_OFF;

	audio_dma->block_size = 0xC0;
	audio_dma->mode = HDMI_AUDIO_TRANSF_DMA;
	audio_dma->fifo_threshold = 0x20; /* in number of samples */

	priv->ip_data.ops->audio_dma_cfg(&priv->ip_data, audio_dma);
	priv->ip_data.ops->audio_fmt_cfg(&priv->ip_data, audio_format);

	/*
	 * I2S config
	 */
	core_cfg->i2s_cfg.en_high_bitrate_aud = false;
	/* Only used with high bitrate audio */
	core_cfg->i2s_cfg.cbit_order = false;
	/* Serial data and word select should change on sck rising edge */
	core_cfg->i2s_cfg.sck_edge_mode = HDMI_AUDIO_I2S_SCK_EDGE_RISING;
	core_cfg->i2s_cfg.vbit = HDMI_AUDIO_I2S_VBIT_FOR_PCM;
	/* Set I2S word select polarity */
	core_cfg->i2s_cfg.ws_polarity = HDMI_AUDIO_I2S_WS_POLARITY_LOW_IS_LEFT;
	core_cfg->i2s_cfg.direction = HDMI_AUDIO_I2S_MSB_SHIFTED_FIRST;
	/* Set serial data to word select shift. See Phillips spec. */
	core_cfg->i2s_cfg.shift = HDMI_AUDIO_I2S_FIRST_BIT_SHIFT;
	/* Enable one of the four available serial data channels */
	core_cfg->i2s_cfg.active_sds = HDMI_AUDIO_I2S_SD0_EN;

	/* Core audio config */
	core_cfg->freq_sample = sample_freq;
	core_cfg->n = n;
	core_cfg->cts = cts;
	/* TODO: add MCLK configuration for 4430 ES2.3/4460/4470 */
	if (dss_has_feature(FEAT_HDMI_CTS_SWMODE)) {
		core_cfg->aud_par_busclk = 0;
		core_cfg->cts_mode = HDMI_AUDIO_CTS_MODE_SW;
	} else {
		core_cfg->aud_par_busclk = (((128 * 31) - 1) << 8);
		core_cfg->cts_mode = HDMI_AUDIO_CTS_MODE_HW;
		core_cfg->mclk_mode = HDMI_AUDIO_MCLK_128FS;
	}
	core_cfg->layout = HDMI_AUDIO_LAYOUT_2CH;
	core_cfg->en_spdif = false;
	/* Use sample frequency from channel status word */
	core_cfg->fs_override = true;
	/* Enable ACR packets */
	core_cfg->en_acr_pkt = true;
	/* Disable direct streaming digital audio */
	core_cfg->en_dsd_audio = false;
	/* Use parallel audio interface */
	core_cfg->en_parallel_aud_input = true;

	priv->ip_data.ops->audio_core_cfg(&priv->ip_data, core_cfg);

	/*
	 * Configure packet
	 * info frame audio see doc CEA861-D page 74
	 */
	aud_if_cfg->db1_coding_type = HDMI_INFOFRAME_AUDIO_DB1CT_FROM_STREAM;
	aud_if_cfg->db1_channel_count = params_channels(params);
	aud_if_cfg->db2_sample_freq = HDMI_INFOFRAME_AUDIO_DB2SF_FROM_STREAM;
	aud_if_cfg->db2_sample_size = HDMI_INFOFRAME_AUDIO_DB2SS_FROM_STREAM;
	aud_if_cfg->db4_channel_alloc = 0;
	aud_if_cfg->db5_downmix_inh = false;
	aud_if_cfg->db5_lsv = 0;

	priv->ip_data.ops->audio_if_cfg(&priv->ip_data, aud_if_cfg);
	return 0;
}

static int hdmi_audio_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct platform_device *pdev = to_platform_device(codec->dev);
	struct hdmi_priv *priv = snd_soc_codec_get_drvdata(codec);
	int err = 0;

	if (!priv && !(priv->ip_data.ops->audio_enable)) {
		dev_err(&pdev->dev, "cannot enable/disable audio\n");
		return -ENODEV;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		priv->ip_data.ops->audio_enable(&priv->ip_data, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		priv->ip_data.ops->audio_enable(&priv->ip_data, false);
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

static int hdmi_audio_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	if (omapdss_hdmi_get_hdmi_mode() != HDMI_HDMI) {
		dev_err(dai->dev,
			"current video settings do not support audio\n");
		return -EIO;
	}
	return 0;
}
static int hdmi_probe(struct snd_soc_codec *codec)
{
	struct hdmi_priv *hdmi = snd_soc_codec_get_drvdata(codec);
	dss_init_hdmi_ip_ops(&hdmi->ip_data);
	if (!hdmi->ip_data.ops) {
		dev_err(codec->dev, "undefined HDMI ops\n");
		return -ENODEV;
	}

	hdmi->ip_data.core_sys_offset = HDMI_CORE_SYS;
	hdmi->ip_data.core_av_offset = HDMI_CORE_AV;

	return 0;
}

static struct snd_soc_codec_driver hdmi_audio_codec_drv = {
	.probe = hdmi_probe,
};

static struct snd_soc_dai_ops hdmi_audio_codec_ops = {
	.hw_params = hdmi_audio_hw_params,
	.trigger = hdmi_audio_trigger,
	.startup = hdmi_audio_startup,
};

static struct snd_soc_dai_driver hdmi_codec_dai_drv = {
	.name = "omap-hdmi-hifi",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &hdmi_audio_codec_ops,
};

static __devinit int hdmi_codec_probe(struct platform_device *pdev)
{
	struct hdmi_priv *hdmi;
	struct resource *res;
	int r;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (hdmi == NULL)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, hdmi);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev, "cannot obtain IORESOURCE_MEM HDMI\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start,
				     resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "memory region already claimed\n");
		return -EBUSY;
	}

	/* Base address taken from platform */
	hdmi->ip_data.base_wp = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));

	if (!hdmi->ip_data.base_wp) {
		dev_err(&pdev->dev, "can't ioremap WP\n");
		return -ENOMEM;
	}

	/* Register ASoC codec DAI */
	r = snd_soc_register_codec(&pdev->dev, &hdmi_audio_codec_drv,
					&hdmi_codec_dai_drv, 1);
	if (r) {
		dev_err(&pdev->dev, "can't register ASoC HDMI audio codec\n");
		return r;
	}

	return 0;
}

static int __devexit hdmi_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}


static struct platform_driver hdmi_codec_driver = {
	.probe          = hdmi_codec_probe,
	.remove         = __devexit_p(hdmi_codec_remove),
	.driver         = {
		.name   = DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

module_platform_driver(hdmi_codec_driver);


MODULE_AUTHOR("Ricardo Neri <ricardo.neri@ti.com>");
MODULE_DESCRIPTION("ASoC OMAP HDMI codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
