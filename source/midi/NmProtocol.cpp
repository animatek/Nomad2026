#include "NmProtocol.h"

NmProtocol::NmProtocol()
{
    startTimer(heartbeatIntervalMs);
}

NmProtocol::~NmProtocol()
{
    stopTimer();
}

void NmProtocol::addListener(Listener* listener)
{
    listeners.add(listener);
}

void NmProtocol::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void NmProtocol::sendMessage(int cc, int slot, const std::vector<uint8_t>& payload,
                             bool expectsReply, bool addChecksum)
{
    auto encoded = SysEx::encode(cc, slot, payload, addChecksum);
    sendQueue.push_back({ std::move(encoded), expectsReply });
}

void NmProtocol::processIncoming(const uint8_t* data, size_t length)
{
    auto msg = SysEx::decode(data, length);
    if (msg.valid)
    {
        waitingForReply = false;
        dispatchMessage(msg);
    }
}

void NmProtocol::setSendFunction(std::function<void(const std::vector<uint8_t>&)> fn)
{
    sendFn = std::move(fn);
}

void NmProtocol::timerCallback()
{
    // Check for timeout
    if (waitingForReply)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSendTime > timeoutMs)
        {
            DBG("NmProtocol: reply timeout");
            waitingForReply = false;
            // Could notify listeners of timeout here
        }
        return;
    }

    // Send next queued message
    if (!sendQueue.empty() && sendFn)
    {
        auto& pending = sendQueue.front();
        sendFn(pending.encoded);

        if (pending.expectsReply)
        {
            waitingForReply = true;
            lastSendTime = juce::Time::getMillisecondCounter();
        }

        sendQueue.pop_front();
    }
}

void NmProtocol::dispatchMessage(const SysEx::DecodedMessage& msg)
{
    // Message type is identified by cc field in the SysEx header, not payload
    switch (msg.cc)
    {
        case NmCmd::IAm:
        {
            auto iam = IAmMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onIAmReceived(iam); });
            break;
        }
        case NmCmd::ParameterChange:
        {
            auto param = ParameterChangeMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onParameterChanged(param); });
            break;
        }
        case NmCmd::NMInfo:
        {
            auto info = NMInfoMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onNMInfoReceived(info); });
            break;
        }
        case NmCmd::ACK:
        {
            auto ack = AckMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onAckReceived(ack); });
            break;
        }
        case NmCmd::PatchHandling:
        {
            auto patch = PatchMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onPatchReceived(patch); });
            break;
        }
        case 0x1c: case 0x1d: case 0x1e: case 0x1f:  // PatchPacket
        {
            auto pkt = PatchPacketMessage::decode(msg.cc, msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onPatchPacketReceived(pkt); });
            break;
        }
        default:
            DBG("NmProtocol: unknown cc 0x" + juce::String::toHexString(msg.cc));
            break;
    }
}
