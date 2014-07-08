/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-vpudec
 *
 * VPU based video decoder.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v filesrc location=test.avi ! avidemux !  queue ! vpudec ! videoconvert ! videoscale ! autovideosink
 * ]| The above pipeline decode the test.avi and renders it to the screen.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>
#include "gstimxcommon.h"
#include "gstvpuallocator.h"
#include "gstvpudec.h"

enum
{
  PROP_0,
  PROP_OUTPUT_FORMAT,
  PROP_ADAPTIVE_FRAME_DROP,
  PROP_FRAMES_PLUS,
};

#define DEFAULT_LOW_LATENCY FALSE
#define DEFAULT_OUTPUT_FORMAT 0
#define DEFAULT_ADAPTIVE_FRAME_DROP TRUE
#define DEFAULT_FRAMES_PLUS 3

GST_DEBUG_CATEGORY_STATIC (vpu_dec_debug);
#define GST_CAT_DEFAULT vpu_dec_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

static void gst_vpu_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_vpu_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_vpu_dec_open (GstVideoDecoder * bdec);
static gboolean gst_vpu_dec_close (GstVideoDecoder * bdec);
static gboolean gst_vpu_dec_start (GstVideoDecoder * bdec);
static gboolean gst_vpu_dec_stop (GstVideoDecoder * bdec);
static gboolean gst_vpu_dec_set_format (GstVideoDecoder * bdec,
    GstVideoCodecState * state);
static GstFlowReturn gst_vpu_dec_handle_frame (GstVideoDecoder * bdec,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_vpu_dec_finish (GstVideoDecoder * bdec);
static gboolean gst_vpu_dec_decide_allocation (GstVideoDecoder * bdec,
    GstQuery * query);
static gboolean gst_vpu_dec_reset (GstVideoDecoder * bdec, gboolean hard);

#define gst_vpu_dec_parent_class parent_class
G_DEFINE_TYPE (GstVpuDec, gst_vpu_dec, GST_TYPE_VIDEO_DECODER);

static void
gst_vpu_dec_finalize (GObject * object)
{
  GstVpuDec *dec;
  GST_DEBUG ("finalizing");

  g_return_if_fail (GST_IS_VPU_DEC (object));

  dec = GST_VPU_DEC (object);

  gst_vpu_dec_object_destroy (dec->vpu_dec_object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vpu_dec_class_init (GstVpuDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoDecoderClass *vdec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  vdec_class = (GstVideoDecoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_vpu_dec_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_vpu_dec_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_vpu_dec_get_property);

  g_object_class_install_property (gobject_class, PROP_OUTPUT_FORMAT,
      g_param_spec_enum ("output-format", "output format",
        "set raw video format for output (Y42B NV16 Y444 NV24 only for MJPEG)", \
        GST_TYPE_VPU_DEC_OUTPUT_FORMAT, \
        DEFAULT_OUTPUT_FORMAT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ADAPTIVE_FRAME_DROP,
      g_param_spec_boolean ("frame-drop", "frame drop",
        "enable adaptive frame drop for smoothly playback", 
          DEFAULT_ADAPTIVE_FRAME_DROP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMES_PLUS,
      g_param_spec_uint ("frame-plus", "addtionlal frames",
        "set number of addtional frames for smoothly playback", 
          0, 16, DEFAULT_FRAMES_PLUS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
          gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_vpu_dec_object_get_sink_caps ()));
  gst_element_class_add_pad_template (element_class,
          gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_vpu_dec_object_get_src_caps ()));
  gst_element_class_set_static_metadata (element_class,
      "VPU-based video decoder", "Codec/Decoder/Video",
      "Decode compressed video to raw data",
      IMX_GST_PLUGIN_AUTHOR);

  vdec_class->open = GST_DEBUG_FUNCPTR (gst_vpu_dec_open);
  vdec_class->close = GST_DEBUG_FUNCPTR (gst_vpu_dec_close);
  vdec_class->start = GST_DEBUG_FUNCPTR (gst_vpu_dec_start);
  vdec_class->stop = GST_DEBUG_FUNCPTR (gst_vpu_dec_stop);
  vdec_class->set_format = GST_DEBUG_FUNCPTR (gst_vpu_dec_set_format);
  vdec_class->handle_frame = GST_DEBUG_FUNCPTR (gst_vpu_dec_handle_frame);
  vdec_class->finish = GST_DEBUG_FUNCPTR (gst_vpu_dec_finish);
  vdec_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_vpu_dec_decide_allocation);
  vdec_class->reset = GST_DEBUG_FUNCPTR (gst_vpu_dec_reset);

  GST_DEBUG_CATEGORY_INIT (vpu_dec_debug, "vpudec", 0, "VPU decoder");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
}

static void
gst_vpu_dec_init (GstVpuDec * dec)
{
  GST_DEBUG ("initializing");

  dec->vpu_dec_object = gst_vpu_dec_object_new ();

  GST_VPU_DEC_LOW_LATENCY (dec->vpu_dec_object) = DEFAULT_LOW_LATENCY;
  GST_VPU_DEC_OUTPUT_FORMAT (dec->vpu_dec_object) = DEFAULT_OUTPUT_FORMAT;
  GST_VPU_DEC_FRAME_DROP (dec->vpu_dec_object) = DEFAULT_ADAPTIVE_FRAME_DROP;
  GST_VPU_DEC_FRAMES_PLUS (dec->vpu_dec_object) = DEFAULT_FRAMES_PLUS;
  GST_VPU_DEC_MIN_BUF_CNT (dec->vpu_dec_object) = 0;

  /* As VPU can support stream mode. need call parser before decode */
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec), TRUE);
}

static void
gst_vpu_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVpuDec *dec;

  g_return_if_fail (GST_IS_VPU_DEC (object));
  dec = GST_VPU_DEC (object);

  switch (prop_id) {
    case PROP_OUTPUT_FORMAT:
      g_value_set_enum (value, GST_VPU_DEC_OUTPUT_FORMAT (dec->vpu_dec_object));
      break;
    case PROP_ADAPTIVE_FRAME_DROP:
      g_value_set_boolean (value, GST_VPU_DEC_FRAME_DROP (dec->vpu_dec_object));
      break;
    case PROP_FRAMES_PLUS:
      g_value_set_uint (value, GST_VPU_DEC_FRAMES_PLUS (dec->vpu_dec_object));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vpu_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpuDec *dec;

  g_return_if_fail (GST_IS_VPU_DEC (object));
  dec = GST_VPU_DEC (object);

  switch (prop_id) {
    case PROP_OUTPUT_FORMAT:
      GST_VPU_DEC_OUTPUT_FORMAT (dec->vpu_dec_object) = g_value_get_enum (value);
      break;
    case PROP_ADAPTIVE_FRAME_DROP:
      GST_VPU_DEC_FRAME_DROP (dec->vpu_dec_object) = g_value_get_boolean (value);
      break;
    case PROP_FRAMES_PLUS:
      GST_VPU_DEC_FRAMES_PLUS (dec->vpu_dec_object) = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_vpu_dec_open (GstVideoDecoder * bdec)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_open (dec->vpu_dec_object);
}

static gboolean
gst_vpu_dec_close (GstVideoDecoder * bdec)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_close (dec->vpu_dec_object);
}

static gboolean
gst_vpu_dec_start (GstVideoDecoder * bdec)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_start (dec->vpu_dec_object);
}

static gboolean
gst_vpu_dec_stop (GstVideoDecoder * bdec)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_stop (dec->vpu_dec_object);
}

static gboolean
gst_vpu_dec_set_format (GstVideoDecoder * bdec, GstVideoCodecState * state)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;
  GstQuery *query;
  gboolean is_live;

  query = gst_query_new_latency ();
  is_live = FALSE;
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (bdec), query)) {
    gst_query_parse_latency (query, &is_live, NULL, NULL);
  }
  gst_query_unref (query);

  if (is_live) {
    GST_INFO_OBJECT (dec, "Pipeline is live, set VPU to low latency mode.\n");
    GST_VPU_DEC_LOW_LATENCY (dec->vpu_dec_object) = TRUE;
  } else {
    GST_INFO_OBJECT (dec, "Pipeline isn't live, set VPU to non-latency mode.\n");
    GST_VPU_DEC_LOW_LATENCY (dec->vpu_dec_object) = FALSE;
  }

  return gst_vpu_dec_object_config (dec->vpu_dec_object, bdec, state);
}

static GstFlowReturn
gst_vpu_dec_handle_frame (GstVideoDecoder * bdec, GstVideoCodecFrame * frame)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_decode (dec->vpu_dec_object, bdec, frame);
}

static GstFlowReturn
gst_vpu_dec_finish (GstVideoDecoder * bdec)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  return gst_vpu_dec_object_decode (dec->vpu_dec_object, bdec, NULL);
}

static gboolean
gst_vpu_dec_decide_allocation (GstVideoDecoder * bdec, GstQuery * query)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  GstStructure *config;
  gboolean update_pool, update_allocator;
  GstVideoInfo vinfo;

  gst_query_parse_allocation (query, &outcaps, NULL);
  gst_video_info_init (&vinfo);
  gst_video_info_from_caps (&vinfo, outcaps);

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    /* try the allocator */
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    update_allocator = TRUE;
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
    update_allocator = FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    size = MAX (size, vinfo.size);
    update_pool = TRUE;
  } else {
    pool = NULL;
    size = vinfo.size;
    min = max = 0;

    update_pool = FALSE;
  }

  if (allocator == NULL || !GST_IS_ALLOCATOR_PHYMEM (allocator)) {
    /* no allocator or isn't physical memory allocator. VPU need continus
     * physical memory. use VPU memory allocator. */
    if (allocator) {
      gst_object_unref (allocator);
    }
    GST_DEBUG_OBJECT (dec, "using vpu allocator.\n");
    allocator = gst_vpu_allocator_obtain();
  }

  if (pool) {
    /* need set video alignment. */
    if (!gst_buffer_pool_has_option (pool, \
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
      GST_DEBUG_OBJECT (dec, "buffer pool hasn't video alignment option, unref it.\n");
      gst_object_unref (pool);
      pool = NULL;
    }
  }

  if (pool == NULL) {
    /* no pool, we can make our own */
    GST_DEBUG_OBJECT (dec, "no pool, making new pool");
    pool = gst_video_buffer_pool_new ();
  }

  max = min += GST_VPU_DEC_MIN_BUF_CNT (dec->vpu_dec_object) \
        + GST_VPU_DEC_FRAMES_PLUS (dec->vpu_dec_object);
  GST_VPU_DEC_ACTUAL_BUF_CNT (dec->vpu_dec_object) = min;
  params.align = GST_VPU_DEC_BUF_ALIGNMENT (dec->vpu_dec_object);
  GST_INFO_OBJECT (dec, "vpudec frame buffer count: %d.\n", \
      GST_VPU_DEC_ACTUAL_BUF_CNT (dec->vpu_dec_object));

  /* now configure */
  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }

  if (!gst_buffer_pool_config_has_option (config, \
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
    gst_buffer_pool_config_add_option (config, \
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  gst_buffer_pool_config_set_video_alignment (config, \
      &GST_VPU_DEC_VIDEO_ALIGNMENT (dec->vpu_dec_object));
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_set_allocator (config, allocator, &params);
  gst_buffer_pool_set_config (pool, config);

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);
  if (allocator)
    gst_object_unref (allocator);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  if (!gst_vpu_dec_object_config (dec->vpu_dec_object, bdec, NULL)) {
    GST_DEBUG ("gst_vpu_dec_object_reopen fail.");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_vpu_dec_reset (GstVideoDecoder * bdec, gboolean hard)
{
  GstVpuDec *dec = (GstVpuDec *) bdec;

  if (hard) {
    return gst_vpu_dec_object_flush (bdec, dec->vpu_dec_object);
  } else {
    return TRUE;
  }
}

