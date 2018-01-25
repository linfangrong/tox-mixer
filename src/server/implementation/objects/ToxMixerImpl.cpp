#include <gst/gst.h>
#include "MediaPipeline.hpp"
#include "HubPort.hpp"
#include "HubPortImpl.hpp"
#include <ToxMixerImplFactory.hpp>
#include "ToxMixerImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_tox_mixer_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoToxMixerImpl"

#define FACTORY_NAME "toxmixer"
#define VIDEO_PORT "video"

namespace kurento
{

ToxMixerImpl::ToxMixerImpl (const boost::property_tree::ptree &conf,
                            std::shared_ptr<MediaPipeline> mediaPipeline) : HubImpl (conf,
                                    std::dynamic_pointer_cast<MediaObjectImpl> (mediaPipeline), FACTORY_NAME)
{
}

void ToxMixerImpl::setVideo (std::shared_ptr<HubPort> video)
{
  std::shared_ptr<HubPortImpl> videoPort =
    std::dynamic_pointer_cast<HubPortImpl> (video);

  g_object_set (G_OBJECT (element), VIDEO_PORT, videoPort->getHandlerId (), NULL);
}

void ToxMixerImpl::removeVideo ()
{
  g_object_set (G_OBJECT (element), VIDEO_PORT, -1, NULL);
}

MediaObjectImpl *
ToxMixerImplFactory::createObject (const boost::property_tree::ptree &conf,
                                   std::shared_ptr<MediaPipeline> mediaPipeline) const
{
  return new ToxMixerImpl (conf, mediaPipeline);
}

ToxMixerImpl::StaticConstructor ToxMixerImpl::staticConstructor;

ToxMixerImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
