// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace debirth
{

//
// One control on a machine's panel: a knob, in the units of its engine's Settings, or a
// switch, by position. The engines turn these into volts and seconds themselves, so all the
// rack has to know of each is its range.
//
template <class Settings>
struct Control
{
    const char* id;                      // as saved
    const char* legend;                  // as printed on the panel
    float lo, hi;
    float Settings::* knob = nullptr;
    int   Settings::* sw   = nullptr;    // a switch instead
    const char* positions = nullptr;     // a switch's position names, "SAW|SQR"
};

//
// A machine's panel, as values: written by the panel, read by the audio thread once a block.
//
template <class Settings>
class Knobs
{
public:
    explicit Knobs(const std::vector<Control<Settings>>& t)
        : table(t), values(new std::atomic<float>[t.size()])
    {
        reset();
    }

    int size() const { return (int) table.size(); }
    const Control<Settings>& control(int i) const { return table[(size_t) i]; }

    int indexOf(const char* id) const
    {
        for (int i = 0; i < size(); ++i)
            if (std::strcmp(table[(size_t) i].id, id) == 0) return i;
        return -1;
    }

    float get(int i) const { return values[(size_t) i].load(std::memory_order_relaxed); }

    void set(int i, float v)
    {
        const auto& c = control(i);
        v = v < c.lo ? c.lo : (v > c.hi ? c.hi : v);
        if (c.sw != nullptr) v = std::round(v);
        values[(size_t) i].store(v, std::memory_order_relaxed);
    }

    float defaultOf(int i) const
    {
        const Settings d {};
        const auto& c = control(i);
        return c.knob != nullptr ? d.*(c.knob) : (float) (d.*(c.sw));
    }

    void reset()
    {
        for (int i = 0; i < size(); ++i) set(i, defaultOf(i));
    }

    Settings read() const
    {
        Settings s;
        for (int i = 0; i < size(); ++i)
        {
            const auto& c = control(i);
            if (c.knob != nullptr) s.*(c.knob) = get(i);
            else                   s.*(c.sw) = (int) get(i);
        }
        return s;
    }

private:
    const std::vector<Control<Settings>>& table;
    std::unique_ptr<std::atomic<float>[]> values;
};

} // namespace debirth
