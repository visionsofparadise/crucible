#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>

namespace crucible::ui {

// Control kinds emitted by libfaust's JSON UI. Mirrors the `type` field
// strings ("hslider", "vslider", "nentry", "button", "checkbox",
// "hbargraph", "vbargraph"). Unknown types are rejected at parse time.
enum class ControlKind
{
    HSlider,
    VSlider,
    NEntry,
    Button,
    Checkbox,
    HBargraph,
    VBargraph
};

bool      isFloatKind    (ControlKind k) noexcept; // hslider/vslider/nentry
bool      isBoolKind     (ControlKind k) noexcept; // button/checkbox
bool      isBargraphKind (ControlKind k) noexcept; // hbargraph/vbargraph

const char* controlKindToString (ControlKind k) noexcept;

// Group kinds — direct match to Faust's `vgroup`/`hgroup`/`tgroup`.
enum class GroupKind
{
    VGroup,
    HGroup,
    TGroup
};

const char* groupKindToString (GroupKind k) noexcept;

// Metadata extracted from a Faust control's `meta` array. The array
// entries are single-key objects (e.g. `{"unit":"Hz"}`); we flatten
// them into a name→value map. Multiple values for the same key (Faust
// allows that for `midi`) collapse to the last-seen value — sufficient
// for v1.
using Metadata = std::unordered_map<std::string, std::string>;

// Forward-declared so Group can hold a vector of variants.
struct Control;
struct Group;
using Node = std::variant<Control, Group>;

struct Control
{
    ControlKind  kind = ControlKind::HSlider;
    std::string  label;
    std::string  address;

    // Continuous controls: ranges from the JSON UI. Bools fill these
    // with init=0, min=0, max=1, step=1 for uniformity.
    float        init = 0.0f;
    float        min  = 0.0f;
    float        max  = 1.0f;
    float        step = 0.0f;

    Metadata     metadata;

    // Slot binding annotated by `ParamBridge::rebindForSchema`. Empty
    // string when unbound (over-budget case) or before annotation.
    std::string  slotId;
};

struct Group
{
    GroupKind          kind = GroupKind::VGroup;
    std::string        label;
    std::vector<Node>  children;
};

// Top-level parsed schema. `dspName`/`inputs`/`outputs` mirror Faust's
// header fields; `root` is the synthesised top-level group containing
// every UI item. If the JSON's top-level `ui` is empty, `root` is a
// vgroup with no children. `flatControls` is a parse-time flattened
// list of every leaf (post-order), giving ParamBridge a stable
// iteration order without re-walking the tree.
struct Schema
{
    std::string        dspName;
    int                inputs  = 0;
    int                outputs = 0;
    Group              root;

    // Pointers into `root`'s leaves. Stable as long as `root` is not
    // mutated structurally. `ParamBridge::rebindForSchema` uses these
    // to write the bound `slotId` back into each control in place.
    std::vector<Control*> flatControls;
};

// Parse the JSON UI emitted by libfaust (`factory->getJSON`). On
// malformed input returns std::nullopt and writes a human-readable
// reason into `outError` if non-null. Unknown control/group types
// also return std::nullopt.
std::unique_ptr<Schema> parseSchema (const std::string& jsonText,
                                     std::string*       outError = nullptr);

// Serialise the schema (with `slotId` annotations) into a JSON string
// the webview consumes. Shape: a tree of groups + controls preserving
// label/address/range/metadata/slotId. Stable across compiles so the
// UI can diff cheaply.
std::string serializeForWebview (const Schema& schema);

} // namespace crucible::ui
