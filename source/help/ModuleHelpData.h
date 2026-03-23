#pragma once
//==============================================================================
// ModuleHelpData.h
// Nord Modular G1 — Editor v3.03 help content
// Auto-generated from Nord_Modular_Editor_v3_03_Helpfile.hlp
// © Clavia DMI AB 1999  |  Parser: Nomad2026 project
//==============================================================================

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

namespace NordHelp
{

// std::string is used instead of juce::String to avoid the ASCII-only assertion
// that juce::String(const char*) fires on UTF-8 content (non-ASCII chars in help text).
// Convert to juce::String via juce::String::fromUTF8() at display time.

struct ParamHelp
{
    std::string name;
    std::string description;
};

struct ModuleHelp
{
    std::string name;
    std::string description;
    std::vector<ParamHelp> params;
};

/** Returns the full help database (157 modules). */
const std::vector<ModuleHelp>& getHelpDatabase();

/** Finds help for a module by name (case-insensitive). Returns nullptr if not found. */
const ModuleHelp* findModuleHelp (const juce::String& moduleName);

} // namespace NordHelp
