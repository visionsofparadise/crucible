// Phase 5.1 verification: crucible::ui::parseSchema turns the JSON UI
// emitted by libfaust into a typed tree with control leaves, group
// containers, addresses, and metadata. serializeForWebview round-trips
// the structure including slot-id annotations.

#include <catch2/catch_test_macros.hpp>

#include "Schema.h"

#include <juce_core/juce_core.h>

namespace
{
    // Representative JSON UI matching libfaust 2.85's output shape for
    // a Faust source like:
    //
    //   import("stdfaust.lib");
    //   freq  = hslider("freq [unit:Hz] [scale:log]", 440, 20, 20000, 1);
    //   gate  = button("gate");
    //   gain  = hslider("gain", 0.5, 0, 1, 0.01);
    //   env   = en.adsr(0.01, 0.1, 0.8, 0.5, gate);
    //   process = vgroup("Synth",
    //                hgroup("Oscillator", os.osc(freq) * env * gain)
    //              );
    //
    // This is a representative shape — not the exact byte-for-byte
    // output. Tests parse-time semantics, not Faust's JSON formatting.
    constexpr const char* kRepresentativeJson = R"({
      "name":    "MySynth",
      "inputs":  0,
      "outputs": 1,
      "ui": [
        {
          "type":  "vgroup",
          "label": "Synth",
          "items": [
            {
              "type":  "hgroup",
              "label": "Oscillator",
              "items": [
                {
                  "type":    "hslider",
                  "label":   "freq",
                  "address": "/MySynth/Synth/Oscillator/freq",
                  "init":    440,
                  "min":     20,
                  "max":     20000,
                  "step":    1,
                  "meta": [
                    {"unit":  "Hz"},
                    {"scale": "log"}
                  ]
                },
                {
                  "type":    "hslider",
                  "label":   "gain",
                  "address": "/MySynth/Synth/Oscillator/gain",
                  "init":    0.5,
                  "min":     0,
                  "max":     1,
                  "step":    0.01
                }
              ]
            },
            {
              "type":    "button",
              "label":   "gate",
              "address": "/MySynth/Synth/gate"
            },
            {
              "type":    "hbargraph",
              "label":   "envOut",
              "address": "/MySynth/Synth/envOut",
              "min":     0,
              "max":     1,
              "step":    0
            }
          ]
        }
      ]
    })";
}

TEST_CASE ("Schema parses tree shape, addresses, and metadata", "[schema]")
{
    std::string err;
    auto schema = crucible::ui::parseSchema (kRepresentativeJson, &err);
    REQUIRE (schema != nullptr);
    REQUIRE (err.empty());

    REQUIRE (schema->dspName == "MySynth");
    REQUIRE (schema->inputs  == 0);
    REQUIRE (schema->outputs == 1);

    // Top-level synthesised vgroup, one real child group "Synth".
    REQUIRE (schema->root.kind == crucible::ui::GroupKind::VGroup);
    REQUIRE (schema->root.children.size() == 1);

    auto* synth = std::get_if<crucible::ui::Group> (&schema->root.children[0]);
    REQUIRE (synth != nullptr);
    REQUIRE (synth->kind  == crucible::ui::GroupKind::VGroup);
    REQUIRE (synth->label == "Synth");
    REQUIRE (synth->children.size() == 3);

    // First child: hgroup "Oscillator" with two hsliders.
    auto* osc = std::get_if<crucible::ui::Group> (&synth->children[0]);
    REQUIRE (osc != nullptr);
    REQUIRE (osc->kind  == crucible::ui::GroupKind::HGroup);
    REQUIRE (osc->label == "Oscillator");
    REQUIRE (osc->children.size() == 2);

    auto* freq = std::get_if<crucible::ui::Control> (&osc->children[0]);
    REQUIRE (freq != nullptr);
    REQUIRE (freq->kind    == crucible::ui::ControlKind::HSlider);
    REQUIRE (freq->label   == "freq");
    REQUIRE (freq->address == "/MySynth/Synth/Oscillator/freq");
    REQUIRE (freq->init    == 440.0f);
    REQUIRE (freq->min     == 20.0f);
    REQUIRE (freq->max     == 20000.0f);
    REQUIRE (freq->step    == 1.0f);
    REQUIRE (freq->metadata.at ("unit")  == "Hz");
    REQUIRE (freq->metadata.at ("scale") == "log");

    // Second child of Synth: button "gate".
    auto* gate = std::get_if<crucible::ui::Control> (&synth->children[1]);
    REQUIRE (gate != nullptr);
    REQUIRE (gate->kind    == crucible::ui::ControlKind::Button);
    REQUIRE (gate->address == "/MySynth/Synth/gate");
    REQUIRE (crucible::ui::isBoolKind (gate->kind));

    // Third child: hbargraph "envOut".
    auto* envOut = std::get_if<crucible::ui::Control> (&synth->children[2]);
    REQUIRE (envOut != nullptr);
    REQUIRE (envOut->kind == crucible::ui::ControlKind::HBargraph);
    REQUIRE (crucible::ui::isBargraphKind (envOut->kind));

    // Flat control list — three controls in post-order:
    // freq, gain (osc group leaves first), then gate, envOut.
    REQUIRE (schema->flatControls.size() == 4);
    REQUIRE (schema->flatControls[0]->address == "/MySynth/Synth/Oscillator/freq");
    REQUIRE (schema->flatControls[1]->address == "/MySynth/Synth/Oscillator/gain");
    REQUIRE (schema->flatControls[2]->address == "/MySynth/Synth/gate");
    REQUIRE (schema->flatControls[3]->address == "/MySynth/Synth/envOut");
}

TEST_CASE ("Schema serializeForWebview includes slot annotations", "[schema]")
{
    auto schema = crucible::ui::parseSchema (kRepresentativeJson);
    REQUIRE (schema != nullptr);

    // Annotate slot IDs as ParamBridge would.
    REQUIRE (schema->flatControls.size() == 4);
    schema->flatControls[0]->slotId = "slot.f.000";
    schema->flatControls[1]->slotId = "slot.f.001";
    schema->flatControls[2]->slotId = "slot.b.001"; // slot.b.000 is bypass
    schema->flatControls[3]->slotId = "slot.m.000";

    const auto json = crucible::ui::serializeForWebview (*schema);
    REQUIRE_FALSE (json.empty());

    // Re-parse the serialised JSON via juce::JSON so we don't string-
    // match against a formatter-specific layout.
    juce::var v;
    const auto pr = juce::JSON::parse (juce::String (json), v);
    REQUIRE (pr.wasOk());

    REQUIRE (v["name"].toString()    == "MySynth");
    REQUIRE (static_cast<int> (v["inputs"])  == 0);
    REQUIRE (static_cast<int> (v["outputs"]) == 1);

    // Walk to the Oscillator/freq control and check its slotId.
    const auto& ui    = v["ui"];
    const auto& synth = ui["items"][0];
    REQUIRE (synth["label"].toString() == "Synth");
    const auto& osc = synth["items"][0];
    REQUIRE (osc["label"].toString() == "Oscillator");
    const auto& freq = osc["items"][0];
    REQUIRE (freq["address"].toString() == "/MySynth/Synth/Oscillator/freq");
    REQUIRE (freq["slotId"].toString()  == "slot.f.000");
    REQUIRE (freq["meta"]["unit"].toString()  == "Hz");
    REQUIRE (freq["meta"]["scale"].toString() == "log");
}

TEST_CASE ("Schema preserves [style:menu{...}] metadata verbatim", "[schema]")
{
    // Why this test: the C++ Schema layer stores Faust meta entries as
    // a flat key->value map; the TS layer (DynamicControl.parseStyleOptions)
    // is what splits `menu{label1:v1; label2:v2}` into options. The C++
    // contract is to preserve the raw value byte-for-byte so the TS
    // parser sees what Faust emitted. A regression that aggressively
    // unescapes / trims / collapses inner braces would silently break
    // the menu/radio rendering rule from the JSON UI rendering table.
    constexpr const char* withStyleMenu = R"({
      "name": "Mix", "inputs": 0, "outputs": 1,
      "ui": [
        {
          "type":    "hslider",
          "label":   "shape",
          "address": "/Mix/shape",
          "init":    0,
          "min":     0,
          "max":     2,
          "step":    1,
          "meta": [
            {"style": "menu{sine:0; saw:1; square:2}"}
          ]
        }
      ]
    })";

    std::string err;
    auto schema = crucible::ui::parseSchema (withStyleMenu, &err);
    REQUIRE (schema != nullptr);
    REQUIRE (err.empty());
    REQUIRE (schema->flatControls.size() == 1);

    auto* shape = schema->flatControls.front();
    REQUIRE (shape->metadata.count ("style") == 1);
    REQUIRE (shape->metadata.at ("style") == "menu{sine:0; saw:1; square:2}");

    // serializeForWebview must also round-trip the raw string so the
    // webview's parser sees the exact characters libfaust emitted.
    const auto json = crucible::ui::serializeForWebview (*schema);
    juce::var v;
    REQUIRE (juce::JSON::parse (juce::String (json), v).wasOk());
    REQUIRE (v["ui"]["items"][0]["meta"]["style"].toString()
             == "menu{sine:0; saw:1; square:2}");
}

TEST_CASE ("Schema parses vbargraph as a bargraph leaf", "[schema]")
{
    // Why this test: hbargraph is exercised by the representative-JSON
    // case above, but vbargraph travels through the same enum-mapping
    // table and a typo in the parseControlKind switch (e.g. mapping
    // "vbargraph" to HBargraph or vice versa) would only surface here.
    // Bargraphs are also the only kind where the JSON omits `init` and
    // `step`; isBargraphKind drives the range-defaulting branch in
    // parseControl.
    constexpr const char* withVbargraph = R"({
      "name": "Meter", "inputs": 1, "outputs": 1,
      "ui": [
        {
          "type":    "vbargraph",
          "label":   "rms",
          "address": "/Meter/rms",
          "min":     0,
          "max":     1
        }
      ]
    })";

    auto schema = crucible::ui::parseSchema (withVbargraph);
    REQUIRE (schema != nullptr);
    REQUIRE (schema->flatControls.size() == 1);

    auto* rms = schema->flatControls.front();
    REQUIRE (rms->kind == crucible::ui::ControlKind::VBargraph);
    REQUIRE (crucible::ui::isBargraphKind (rms->kind));
    REQUIRE_FALSE (crucible::ui::isFloatKind (rms->kind));
    REQUIRE_FALSE (crucible::ui::isBoolKind  (rms->kind));
    REQUIRE (rms->min == 0.0f);
    REQUIRE (rms->max == 1.0f);
}

TEST_CASE ("Schema rejects unknown control type", "[schema]")
{
    constexpr const char* bad = R"({
      "name": "X", "inputs": 0, "outputs": 1,
      "ui": [ { "type": "frobnicator", "label": "f", "address": "/X/f" } ]
    })";

    std::string err;
    auto schema = crucible::ui::parseSchema (bad, &err);
    REQUIRE (schema == nullptr);
    REQUIRE_FALSE (err.empty());
}
