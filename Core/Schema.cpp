#include "Schema.h"

#include <juce_core/juce_core.h>

#include <utility>

namespace crucible::ui {

namespace
{
    bool parseControlKind (const juce::String& s, ControlKind& out)
    {
        if (s == "hslider")   { out = ControlKind::HSlider;   return true; }
        if (s == "vslider")   { out = ControlKind::VSlider;   return true; }
        if (s == "nentry")    { out = ControlKind::NEntry;    return true; }
        if (s == "button")    { out = ControlKind::Button;    return true; }
        if (s == "checkbox")  { out = ControlKind::Checkbox;  return true; }
        if (s == "hbargraph") { out = ControlKind::HBargraph; return true; }
        if (s == "vbargraph") { out = ControlKind::VBargraph; return true; }
        return false;
    }

    bool parseGroupKind (const juce::String& s, GroupKind& out)
    {
        if (s == "vgroup") { out = GroupKind::VGroup; return true; }
        if (s == "hgroup") { out = GroupKind::HGroup; return true; }
        if (s == "tgroup") { out = GroupKind::TGroup; return true; }
        return false;
    }

    // Faust emits metadata as either a top-level array of single-key
    // objects on a control, or — in older builds — as `meta` nested
    // under the control. Handle both: any array-of-object field named
    // `meta` is flattened key-by-key into the destination map.
    void readMeta (const juce::var& v, Metadata& dest)
    {
        if (! v.isArray())
            return;
        for (int i = 0; i < v.size(); ++i)
        {
            const auto& entry = v[i];
            if (auto* obj = entry.getDynamicObject())
            {
                for (const auto& prop : obj->getProperties())
                {
                    dest[prop.name.toString().toStdString()] =
                        prop.value.toString().toStdString();
                }
            }
        }
    }

    bool isControlType (const juce::String& s)
    {
        ControlKind k;
        return parseControlKind (s, k);
    }

    bool isGroupType (const juce::String& s)
    {
        GroupKind k;
        return parseGroupKind (s, k);
    }

    // Forward decls.
    bool parseControl (const juce::var& v, Control& out, std::string* outError);
    bool parseGroup   (const juce::var& v, Group& out, std::string* outError);

    bool parseNode (const juce::var& v, std::vector<Node>& dest, std::string* outError)
    {
        auto* obj = v.getDynamicObject();
        if (obj == nullptr)
        {
            if (outError) *outError = "ui item is not an object";
            return false;
        }

        const auto type = v["type"].toString();
        if (isControlType (type))
        {
            Control c;
            if (! parseControl (v, c, outError))
                return false;
            dest.emplace_back (std::move (c));
            return true;
        }
        if (isGroupType (type))
        {
            Group g;
            if (! parseGroup (v, g, outError))
                return false;
            dest.emplace_back (std::move (g));
            return true;
        }

        if (outError) *outError = "unknown ui type: " + type.toStdString();
        return false;
    }

    bool parseControl (const juce::var& v, Control& out, std::string* outError)
    {
        if (! parseControlKind (v["type"].toString(), out.kind))
        {
            if (outError) *outError = "unknown control type";
            return false;
        }

        out.label   = v["label"].toString().toStdString();
        out.address = v["address"].toString().toStdString();

        if (isFloatKind (out.kind) || isBargraphKind (out.kind))
        {
            // hslider/vslider/nentry expose init/min/max/step. bargraphs
            // expose min/max only (init is implicit 0). Faust serialises
            // these as JSON numbers, which juce::var stores as double.
            out.init = static_cast<float> (static_cast<double> (v["init"]));
            out.min  = static_cast<float> (static_cast<double> (v["min"]));
            out.max  = static_cast<float> (static_cast<double> (v["max"]));
            out.step = static_cast<float> (static_cast<double> (v["step"]));
        }
        else
        {
            // button/checkbox — synthesise a 0..1 range with step 1 so
            // ParamBridge can treat every leaf uniformly.
            out.init = 0.0f;
            out.min  = 0.0f;
            out.max  = 1.0f;
            out.step = 1.0f;
        }

        // Metadata: `meta` (Faust JSON UI), but also tolerate the
        // older `metadata` key.
        if (v["meta"].isArray())
            readMeta (v["meta"], out.metadata);
        if (v["metadata"].isArray())
            readMeta (v["metadata"], out.metadata);

        return true;
    }

    bool parseGroup (const juce::var& v, Group& out, std::string* outError)
    {
        if (! parseGroupKind (v["type"].toString(), out.kind))
        {
            if (outError) *outError = "unknown group type";
            return false;
        }
        out.label = v["label"].toString().toStdString();

        const auto& items = v["items"];
        if (! items.isArray())
        {
            // Empty groups are allowed (Faust emits them for empty
            // namespaces). No children → keep `out.children` empty.
            return true;
        }
        for (int i = 0; i < items.size(); ++i)
        {
            if (! parseNode (items[i], out.children, outError))
                return false;
        }
        return true;
    }

    // Walk the group tree in post-order, recording pointers to every
    // Control leaf so ParamBridge can iterate without re-walking.
    void collectControls (Group& g, std::vector<Control*>& dest)
    {
        for (auto& child : g.children)
        {
            if (auto* c = std::get_if<Control> (&child))
                dest.push_back (c);
            else if (auto* sg = std::get_if<Group> (&child))
                collectControls (*sg, dest);
        }
    }

    juce::String controlKindToJsonString (ControlKind k)
    {
        return controlKindToString (k);
    }

    juce::String groupKindToJsonString (GroupKind k)
    {
        return groupKindToString (k);
    }

    juce::var controlToVar (const Control& c)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("type",    juce::String (controlKindToJsonString (c.kind)));
        obj->setProperty ("label",   juce::String (c.label));
        obj->setProperty ("address", juce::String (c.address));
        obj->setProperty ("slotId",  juce::String (c.slotId));
        obj->setProperty ("init",    c.init);
        obj->setProperty ("min",     c.min);
        obj->setProperty ("max",     c.max);
        obj->setProperty ("step",    c.step);

        auto* meta = new juce::DynamicObject();
        for (const auto& kv : c.metadata)
            meta->setProperty (juce::String (kv.first), juce::String (kv.second));
        obj->setProperty ("meta", juce::var (meta));

        return juce::var (obj);
    }

    juce::var groupToVar (const Group& g)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("type",  juce::String (groupKindToJsonString (g.kind)));
        obj->setProperty ("label", juce::String (g.label));

        juce::Array<juce::var> items;
        for (const auto& child : g.children)
        {
            if (auto* c = std::get_if<Control> (&child))
                items.add (controlToVar (*c));
            else if (auto* sg = std::get_if<Group> (&child))
                items.add (groupToVar (*sg));
        }
        obj->setProperty ("items", juce::var (items));
        return juce::var (obj);
    }
}

bool isFloatKind (ControlKind k) noexcept
{
    return k == ControlKind::HSlider
        || k == ControlKind::VSlider
        || k == ControlKind::NEntry;
}

bool isBoolKind (ControlKind k) noexcept
{
    return k == ControlKind::Button || k == ControlKind::Checkbox;
}

bool isBargraphKind (ControlKind k) noexcept
{
    return k == ControlKind::HBargraph || k == ControlKind::VBargraph;
}

const char* controlKindToString (ControlKind k) noexcept
{
    switch (k)
    {
        case ControlKind::HSlider:   return "hslider";
        case ControlKind::VSlider:   return "vslider";
        case ControlKind::NEntry:    return "nentry";
        case ControlKind::Button:    return "button";
        case ControlKind::Checkbox:  return "checkbox";
        case ControlKind::HBargraph: return "hbargraph";
        case ControlKind::VBargraph: return "vbargraph";
    }
    return "unknown";
}

const char* groupKindToString (GroupKind k) noexcept
{
    switch (k)
    {
        case GroupKind::VGroup: return "vgroup";
        case GroupKind::HGroup: return "hgroup";
        case GroupKind::TGroup: return "tgroup";
    }
    return "unknown";
}

std::unique_ptr<Schema> parseSchema (const std::string& jsonText, std::string* outError)
{
    juce::var root;
    const auto parseResult = juce::JSON::parse (juce::String (jsonText), root);
    if (parseResult.failed())
    {
        if (outError) *outError = parseResult.getErrorMessage().toStdString();
        return {};
    }

    auto schema = std::make_unique<Schema>();
    schema->dspName = root["name"].toString().toStdString();
    schema->inputs  = static_cast<int> (root["inputs"]);
    schema->outputs = static_cast<int> (root["outputs"]);

    schema->root.kind  = GroupKind::VGroup;
    schema->root.label = schema->dspName;

    // Faust's top-level `ui` is an array of group/leaf nodes. Treat
    // the array as the children of a synthesised top-level vgroup.
    const auto& ui = root["ui"];
    if (ui.isArray())
    {
        for (int i = 0; i < ui.size(); ++i)
        {
            if (! parseNode (ui[i], schema->root.children, outError))
                return {};
        }
    }
    else if (! ui.isVoid())
    {
        if (outError) *outError = "`ui` is not an array";
        return {};
    }

    collectControls (schema->root, schema->flatControls);
    return schema;
}

std::string serializeForWebview (const Schema& schema)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name",    juce::String (schema.dspName));
    obj->setProperty ("inputs",  schema.inputs);
    obj->setProperty ("outputs", schema.outputs);
    obj->setProperty ("ui",      groupToVar (schema.root));

    return juce::JSON::toString (juce::var (obj), false).toStdString();
}

} // namespace crucible::ui
