#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "kmstoxmixer.h"
#include <commons/kmsagnosticcaps.h>
#include <commons/kmshubport.h>
#include <commons/kmsrefstruct.h>

#define PLUGIN_NAME "toxmixer"

#define KMS_TOX_MIXER_LOCK(mixer) \
  (g_rec_mutex_lock (&( (KmsToxMixer *) mixer)->priv->mutex))

#define KMS_TOX_MIXER_UNLOCK(mixer) \
  (g_rec_mutex_unlock (&( (KmsToxMixer *) mixer)->priv->mutex))

GST_DEBUG_CATEGORY_STATIC (kms_tox_mixer_debug_category);
#define GST_CAT_DEFAULT kms_tox_mixer_debug_category

#define KMS_TOX_MIXER_GET_PRIVATE(obj) (\
  G_TYPE_INSTANCE_GET_PRIVATE (         \
    (obj),                              \
    KMS_TYPE_TOX_MIXER,                 \
    KmsToxMixerPrivate                  \
  )                                     \
)

#define VIDEO_PORT_NONE (-1)

#define AUDIO_SINK_PAD_PREFIX_COMP "audio_sink_"
#define VIDEO_SINK_PAD_PREFIX_COMP "video_sink_"
#define AUDIO_SRC_PAD_PREFIX_COMP "audio_src_"
#define VIDEO_SRC_PAD_PREFIX_COMP "video_src_"
#define AUDIO_SINK_PAD_NAME_COMP AUDIO_SINK_PAD_PREFIX_COMP "%u"
#define VIDEO_SINK_PAD_NAME_COMP VIDEO_SINK_PAD_PREFIX_COMP "%u"
#define AUDIO_SRC_PAD_NAME_COMP AUDIO_SRC_PAD_PREFIX_COMP "%u"
#define VIDEO_SRC_PAD_NAME_COMP VIDEO_SRC_PAD_PREFIX_COMP "%u"

static GstStaticPadTemplate audio_sink_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SINK_PAD_NAME_COMP,
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_sink_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SINK_PAD_NAME_COMP,
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_VIDEO_CAPS)
    );

static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SRC_PAD_NAME_COMP,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_src_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SRC_PAD_NAME_COMP,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_VIDEO_CAPS)
    );

struct _KmsToxMixerPrivate
{
  GRecMutex mutex;
  GHashTable *ports;
  gint video_port;
};

typedef struct _KmsToxMixerPortData
{
  KmsToxMixer *mixer;
  GstElement *audiomixer;
  gint id;
  GstElement *audio_agnostic;
  GstElement *video_agnostic;
} KmsToxMixerPortData;

enum
{
  PROP_0,
  PROP_VIDEO_PORT
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsToxMixer, kms_tox_mixer,
    KMS_TYPE_BASE_HUB,
    GST_DEBUG_CATEGORY_INIT (kms_tox_mixer_debug_category,
        PLUGIN_NAME, 0, "debug category for tox_mixer element") );


static void
release_src_pads (GstElement *agnosticbin)
{
  GValue val = G_VALUE_INIT;
  GstIterator *it;
  gboolean done = FALSE;

  it = gst_element_iterate_src_pads (agnosticbin);
  do {
    switch (gst_iterator_next (it, &val)) {
      case GST_ITERATOR_OK:
      {
        GstPad *srcpad, *sinkpad;
        GstElement *audiomixer;

        srcpad = g_value_get_object (&val);
        sinkpad = gst_pad_get_peer (srcpad);
        audiomixer = gst_pad_get_parent_element (sinkpad);

        GST_DEBUG ("Unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
            agnosticbin, audiomixer);

        if (!gst_pad_unlink (srcpad, sinkpad)) {
          GST_ERROR ("Can not unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
              srcpad, sinkpad);
        }

        gst_element_release_request_pad (audiomixer, sinkpad);
        gst_element_release_request_pad (agnosticbin, srcpad);

        gst_object_unref (sinkpad);
        gst_object_unref (audiomixer);
        g_value_reset (&val);
        break;
      }
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
        GST_ERROR ("Error iterating over %s's src pads",
            GST_ELEMENT_NAME (agnosticbin));
      case GST_ITERATOR_DONE:
        g_value_unset (&val);
        done = TRUE;
        break;
    }
  } while (!done);

  gst_iterator_free (it);
}

static void
release_sink_pads (GstElement *audiomixer)
{
  GValue val = G_VALUE_INIT;
  GstIterator *it;
  gboolean done = FALSE;

  it = gst_element_iterate_sink_pads (audiomixer);
  do {
    switch (gst_iterator_next (it, &val)) {
      case GST_ITERATOR_OK:
      {
        GstPad *sinkpad, *srcpad;
        GstElement *agnosticbin;

        sinkpad = g_value_get_object (&val);
        srcpad = gst_pad_get_peer (sinkpad);
        agnosticbin = gst_pad_get_parent_element (srcpad);

        GST_DEBUG ("Unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
            agnosticbin, audiomixer);

        if (!gst_pad_unlink (srcpad, sinkpad)) {
          GST_ERROR ("Can not unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
              srcpad, sinkpad);
        }

        gst_element_release_request_pad (audiomixer, sinkpad);
        gst_element_release_request_pad (agnosticbin, srcpad);

        gst_object_unref (srcpad);
        gst_object_unref (agnosticbin);
        g_value_reset (&val);
        break;
      }
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
        GST_ERROR ("Error iterating over %s's src pads",
            GST_ELEMENT_NAME (audiomixer));
      case GST_ITERATOR_DONE:
        g_value_unset (&val);
        done = TRUE;
        break;
    }
  } while (!done);

  gst_iterator_free (it);
}

static KmsToxMixerPortData *
kms_tox_mixer_create_port_data (KmsToxMixer *mixer,
    gint id)
{
  KmsToxMixerPortData *port_data =
      g_slice_new0 (KmsToxMixerPortData);

  port_data->mixer = mixer;
  port_data->audiomixer = gst_element_factory_make ("audiomixerbin", NULL);
  port_data->id = id;
  port_data->audio_agnostic = gst_element_factory_make ("agnosticbin", NULL);
  port_data->video_agnostic = gst_element_factory_make ("agnosticbin", NULL);

  gst_bin_add_many (GST_BIN (mixer), g_object_ref (port_data->audio_agnostic),
      g_object_ref (port_data->video_agnostic), g_object_ref (port_data->audiomixer), NULL);

  gst_element_sync_state_with_parent (port_data->audio_agnostic);
  gst_element_sync_state_with_parent (port_data->video_agnostic);
  gst_element_sync_state_with_parent (port_data->audiomixer);

  kms_base_hub_link_video_sink (KMS_BASE_HUB (mixer), id, port_data->video_agnostic,
      "sink", FALSE);
  kms_base_hub_link_audio_sink (KMS_BASE_HUB (mixer), id, port_data->audio_agnostic,
      "sink", FALSE);
  kms_base_hub_link_audio_src (KMS_BASE_HUB (mixer), id, port_data->audiomixer,
      "src", FALSE);

  return port_data;
}

static void
kms_tox_mixer_destroy_port_data (gpointer data)
{
  KmsToxMixerPortData *port_data =
      (KmsToxMixerPortData *) data;
  KmsToxMixer *mixer = port_data->mixer;

  gst_element_set_state (port_data->audiomixer, GST_STATE_NULL);
  gst_element_set_state (port_data->video_agnostic, GST_STATE_NULL);
  gst_element_set_state (port_data->audio_agnostic, GST_STATE_NULL);

  KMS_TOX_MIXER_LOCK(mixer);
  release_src_pads (port_data->audio_agnostic);
  release_sink_pads (port_data->audiomixer);
  kms_base_hub_unlink_video_sink (KMS_BASE_HUB (mixer), port_data->id);
  kms_base_hub_unlink_audio_sink (KMS_BASE_HUB (mixer), port_data->id);
  kms_base_hub_unlink_audio_src (KMS_BASE_HUB (mixer), port_data->id);
  gst_bin_remove_many (GST_BIN (mixer), port_data->audiomixer,
      port_data->video_agnostic, port_data->audio_agnostic, NULL);
  KMS_TOX_MIXER_UNLOCK(mixer);

  g_clear_object (&port_data->audiomixer);
  g_clear_object (&port_data->video_agnostic);
  g_clear_object (&port_data->audio_agnostic);

  g_slice_free(KmsToxMixerPortData, data);
}

static void
kms_tox_mixer_audiomixer_iterator (gpointer key, gpointer value,
    gpointer user_data)
{
  KmsToxMixerPortData *source_port =
      (KmsToxMixerPortData *) value;
  KmsToxMixerPortData *sink_port =
      (KmsToxMixerPortData *) user_data;
  KmsToxMixer *mixer = source_port->mixer;
  GstPad *src_pad, *sink_pad;

  KMS_TOX_MIXER_LOCK (mixer);
  // 其他人的原音频合并给当前用户
  src_pad = gst_element_get_request_pad (source_port->audio_agnostic, "src_%u");
  sink_pad = gst_element_get_request_pad (sink_port->audiomixer, "sink_%u");
  gst_element_link_pads (source_port->audio_agnostic, GST_OBJECT_NAME (src_pad),
      sink_port->audiomixer, GST_OBJECT_NAME (sink_pad));
  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  // 当前用户的原音频合并给其他人
  src_pad = gst_element_get_request_pad (sink_port->audio_agnostic, "src_%u");
  sink_pad = gst_element_get_request_pad (source_port->audiomixer, "sink_%u");
  gst_element_link_pads (sink_port->audio_agnostic, GST_OBJECT_NAME (src_pad),
      source_port->audiomixer, GST_OBJECT_NAME (sink_pad));
  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  KMS_TOX_MIXER_UNLOCK (mixer);
}

static void
kms_tox_mixer_audiomixer (KmsToxMixerPortData *port_data)
{
  KmsToxMixer *mixer = port_data->mixer;

  KMS_TOX_MIXER_LOCK(mixer);

  g_hash_table_foreach (mixer->priv->ports,
      kms_tox_mixer_audiomixer_iterator, port_data);

  KMS_TOX_MIXER_UNLOCK(mixer);
}

static void
kms_tox_mixer_link_video_port (KmsToxMixer *mixer, gint to)
{
  KmsToxMixerPortData *port_data;

  KMS_TOX_MIXER_LOCK (mixer);
  if (mixer->priv->video_port < 0) {
    kms_base_hub_unlink_video_src (KMS_BASE_HUB (mixer), to);
  } else {
    port_data = g_hash_table_lookup (mixer->priv->ports, 
        GINT_TO_POINTER (mixer->priv->video_port));
    kms_base_hub_link_video_src (KMS_BASE_HUB (mixer), to,
        port_data->video_agnostic, "src_%u", TRUE);
  }
  KMS_TOX_MIXER_UNLOCK (mixer);
}

static void
kms_tox_mixer_change_video_port_iterator (gpointer key, gpointer value,
    gpointer user_data)
{
  KmsToxMixerPortData *port_data = 
      (KmsToxMixerPortData *) value;

  kms_tox_mixer_link_video_port (port_data->mixer, port_data->id);
}

static void
kms_tox_mixer_change_video_port (KmsToxMixer *mixer)
{
  KMS_TOX_MIXER_LOCK (mixer);

  g_hash_table_foreach (mixer->priv->ports,
      kms_tox_mixer_change_video_port_iterator, NULL);

  KMS_TOX_MIXER_UNLOCK (mixer);
}

static gint
kms_tox_mixer_handle_port (KmsBaseHub *hub,
    GstElement *mixer_port)
{
  KmsToxMixer *mixer = KMS_TOX_MIXER (hub);
  KmsToxMixerPortData *port_data;
  gint port_id;

  port_id = KMS_BASE_HUB_CLASS (G_OBJECT_CLASS
      (kms_tox_mixer_parent_class))->handle_port (hub, mixer_port);

  if (port_id < 0)
    return port_id;

  port_data = kms_tox_mixer_create_port_data (mixer, port_id);

  KMS_TOX_MIXER_LOCK (mixer);

  kms_tox_mixer_audiomixer (port_data);

  g_hash_table_insert (mixer->priv->ports, GINT_TO_POINTER (port_id), port_data);

  kms_tox_mixer_link_video_port (mixer, port_id);

  KMS_TOX_MIXER_UNLOCK (mixer);

  return port_id;
}

static void
kms_tox_mixer_unhandle_port (KmsBaseHub *hub, gint id)
{
  KmsToxMixer *mixer= KMS_TOX_MIXER (hub);

  KMS_TOX_MIXER_LOCK (mixer);

  g_hash_table_remove (mixer->priv->ports, GINT_TO_POINTER (id));

  if (mixer->priv->video_port == id) {
    mixer->priv->video_port = VIDEO_PORT_NONE;
    kms_tox_mixer_change_video_port (mixer);
  }

  KMS_TOX_MIXER_UNLOCK (mixer);

  KMS_BASE_HUB_CLASS (G_OBJECT_CLASS
      (kms_tox_mixer_parent_class))->unhandle_port (hub, id);
}

static void
kms_tox_mixer_get_property (GObject *object, guint property_id,
    GValue *value, GParamSpec *pspec)
{
  KmsToxMixer *mixer = KMS_TOX_MIXER (object);

  KMS_TOX_MIXER_LOCK (mixer);
  switch (property_id) {
    case PROP_VIDEO_PORT:
      g_value_set_int (value, mixer->priv->video_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  KMS_TOX_MIXER_UNLOCK (mixer);
}

static void
kms_tox_mixer_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  KmsToxMixer *mixer = KMS_TOX_MIXER (object);

  KMS_TOX_MIXER_LOCK (mixer);
  switch (property_id) {
    case PROP_VIDEO_PORT:
      mixer->priv->video_port = g_value_get_int (value);
      kms_tox_mixer_change_video_port (mixer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  KMS_TOX_MIXER_UNLOCK (mixer);
}

static void
kms_tox_mixer_dispose (GObject *object)
{
  KmsToxMixer *mixer = KMS_TOX_MIXER (object);

  KMS_TOX_MIXER_LOCK (mixer);

  if (mixer->priv->ports != NULL) {
    g_hash_table_remove_all (mixer->priv->ports);
    g_hash_table_unref (mixer->priv->ports);
    mixer->priv->ports = NULL;
  }

  KMS_TOX_MIXER_UNLOCK (mixer);

  G_OBJECT_CLASS (kms_tox_mixer_parent_class)->dispose (object);
}

static void
kms_tox_mixer_finalize (GObject *object)
{
  KmsToxMixer *mixer = KMS_TOX_MIXER (object);

  g_rec_mutex_clear (&mixer->priv->mutex);

  G_OBJECT_CLASS (kms_tox_mixer_parent_class)->finalize (object);
}

static void
kms_tox_mixer_class_init (KmsToxMixerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  KmsBaseHubClass *base_hub_class = KMS_BASE_HUB_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "ToxMixer", "Generic",
      "Tox Mixer element that VIDEO OneToMany and AUDIO mixer except self",
      "Tox <http://code.cloudist.cc/>");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (kms_tox_mixer_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (kms_tox_mixer_finalize);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (kms_tox_mixer_get_property);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (kms_tox_mixer_set_property);

  base_hub_class->handle_port =
      GST_DEBUG_FUNCPTR (kms_tox_mixer_handle_port);
  base_hub_class->unhandle_port =
      GST_DEBUG_FUNCPTR (kms_tox_mixer_unhandle_port);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_factory));

  g_object_class_install_property (gobject_class, PROP_VIDEO_PORT,
      g_param_spec_int ("video",
          "Selected video port",
          "The selected video port, -1 indicates none.", -1, G_MAXINT,
          VIDEO_PORT_NONE, G_PARAM_READWRITE));

  /* Registers a private structure for the instantiatable type */
  g_type_class_add_private (klass, sizeof (KmsToxMixerPrivate));
}

static void
kms_tox_mixer_init (KmsToxMixer *mixer)
{
  mixer->priv = KMS_TOX_MIXER_GET_PRIVATE (mixer);

  g_rec_mutex_init (&mixer->priv->mutex);

  mixer->priv->ports = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) kms_tox_mixer_destroy_port_data);

  mixer->priv->video_port = VIDEO_PORT_NONE;
}

gboolean
kms_tox_mixer_plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_TOX_MIXER);
}
