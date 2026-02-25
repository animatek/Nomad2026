#include "MidiDeviceManager.h"

MidiDeviceManager::MidiDeviceManager(NmProtocol& proto)
    : protocol(proto)
{
    // Wire the protocol's send function to our output
    protocol.setSendFunction([this](const std::vector<uint8_t>& data)
    {
        sendSysEx(data);
    });
}

MidiDeviceManager::~MidiDeviceManager()
{
    disconnect();
}

juce::Array<juce::MidiDeviceInfo> MidiDeviceManager::getAvailableInputDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

juce::Array<juce::MidiDeviceInfo> MidiDeviceManager::getAvailableOutputDevices()
{
    return juce::MidiOutput::getAvailableDevices();
}

bool MidiDeviceManager::connect(const juce::String& inputId, const juce::String& outputId)
{
    disconnect();

    midiInput = juce::MidiInput::openDevice(inputId, this);
    midiOutput = juce::MidiOutput::openDevice(outputId);

    if (midiInput == nullptr || midiOutput == nullptr)
    {
        disconnect();
        return false;
    }

    midiInput->start();

    DBG("MIDI connected: input=" + getInputDeviceName() + " output=" + getOutputDeviceName());
    return true;
}

void MidiDeviceManager::disconnect()
{
    if (midiInput)
    {
        midiInput->stop();
        midiInput.reset();
    }
    midiOutput.reset();
}

void MidiDeviceManager::sendSysEx(const std::vector<uint8_t>& data)
{
    // data already contains the full SysEx frame (F0 ... F7) from SysEx::encode(),
    // so construct MidiMessage directly — createSysExMessage would double-wrap.
    if (midiOutput && !data.empty())
    {
#if JUCE_DEBUG
        juce::String hex;
        for (auto b : data)
            hex += juce::String::toHexString(b).paddedLeft('0', 2) + " ";
        DBG("TX SysEx [" + juce::String(data.size()) + "]: " + hex.trimEnd());
#endif
        midiOutput->sendMessageNow(juce::MidiMessage(data.data(), static_cast<int>(data.size())));
    }
}

juce::String MidiDeviceManager::getInputDeviceName() const
{
    return midiInput ? midiInput->getName() : juce::String();
}

juce::String MidiDeviceManager::getOutputDeviceName() const
{
    return midiOutput ? midiOutput->getName() : juce::String();
}

void MidiDeviceManager::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!message.isSysEx())
        return;

    // Use getRawData to get the full SysEx frame (F0 ... F7) that SysEx::decode() expects.
    // getSysExData() strips the leading F0 which would break our decoder.
    auto* data = message.getRawData();
    auto size = message.getRawDataSize();

    // Wire format: F0 33 [header] 06 ...
    if (size < 5 || data[1] != 0x33 || data[3] != 0x06)
        return;

    // Forward to protocol on the message thread
    auto sysexCopy = std::make_shared<std::vector<uint8_t>>(data, data + size);
    juce::MessageManager::callAsync([this, sysexCopy]()
    {
#if JUCE_DEBUG
        juce::String hex;
        for (auto b : *sysexCopy)
            hex += juce::String::toHexString(b).paddedLeft('0', 2) + " ";
        DBG("RX SysEx [" + juce::String(sysexCopy->size()) + "]: " + hex.trimEnd());
#endif
        protocol.processIncoming(sysexCopy->data(), sysexCopy->size());
    });
}
