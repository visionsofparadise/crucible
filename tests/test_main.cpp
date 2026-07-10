#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

// juce::AudioProcessorValueTreeState starts a juce::Timer in its constructor,
// which needs the MessageManager that ScopedJuceInitialiser_GUI installs.
// Without it the APVTS-constructing tests crash on macOS/Linux; Windows
// tolerates the missing message loop, so this only surfaced in CI.
int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return Catch::Session().run (argc, argv);
}
