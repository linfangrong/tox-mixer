#ifndef _KMS_TOX_MIXER_H_
#define _KMS_TOX_MIXER_H_

#include <commons/kmsbasehub.h>

G_BEGIN_DECLS
#define KMS_TYPE_TOX_MIXER   (kms_tox_mixer_get_type())
#define KMS_TOX_MIXER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_TOX_MIXER,KmsToxMixer))
#define KMS_TOX_MIXER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_TOX_MIXER,KmsToxMixerClass))
#define KMS_IS_TOX_MIXER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_TOX_MIXER))
#define KMS_IS_TOX_MIXER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_TOX_MIXER))
typedef struct _KmsToxMixer KmsToxMixer;
typedef struct _KmsToxMixerClass KmsToxMixerClass;
typedef struct _KmsToxMixerPrivate KmsToxMixerPrivate;

struct _KmsToxMixer
{
  KmsBaseHub parent;

  /*< private > */
  KmsToxMixerPrivate *priv;
};

struct _KmsToxMixerClass
{
  KmsBaseHubClass parent_class;
};

GType kms_tox_mixer_get_type (void);

gboolean kms_tox_mixer_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _KMS_TOX_MIXER_H_ */
