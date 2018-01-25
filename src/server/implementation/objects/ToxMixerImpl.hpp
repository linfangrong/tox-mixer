#ifndef __TOX_MIXER_IMPL_HPP__ 
#define __TOX_MIXER_IMPL_HPP__

#include "HubImpl.hpp"
#include "ToxMixer.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipeline;
class ToxMixerImpl;

void Serialize (std::shared_ptr<ToxMixerImpl> &object,
                JsonSerializer &serializer);

class ToxMixerImpl : public HubImpl, public virtual ToxMixer
{

public:

  ToxMixerImpl (const boost::property_tree::ptree &conf,
                std::shared_ptr<MediaPipeline> mediaPipeline);

  virtual ~ToxMixerImpl () {};

  void setVideo (std::shared_ptr<HubPort> video);
  void removeVideo ();

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /* __TOX_MIXER_IMPL_HPP__ */
