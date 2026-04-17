#pragma once

#include <juce_core/juce_core.h>

// Port of nmedit/libs/nordmodular/data/module-descriptions/nmformat.js
//
// Single entry point: format(formatterName, value) — dispatches by name to
// the 63 fmtXxx functions defined by the original Nomad editor. Formatter
// names come from ParameterDescriptor::formatter (parsed from modules.xml).
//
// Special non-function expressions from modules.xml are also recognized:
//   "value+1"   → value + 1
//   "value-64"  → value - 64
//
// Unknown names fall back to String(value).
namespace ValueFormatters
{
    juce::String format (const juce::String& formatterName, int value);
}
