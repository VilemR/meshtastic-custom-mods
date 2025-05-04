#pragma once
#include "SinglePortModule.h"


enum SIGNAL_REPLY_MODULE_COMMAND {
  SERVICE_PING_ON,        // 0
  SERVICE_PING_OFF,       // 1
  REQUEST_PING_REPLY,
  SERVICE_DISCOVERY,      // 2
  SERVICE_LOC_ON,
  SERVICE_LOC_OFF,
  REQUEST_LOC_REPLY,
  UNKNOWN_COMMAND
};

class SignalReplyModule : public SinglePortModule, public Observable<const meshtastic_MeshPacket *>
{
  public:
    /** Constructor
     * name is for debugging output
     */
    SignalReplyModule() : SinglePortModule("XXXXMod", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

  //virtual ~SignalReplyModule() {}

  protected:
    /** For reply module we do all of our processing in the (normally optional)
     * want_replies handling
     */
    bool pingServiceEnabled = 0;
    bool locServiceEnabled = 0;
    
    unsigned long activationPingTime = 0;
    unsigned long activationLocTime = 0;

    //unsigned long EXPIRATION_TIME_MS = 1000 * 60 * 60 * 8; 
    unsigned long EXPIRATION_TIME_MS = 1000 * 60; 

    virtual meshtastic_MeshPacket *allocReply() override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    SIGNAL_REPLY_MODULE_COMMAND getCommand(const meshtastic_MeshPacket &currentRequest);
    void reply(const meshtastic_MeshPacket &currentRequest, SIGNAL_REPLY_MODULE_COMMAND command);

};

extern SignalReplyModule *signalReplyModule;
