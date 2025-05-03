#include "SignalReplyModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"

SignalReplyModule *signalReplyModule;

const char *strcasestr_custom(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return nullptr;
    size_t needle_len = strlen(needle);
    if (!needle_len)
        return haystack;
    for (; *haystack; ++haystack)
    {
        if (strncasecmp(haystack, needle, needle_len) == 0)
        {
            return haystack;
        }
    }
    return nullptr;
}

SIGNAL_REPLY_MODULE_COMMAND SignalReplyModule::getCommand(const meshtastic_MeshPacket &currentRequest)
{
    auto &p = currentRequest.decoded;
        char messageRequest[250];
        for (size_t i = 0; i < p.payload.size; ++i)
        {
            messageRequest[i] = static_cast<char>(p.payload.bytes[i]);
        }
        messageRequest[p.payload.size] = '\0';

    if (strcasestr_custom(messageRequest, "ping on") == 0)
    {
        return SERVICE_PING_ON;
    }
    else if (strcasestr_custom(messageRequest, "ping off") == 0)
    {
        return SERVICE_PING_OFF;
    }
    else if (strcasestr_custom(messageRequest, "ping") == 0)
    {
        return REQUEST_PING_REPLY;
    }
    else if (strcasestr_custom(messageRequest, "services?") == 0)
    {
        return SERVICE_DISCOVERY;
    }
    else if (strcasestr_custom(messageRequest, "loc on") == 0)
    {
        return SERVICE_LOC_ON;
    }
    else if (strcasestr_custom(messageRequest, "loc off") == 0)
    {
        return SERVICE_LOC_OFF;
    }
    else if (strcasestr_custom(messageRequest, "seq ") == 0)
    {
        return REQUEST_LOC_REPLY;
    }
    return UNKNOWN_COMMAND;
}

void SignalReplyModule::reply(const meshtastic_MeshPacket &currentRequest, SIGNAL_REPLY_MODULE_COMMAND command)
{
    if (currentRequest.from != 0x0 && currentRequest.from != nodeDB->getNodeNum())
    {

        

        int hopLimit = currentRequest.hop_limit;
        int hopStart = currentRequest.hop_start;
        char idSender[10];
        char idReceipient[10];
        snprintf(idSender, sizeof(idSender), "%d", currentRequest.from);
        snprintf(idReceipient, sizeof(idReceipient), "%d", nodeDB->getNodeNum());
        char messageReply[250];
        meshtastic_NodeInfoLite *nodeSender = nodeDB->getMeshNode(currentRequest.from);
        const char *nodeRequesting = nodeSender->has_user ? nodeSender->user.short_name : idSender;
        meshtastic_NodeInfoLite *nodeReceiver = nodeDB->getMeshNode(nodeDB->getNodeNum());
        const char *nodeMeassuring = nodeReceiver->has_user ? nodeReceiver->user.short_name : idReceipient;
        //LOG_ERROR("SignalReplyModule::handleReceived(): '%s' from %s.", messageRequest, nodeRequesting);
        if (hopLimit != hopStart)
        {
            snprintf(messageReply, sizeof(messageReply), "%s: RSSI/SNR cannot be determined due to indirect connection through %d nodes!", nodeRequesting, (hopLimit - hopStart));
        }
        else
        {
            snprintf(messageReply, sizeof(messageReply), "Request '%s'->'%s' : RSSI %d dBm, SNR %.1f dB (@%s).", nodeRequesting, nodeMeassuring, currentRequest.rx_rssi, currentRequest.rx_snr, nodeMeassuring);
        }
        auto reply = allocDataPacket();
        reply->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        reply->decoded.payload.size = strlen(messageReply);
        reply->from = getFrom(&currentRequest);
        reply->to = currentRequest.from;
        reply->channel = currentRequest.channel;
        reply->want_ack = (currentRequest.from != 0) ? currentRequest.want_ack : false;
        if (currentRequest.priority == meshtastic_MeshPacket_Priority_UNSET)
        {
            reply->priority = meshtastic_MeshPacket_Priority_RELIABLE;
        }
        reply->id = generatePacketId();
        memcpy(reply->decoded.payload.bytes, messageReply, reply->decoded.payload.size);
        service->handleToRadio(*reply);
    }
}

ProcessMessage SignalReplyModule::handleReceived(const meshtastic_MeshPacket &currentRequest)
{
    if (currentRequest.from != 0x0 && currentRequest.from != nodeDB->getNodeNum())
    {
        SIGNAL_REPLY_MODULE_COMMAND command = getCommand(currentRequest);

        if (command == SERVICE_PING_ON && currentRequest.to == nodeDB->getNodeNum())
        {
            pingServiceEnabled = 1;
            LOG_INFO("SignalReplyModule::handleReceived(): Ping service enabled.");
            reply(currentRequest,command);
        }
        else if (command == SERVICE_PING_OFF && currentRequest.to == nodeDB->getNodeNum())
        {
            pingServiceEnabled = 0;
            LOG_INFO("SignalReplyModule::handleReceived(): Ping service disabled.");
        }
        else if (command == SERVICE_DISCOVERY)
        {
            LOG_INFO("SignalReplyModule::handleReceived(): Service discovery requested.");
        }
        else if (command == SERVICE_LOC_ON && currentRequest.to == nodeDB->getNodeNum())
        {
            locServiceEnabled = 1;
            LOG_INFO("SignalReplyModule::handleReceived(): Location service enabled.");
            reply(currentRequest,command);
        }
        else if (command == SERVICE_LOC_OFF && currentRequest.to == nodeDB->getNodeNum())
        {
            locServiceEnabled = 0;
            LOG_INFO("SignalReplyModule::handleReceived(): Location service disabled.");
        }
        else if (command == REQUEST_PING_REPLY)
        {
            LOG_INFO("SignalReplyModule::handleReceived(): Ping reply requested.");
            reply(currentRequest,command);
        }
        else if (command == REQUEST_LOC_REPLY)
        {
            LOG_INFO("SignalReplyModule::handleReceived(): Location reply requested.");
            reply(currentRequest,command);
        } else {
            LOG_INFO("SignalReplyModule::handleReceived()  FROM:", currentRequest.from);
            LOG_INFO("SignalReplyModule::handleReceived()  TO:", currentRequest.to);
            //LOG_INFO("SignalReplyModule::handleReceived()  HOP_LIMIT:", currentRequest.hop_limit);
            //LOG_INFO("SignalReplyModule::handleReceived()  HOP_START:", currentRequest.hop_start);
            //LOG_INFO("SignalReplyModule::handleReceived()  RX_RSSI:", currentRequest.rx_rssi);
            //LOG_INFO("SignalReplyModule::handleReceived()  RX_SNR:", currentRequest.rx_snr);
            LOG_INFO("SignalReplyModule::handleReceived()  PORTNUM:", currentRequest.decoded.portnum);
            //LOG_INFO("SignalReplyModule::handleReceived()  CHANNEL:", currentRequest.channel);
            //LOG_INFO("SignalReplyModule::handleReceived()  PRIORITY:", currentRequest.priority);
            //LOG_INFO("SignalReplyModule::handleReceived()  WANT_ACK:", currentRequest.want_ack);
        }
    }
    notifyObservers(&currentRequest);
    return ProcessMessage::CONTINUE;
}

meshtastic_MeshPacket *SignalReplyModule::allocReply()
{
    assert(currentRequest); // should always be !NULL
#ifdef DEBUG_PORT
    auto req = *currentRequest;
    auto &p = req.decoded;
    // The incoming message is in p.payload
    LOG_INFO("Received message from=0x%0x, id=%d, msg=%.*s", req.from, req.id, p.payload.size, p.payload.bytes);
#endif
    screen->print("Send reply\n");
    const char *replyStr = "Message Received";
    auto reply = allocDataPacket();                 // Allocate a packet for sending
    reply->decoded.payload.size = strlen(replyStr); // You must specify how many bytes are in the reply
    memcpy(reply->decoded.payload.bytes, replyStr, reply->decoded.payload.size);
    return reply;
}

bool SignalReplyModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}
