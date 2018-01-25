#include <config.h>
#include <gst/gst.h>

#include "kmstoxmixer.h"

static gboolean
init (GstPlugin * kurento)
{
  if (!kms_tox_mixer_plugin_init (kurento))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    toxmixer,
    "Kurento tox mixer",
    init, VERSION, GST_LICENSE_UNKNOWN, "Kurento Tox Mixer", "http://kurento.com/")
