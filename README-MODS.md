# CUSTOM MESHTASTIC MODS
This is an experimental set of custom, purpose-built modifications to the official Meshtastic firmware, designed to **help advanced users fine-tune their Meshtastic networks**.

- default channel for all (if direct then redistributed) is skipping but special prak channel is redistributed

There is no guarantee of stability or full functionality. This is an early proof-of-concept release intended to explore potential responses to future challenges that may arise as Meshtastic networks grow in size and complexity.

It includes a new **SignalReplyModule**, which allows for automated replies to received "Ping" messages. The response can either include RSSI/SNR signal quality (useful for evaluating link performance) or the number of hops the message took to reach the replying node.

Additionally, a modified **Range Test Module** extends the standard "loc" message by including a Google Maps link, making it easier to identify the origin of the tested location.

The most recent release also includes a **modification to the Router class** that reduces airtime usage by dropping non-core Meshtastic packets. This helps prevent excessive load on exposed nodes that serve as relays in large networks. The filtering implementation is based on the pioneering work of CamFlyerCH (Jean-Marc Ulrich). 

**All features can be controlled remotely.** The current version supports the following commands (case insensitive):
 

 - **Services? / Serv?** – Requests a list of services installed on the remote node. The node responds to this command whether it is sent directly or broadcast on the general channel.
 - Status? / Stat? – Requests the status of services on a remote node running this modified firmware. It shows which services are active or inactive. Responds only to direct messages sent to the remote node; broadcast or indirect messages are ignored.
 - **Ping ON/OFF** – Enables or disables the service that replies to incoming "Ping" requests.Responds only to direct messages sent to the remote node; broadcast or indirect messages are ignored.
 - **Ping** – Sends a ping request. The response includes either signal quality (RSSI/SNR) or the number of hops it took to reach the responding node.Responds only to direct messages sent to the remote node; broadcast or indirect messages are ignored.
 - **LOC ON/OFF** – Enables or disables the extended "LOC" response, which includes a Google Maps link to the reported location.Responds only to direct messages sent to the remote node; broadcast or indirect messages are ignored.
 - **FILT ON/OFF** – Enables (ON) or disables (OFF, default Meshtastic routing behavior) a custom packet filter implemented in Router.cpp. When enabled, all incoming Meshtastic packet types (PORTNUM) are received and processed locally. However, only core Meshtastic packets are relayed further. Less critical packets (system or overhead traffic) — such as telemetry, ATAK, or unknown types—are either delayed or dropped entirely.

This feature is intended for strategically placed nodes exposed to a high volume of traffic. By reducing unnecessary packet forwarding, it helps minimize spectrum congestion and prevents network overload or collapse. On the other hand, this filter guarantees that the core functionality of the Meshtastic network remains fully operational — ensuring neighbor node awareness and unrestricted text message forwarding between nodes.