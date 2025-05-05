#include "SignalReplyModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"

// TODO : service enabled to requestor only (to avoid flooding the network with ping-reply requests to not interested users)
// TODO : Pri preposilani Seq ? placeholder na GPS souradnice

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
    
    LOG_ERROR("SignalReplyModule::getCommand(): '%s' from %s.", messageRequest, currentRequest.from == 0 ? "local" : std::to_string(currentRequest.from).c_str());
    
    if (strcasestr_custom(messageRequest, "ping on") != nullptr)
    {
        return SERVICE_PING_ON;
    }
    else if (strcasestr_custom(messageRequest, "ping off") != nullptr)
    {
        return SERVICE_PING_OFF;
    }
    else if (strcasestr_custom(messageRequest, "ping") != nullptr)
    {
        return REQUEST_PING_REPLY;
    }
    else if (strcasestr_custom(messageRequest, "services?") || strcasestr_custom(messageRequest, "serv?") != nullptr ||
             strcasestr_custom(messageRequest, "service?") != nullptr)
    {
        return SERVICE_DISCOVERY;
    }
    else if (strcasestr_custom(messageRequest, "status?") || strcasestr_custom(messageRequest, "stat?") != nullptr)
    {
        return SERVICE_GET_STATUS;
    }
    else if (strcasestr_custom(messageRequest, "loc on") != nullptr)
    {
        return SERVICE_LOC_ON;
    }
    else if (strcasestr_custom(messageRequest, "loc off") != nullptr)
    {
        return SERVICE_LOC_OFF;
    }
    else if (strcasestr_custom(messageRequest, "seq ") != nullptr)
    {
        return REQUEST_LOC_REPLY;
    }
    return UNKNOWN_COMMAND;
}

int hopDistanceSinceSenderHops(int hopStart, int hopLimit){
    //When receiving a packet, the difference between hop_start and hop_limit gives how many hops it traveled.
    return hopStart - hopLimit;
}

int hopDistanceSinceSender(const meshtastic_MeshPacket &currentRequest){
    //When receiving a packet, the difference between hop_start and hop_limit gives how many hops it traveled.
    int hopLimit = currentRequest.hop_limit; // If unset treated as zero (no forwarding, send to direct neighbor nodes only) 
                                             // if 1, allow hopping through one node, etc...
                                             // For our usecase real world topologies probably have a max of about 3.
    int hopStart = currentRequest.hop_start;
    return hopDistanceSinceSenderHops(hopStart, hopLimit);
}

const char* commandToString(SIGNAL_REPLY_MODULE_COMMAND command) {
    switch (command) {
        case SERVICE_PING_ON:
            return "SERVICE_PING_ON";
        case SERVICE_PING_OFF:
            return "SERVICE_PING_OFF";
        case REQUEST_PING_REPLY:
            return "REQUEST_PING_REPLY";
        case SERVICE_DISCOVERY:
            return "SERVICE_DISCOVERY";
        case SERVICE_GET_STATUS:
            return "SERVICE_GET_STATUS";    
        case SERVICE_LOC_ON:
            return "SERVICE_LOC_ON";
        case SERVICE_LOC_OFF:
            return "SERVICE_LOC_OFF";
        case REQUEST_LOC_REPLY:
            return "REQUEST_LOC_REPLY";
        case UNKNOWN_COMMAND:
        default:
            return "UNKNOWN_COMMAND";
    }
}

void SignalReplyModule::reply(const meshtastic_MeshPacket &currentRequest, SIGNAL_REPLY_MODULE_COMMAND command)
{
    if (currentRequest.from != 0x0 && currentRequest.from != nodeDB->getNodeNum())
    {
        LOG_INFO("SignalReplyModule::reply(): COMMAND %s.", commandToString(command));
        int hopLimit = currentRequest.hop_limit;
        int hopStart = currentRequest.hop_start;
        char idSender[10];
        char idReceipient[10];
        int hopReplyLimit = 5;
        int hopDistance = hopDistanceSinceSender(currentRequest);
        snprintf(idSender, sizeof(idSender), "%d", currentRequest.from);
        snprintf(idReceipient, sizeof(idReceipient), "%d", nodeDB->getNodeNum());
        char messageReply[250];
        meshtastic_NodeInfoLite *nodeSender = nodeDB->getMeshNode(currentRequest.from);
        const char *nodeRequesting = nodeSender->has_user ? nodeSender->user.short_name : idSender;
        meshtastic_NodeInfoLite *nodeReceiver = nodeDB->getMeshNode(nodeDB->getNodeNum());
        const char *nodeMeassuring = nodeReceiver->has_user ? nodeReceiver->user.short_name : idReceipient;
        if (command==SERVICE_DISCOVERY) {
            snprintf(messageReply, sizeof(messageReply), "Available services : PING (ON/OFF), LOC (ON/OFF)");
            hopReplyLimit = HOP_LIMIT_OBSERVABLE;  //this request is accupeted when broadcasted to all and to reply within limited range (who might be interested in flooding bandwidth)
            //snprintf(messageReply, sizeof(messageReply), "Available services @%s: PING (ON/OFF), LOC (ON/OFF), FILTER, FORWARDER, BRIDGE, STAT (HOP %s)", nodeMeassuring,hopDistance);
        }
        else if (command==SERVICE_GET_STATUS){
            snprintf(messageReply, sizeof(messageReply), "SERVICE STATUS : PING %d, LOC %d", pingServiceEnabled, locServiceEnabled);
        }
        else if (hopLimit != hopStart)
        {
            snprintf(messageReply, sizeof(messageReply), "%s: RSSI/SNR cannot be determined due to indirect connection through %d nodes!", nodeRequesting, (hopDistance));
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

bool isWithinTimespanMs(uint32_t activationTime, uint32_t timeSpanMs)
{
    return (millis() - activationTime) < timeSpanMs;
}

ProcessMessage SignalReplyModule::handleReceived(const meshtastic_MeshPacket &currentRequest)
{
    if (currentRequest.from != 0x0 && currentRequest.from != nodeDB->getNodeNum())
    {
        SIGNAL_REPLY_MODULE_COMMAND command = getCommand(currentRequest);
        if (command == SERVICE_PING_ON && currentRequest.to == nodeDB->getNodeNum())
        {
            pingServiceEnabled = 1;
            activationPingTime = millis();
            LOG_INFO("SignalReplyModule::handleReceived(): Ping service enabled.");
            reply(currentRequest,command);
        }
        else if (command == SERVICE_PING_OFF && currentRequest.to == nodeDB->getNodeNum())
        {
            pingServiceEnabled = 0;
            LOG_INFO("SignalReplyModule::handleReceived(): Ping service disabled.");
        }
        else if (command == SERVICE_DISCOVERY || command == SERVICE_GET_STATUS)
        {
            if (hopDistanceSinceSender(currentRequest) <= HOP_LIMIT_OBSERVABLE) {
                LOG_INFO("SignalReplyModule::handleReceived(): Service discovery requested.");
                reply(currentRequest,command); //HOP_LIMIT_OBSERVABLE
            } else {
                LOG_INFO("SignalReplyModule::handleReceived(): Service discovery requested (but ignored - too far away).");
            }
        }
        else if (command == SERVICE_GET_STATUS && currentRequest.to == nodeDB->getNodeNum())
        {
                LOG_INFO("SignalReplyModule::handleReceived(): Service status requested.");
                reply(currentRequest,command); 
        }
        else if (command == SERVICE_LOC_ON && currentRequest.to == nodeDB->getNodeNum())
        {
            locServiceEnabled = 1;
            activationLocTime = millis();
            LOG_INFO("SignalReplyModule::handleReceived(): Location service enabled.");
            reply(currentRequest,command);
        }
        else if (command == SERVICE_LOC_OFF && currentRequest.to == nodeDB->getNodeNum())
        {
            locServiceEnabled = 0;
            LOG_INFO("SignalReplyModule::handleReceived(): Location service disabled.");
        }
        else if (command == REQUEST_PING_REPLY &&  pingServiceEnabled == 1)
        {
            if (isWithinTimespanMs(activationPingTime, EXPIRATION_TIME_MS))
            {
                LOG_INFO("SignalReplyModule::handleReceived(): Ping reply requested.");
                reply(currentRequest,command);    
            } else {
                LOG_INFO("SignalReplyModule::handleReceived(): Ping reply ignored (activation expired)");
                pingServiceEnabled = 0;
            }
        }    
        else if (command == REQUEST_PING_REPLY &&  pingServiceEnabled == 0)
        {
            LOG_INFO("SignalReplyModule::handleReceived(): Ping reply ignored (OFF)");
        }
        else if (command == REQUEST_LOC_REPLY &&  locServiceEnabled == 1)
        {
            if (isWithinTimespanMs(activationLocTime, EXPIRATION_TIME_MS)){
                LOG_INFO("SignalReplyModule::handleReceived(): Location reply requested.");
                reply(currentRequest,command);
            } else {
                LOG_INFO("SignalReplyModule::handleReceived(): Location reply ignored (activation expired)");
                locServiceEnabled = 0;
            }
        }
        else if (command == REQUEST_LOC_REPLY &&  locServiceEnabled == 0)
        {
            LOG_INFO("SignalReplyModule::handleReceived(): Location reply ignored (OFF)");
        } else {
            LOG_INFO("SignalReplyModule::handleReceived()     FROM: %s", std::to_string(currentRequest.from).c_str());
            LOG_INFO("SignalReplyModule::handleReceived()       TO: %s", std::to_string(currentRequest.to).c_str());
            LOG_INFO("SignalReplyModule::handleReceived()  PORTNUM: %s", std::to_string(currentRequest.decoded.portnum).c_str() );
            //LOG_INFO("SignalReplyModule::handleReceived()  CHANNEL:", currentRequest.channel);
            //LOG_INFO("SignalReplyModule::handleReceived()  PRIORITY:", currentRequest.priority);
            //LOG_INFO("SignalReplyModule::handleReceived()  WANT_ACK:", currentRequest.want_ack);
            //LOG_INFO("SignalReplyModule::handleReceived()  HOP_LIMIT:", currentRequest.hop_limit);
            //LOG_INFO("SignalReplyModule::handleReceived()  HOP_START:", currentRequest.hop_start);
            //LOG_INFO("SignalReplyModule::handleReceived()  RX_RSSI:", currentRequest.rx_rssi);
            //LOG_INFO("SignalReplyModule::handleReceived()  RX_SNR:", currentRequest.rx_snr);
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
