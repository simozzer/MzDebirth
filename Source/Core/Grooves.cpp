// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Grooves.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace debirth
{

namespace
{

//==============================================================================
//
// THE NOTATION
//
// A mono line is sixteen steps, space between:
//
//     C  C#  D ... B  C'     a note, C up to the C above
//     +  -                   after it: an octave up, or down
//     >                      accented
//     ~                      slid into the next step
//     .                      a rest
//
// A drum pattern is a row an instrument, | between rows: the instrument's code as printed
// on its select pad, then sixteen steps, x a hit. AC is the accents.
//
//     "BD x...x...x...x...|CH x.x.x.x.x.x.x.x.|AC x...x...x...x..."
//
// A sampler pattern is the same, each row named by the sample its key starts on, then =,
// and AC and TIE for the accents and the ties.
//
//     "KICK 1=x...x...x...x...|HAT PEDAL=...x...x...x...x"
//

struct Bank
{
    const char* mz303[patternsPerBank];
    const char* mz101[patternsPerBank];
    const char* mz808[patternsPerBank];
    const char* mz909[patternsPerBank];
    const char* mz950[patternsPerBank];
};

const Bank banksOfGrooves[banks] = {

    //========================================================================== A: acid house, C minor
    {
        {
            "C> C C'~ C . D#> C~ A#- C> C+ . G~ F D#>~ C C+",
            "C C> D# C F~ G C . C> A#- C C'~ A# G~ F D#",
            "C>~ C C C D#>~ D# C C A#->~ A#- C C G>~ G F D#",
            "C> C> C> C> D#~ F~ G~ A#~ C'> . C' . G> . F D#~",
            "C . . . C . . . C . . . C . D#~ F",
            "D#> D# . D# F~ D# C . D#> . C' A# G . F G~",
            "C C C'> C C C A#> C C C G> C F~ G D#> C",
            "C> . C . C> . C . C>~ D#~ F~ G~ A#~ C'~ D#+~ C'",
        },
        {
            ". . G . . . A#~ C' . . G . F~ G . .",
            ". . . . C'~ D#+ . . . . A#~ C' . . G .",
            "C' . C' . D#+ . C' . A# . G . A# . C' .",
            ". . . . . . . . C'~ D#+~ F+~ G+ . . . .",
            "C'~ G~ D#~ C . . . . A#-~ C~ D# . . . . .",
            ". . G . . G . . . . A# . . A# C' .",
            "G . . . A# . . . C' . . . D#+ . C' .",
            ". . . . . . . . . . . . G~ A#~ C'~ D#+",
        },
        {
            "BD x...x...x...x..x|CP ....x.......x...|CH x.x.x.x.x.x.x.x.|AC x...x...x...x...",
            "BD x...x...x...x...|CP ....x.......x...|CH x.x.x.x.x.x.x.xx|AC x...x...x...x...",
            "BD x...x...x...x...|CP ....x.......x...|CH x.x.x.x.x.x.x.x.|LT ...........x....|AC x...x...x...x...",
            "BD x...x...x.x.x.x.|SD ............xxxx|CP ....x.......x...|CH x.x.x.x.x.x.....|AC x...x...x...x.xx",
            "CH x.x.x.x.x.x.x.x.|CP ....x.......x...",
            "BD x...x...x...x...|CP ....x.......x...|CH x.x.x.x.x.x.x.x.|MA .x.x.x.x.x.x.x.x|AC x...x...x...x...",
            "BD x...x...x...x...|CP ....x..x....x...|CH x.x.x.x.x.x.x.x.|CB ..x.....x.....x.|AC x...x...x...x...",
            "BD x...x...x...xxxx|SD ........x.x.xxxx|CH x.x.x.x.........|AC x...x...x.x.x.xx",
        },
        {
            "OH ..x...x...x...x.|RC x.x.x.x.x.x.x.x.|RS ......x......x..",
            "OH ..x...x...x...x.|RS ...x..x....x..x.",
            "OH ..x...x...x...x.|RC x.x.x.x.x.x.x.x.|CP ....x.......x...",
            "OH ..x...x...x.....|RC x.x.x.x.x.x.....|RS ............x.x.",
            "OH ..x...x...x...x.",
            "OH ..x...x...x...x.|RS ...x.....x....x.",
            "OH ..x...x...x...x.|RC x.x.x.x.x.x.x.x.",
            "CC x...............|OH ..x...x.........",
        },
        {
            "HAT PEDAL=...x...x...x...x|RIMSHOT=..........x..x..",
            "TOM LO=......x.......x.|HAT PEDAL=...x...x...x...x",
            "SNARE 2=.......x......x.|HAT PEDAL=...x...x...x...x",
            "TOM HI=............x...|TOM MID=.............x..|TOM LO=..............xx",
            "HAT PEDAL=...x...x...x...x",
            "CLAP=....x.......x...|RIMSHOT=..x.....x.x.....",
            "HAT PEDAL=...x...x...x...x|TOM LO=...........x....",
            "TOM HI=........x...x...|TOM MID=.........x...x..|TOM LO=..........x...x.",
        },
    },

    //========================================================================== B: techno, F minor
    {
        {
            "F- F F+ F- F F F+> F- F F F+ F- F G#~ F",
            "F- F G# F- F F C+> F- F F A# F- C+ F G#~ A#",
            "G#- G# G#+ G#- G# G# G#+> G#- G# G# G#+ G#- G# A#~ G#",
            "F> F> F> F> G#> G#> G#> G#> A#> A#> A#> A#> C+> C+> D#+>~ F+",
            "F- . F . F- . F . F- . F . F- . G#~ A#",
            "F- F+ F- F F- F+> F- G# F- F+ F- A# F- C+> F- D#",
            "F-> F . F-> F . F-> F . F-> F . G#~ A# C+ D#+",
            "F- F- F- F- F- F- F- F- F-> F> F+> F> F-> F> F+> F+>",
        },
        {
            "F . . . . . F . . . . . G# . . .",
            ". . . . F+ . . . . . . . D#+ . . .",
            "G# . . . . . G# . . . . . A# . . .",
            ". . . . . . . . . . . . . . . .",
            "F+~ D#+~ C+~ G# . . . . F+~ G#+~ G+ . . . . .",
            ". . . . . . F+ . . . . . . . G#+ .",
            "F+ . . . D#+ . . . C+ . . . A# . . .",
            ". . . . . . . . F+~ G#+~ A#+~ C'+~ . . . .",
        },
        {
            "CL ..x...x.x....x..",
            "CL ..x...x.x....x..|CB ......x.......x.",
            "CL ..x...x.x....x..|MA x.x.x.x.x.x.x.x.",
            "HT ............x...|MT .............x..|LT ..............xx",
            "CL ..x...x.x....x..",
            "CB ..x..x....x..x..|CL x..x..x...x..x..",
            "MA x.x.x.x.x.x.x.x.|CL ..x...x.x....x..",
            "HT ........x..x....|MT ..........x..x..|LT ............x..x",
        },
        {
            "BD x...x...x...x...|OH ..x...x...x...x.|CP ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|CP ....x.......x...|RS .x.....x.....x..|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|LT ...........x....|MT ..............x.|CP ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|SD ........x.x.xxxx|OH ..x...x.........|CP ....x...........|AC x...x...x...x.xx",
            "OH ..x...x...x...x.|CP ....x.......x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|RC x.x.x.x.x.x.x.x.|CP ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|RS ...x..x....x..x.|AC x...x...x...x...",
            "BD x...x...x...xxxx|SD ........xxxxxxxx|OH ..x...x.........|AC x...x...x...x.xx",
        },
        {
            "HAT CLOSED=xxxxxxxxxxxxxxxx|AC=x...x...x...x...",
            "HAT CLOSED=x.xxx.xxx.xxx.xx|RIDE=x...x...x...x...",
            "HAT CLOSED=x.xxx.xxx.xxx.xx|RIDE BELL=..x...x...x...x.",
            "HAT CLOSED=x.xxx.xxx.......",
            "RIDE=x.x.x.x.x.x.x.x.",
            "HAT CLOSED=x.xxx.xxx.xxx.xx",
            "RIDE=x.x.x.x.x.x.x.x.|HAT PEDAL=...x...x...x...x",
            "HAT CLOSED=x.x.x.x.........|CRASH=x...............",
        },
    },

    //========================================================================== C: electro, A minor
    {
        {
            "A- . A- A . . A- . C . . A- . G- A- .",
            "A- . A- A . . C . A- . E . D . C .",
            "D- . D- D . . D- . F . D- . C- . D- A-~",
            "A-> . A-> . A-> . A-> . C> . D> . E> . G> .",
            "A- . . . . . . . A- . . . . . G-~ A-",
            "E- . E- E . . G . E- . D . C . A- .",
            "A- A- A A- C A- D A- E A- G A- E D C A-",
            "A- . . . A- . . . A-> A> A+> A> E> D> C> A>",
        },
        {
            "A- C E A G E C A- A- C E A G E C E",
            "E G A C' A G E C E G A C' D+ C' A G",
            "D F A D+ A F D F D F A D+ F+ D+ A F",
            "A C' E+ A+ G+ E+ C' A A C' E+ A+ C'+ A+ E+ C'",
            "A . . . E . . . G . . . E . D .",
            "E G B E+ B G E G E G B E+ G+ E+ B G",
            "A . C . E . A . G . E . C . E .",
            "A+ . . . . . . . E+~ D+~ C+~ A~ G~ E~ D~ C",
        },
        {
            "BD x......x..x...x.|SD ....x.......x...|CH x.xxx.xxx.xxx.xx|AC x...x...x...x...",
            "BD x......x..x.x...|SD ....x.......x...|CH x.xxx.xxx.xxx.xx|CB ..x.....x.x.....|AC x...x...x...x...",
            "BD x.....x...x.....|SD ....x.......x...|CH xxxxxxxxxxxxxxxx|HC ...x......x....x|AC x...x...x...x...",
            "BD x......x..x.....|SD ....x.......xxxx|CH x.xxx.xxx.xx....|HT ............x...|MT .............x..|LT ..............x.|AC x...x...x...x..x",
            "CH x.xxx.xxx.xxx.xx|CB ..x.....x.x.....",
            "BD x.....x..x..x...|SD ....x.......x...|CH x.xxx.xxx.xxx.xx|CL ...x..x...x..x..|AC x...x...x...x...",
            "BD x......x..x.....|SD ....x..x....x...|CH x.xxx.xxx.xxx.xx|CB x..x..x...x..x..|AC x...x...x...x...",
            "BD x...x...x...x...|SD x.x.x.x.xxxxxxxx|CH x.x.x.x.........|AC x...x...x.x.xxxx",
        },
        {
            "OH ......x.......x.",
            "OH ......x.......x.",
            "OH ......x.......x.|RS ..x.......x.....",
            "OH ......x.........",
            "",
            "OH ......x.......x.",
            "RS ..x.......x.....",
            "CC x...............",
        },
        {
            "CLAP=....x.......x...",
            "CLAP=....x.......x..x",
            "CLAP=....x.......x...|RIMSHOT=..x..x....x.x...",
            "TOM HI=............x...|TOM MID=.............x..|TOM LO=..............xx",
            "CLAP=....x.......x...",
            "CLAP=....x.......x...|SNARE 1=..........x.....",
            "CLAP=....x.......x...",
            "TOM HI=........x.x.....|TOM MID=..........x.x...|TOM LO=............x.xx",
        },
    },

    //========================================================================== D: deep and dub house, G minor
    {
        {
            "G- . . G- . . A#-~ C . . G- . F-~ G- . .",
            "G- . G- . . A#- . C~ D . . C . A#-~ G- .",
            "D#- . . D#- . . F-~ G- . . D#- . D-~ D#- . .",
            "G- . . G- . . A#-~ C . D~ F . G>~ F D C",
            "G- . . . . . . . G- . . . F-~ G- . .",
            "C . . C . . D~ D# . . C . A#-~ C . .",
            "G- G- . G- . A#- . C . G- . G- F- . G- .",
            "G- . . . G- . . . G-~ A#-~ C~ D~ F~ G~ A#~ C'",
        },
        {
            ". . D+ . . . . . . . D+ . . . F+ .",
            ". . A# . . . . . . . A# . . . D+ .",
            ". . G . . . . . . . G . . . A# .",
            ". . D+ . . . . . . . F+ . . G+~ F+ .",
            "D+~ C+~ A# . . . . . . . . . G~ A#~ C+ .",
            ". . C+ . . . . . . . D#+ . . . . .",
            ". . . . D+ . . . . . . . F+ . . .",
            ". . . . . . . . . . . . . . . .",
        },
        {
            "HC ..x...x...x..x..|LC x.......x.....x.|CL ...x..x...x.....",
            "HC ..x...x...x..x..|MC .....x.......x..|CL ...x..x...x.....",
            "HC ..x.x...x.x...x.|LC x.......x.......|CB ..........x.....",
            "HC ..x...x...x..x..|LC x.......x.....x.|MC .............x.x",
            "HC ..x...x...x..x..|CL ...x..x...x.....",
            "MA x.x.x.x.x.x.x.x.|HC ..x...x...x..x..|LC x.......x.....x.",
            "HC ..x...x...x..x..|CL ...x..x...x.....|CB ......x.......x.",
            "HT ........x.x.....|MT ..........x.x...|LT ............x.x.",
        },
        {
            "BD x...x...x...x...|OH ..x...x...x...x.|RS ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|CP ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|RS ....x.......x...|CH x...x...x...x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|RS ....x.......x.x.|AC x...x...x...x...",
            "OH ..x...x...x...x.|RS ....x.......x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|RC x.x.x.x.x.x.x.x.|RS ....x.......x...|AC x...x...x...x...",
            "BD x...x...x...x...|OH ..x...x...x...x.|CP ....x.......x...|AC x...x...x...x...",
            "BD x...x...x.......|OH ..x...x.........|CC ............x...",
        },
        {
            "RIDE=x.x.x.x.x.x.x.x.|HAT PEDAL=...x...x...x...x",
            "RIDE=x.x.x.x.x.x.x.x.|RIMSHOT=.......x.......x",
            "RIDE BELL=..x...x...x...x.|HAT PEDAL=...x...x...x...x",
            "RIDE=x.x.x.x.x.x.x.x.|SNARE 3=............x..x",
            "RIDE=x.x.x.x.x.x.x.x.",
            "RIDE=x.x.x.x.x.x.x.x.|HAT PEDAL=...x...x...x...x",
            "RIDE=x.x.x.x.x.x.x.x.|CLAP=....x.......x...",
            "CRASH=x...............|RIDE=x.x.x.x.x.......",
        },
    },
};

//============================================================================== reading it

std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

std::string upper(std::string s)
{
    for (auto& c : s) c = (char) std::toupper((unsigned char) c);
    return s;
}

std::vector<std::string> split(const std::string& s, char by)
{
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, by))
        if (! trim(part).empty()) out.push_back(trim(part));
    return out;
}

std::string where(char bank, int index, const char* machine)
{
    return std::string(machine) + " " + bank + std::to_string(index % patternsPerBank + 1);
}

bool readStep(std::string token, BassStep& s)
{
    s = {};
    if (token == ".") return true;

    s.accent = token.find('>') != std::string::npos;
    s.slide = token.find('~') != std::string::npos;
    token.erase(std::remove_if(token.begin(), token.end(), [] (char c) { return c == '>' || c == '~'; }), token.end());

    if (! token.empty() && (token.back() == '+' || token.back() == '-'))
    {
        s.octave = token.back() == '+' ? 1 : -1;
        token.pop_back();
    }

    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };
    for (int n = 0; n < 13; ++n)
        if (token == names[n]) { s.note = n; s.gate = true; return true; }
    return false;
}

void mono(Track<BassPattern>& t, int index, const char* text, const std::string& at, std::vector<std::string>& problems)
{
    auto& p = t.patterns[index];
    p.clear();

    std::istringstream in(text);
    std::string token;
    int i = 0;
    while (in >> token)
    {
        BassStep s;
        if (i >= stepsPerPattern)  { problems.push_back(at + ": more than sixteen steps"); break; }
        if (! readStep(token, s))  problems.push_back(at + ": no note \"" + token + "\"");
        p.set(i++, s);
    }
    if (i > 0) p.length = i;
}

// A row's steps, sixteen of them, into the bits of a grid row.
bool readRow(const std::string& steps, uint32_t& bits)
{
    if ((int) steps.size() != stepsPerPattern) return false;
    bits = 0;
    for (int i = 0; i < stepsPerPattern; ++i)
    {
        if (steps[(size_t) i] == 'x' || steps[(size_t) i] == 'X') bits |= 1u << i;
        else if (steps[(size_t) i] != '.') return false;
    }
    return true;
}

template <class Info>
void drums(Track<DrumPattern>& t, int index, int voices, Info info, const char* text,
           const std::string& at, std::vector<std::string>& problems)
{
    auto& p = t.patterns[index];
    p.clear();

    for (const auto& row : split(text, '|'))
    {
        const auto space = row.find(' ');
        const std::string code = row.substr(0, space);
        const std::string steps = space == std::string::npos ? std::string() : trim(row.substr(space));

        int r = code == "AC" ? DrumPattern::accentRow : -1;
        for (int v = 0; v < voices && r < 0; ++v)
            if (code == info(v).code) r = v;

        uint32_t bits = 0;
        if (r < 0)                       problems.push_back(at + ": no instrument " + code);
        else if (! readRow(steps, bits)) problems.push_back(at + ": " + code + " is not sixteen steps of x and .");
        else                             p.hits[r].store(bits);
    }
}

} // namespace

//==============================================================================

std::vector<std::string> writeGrooves(Rack& rack)
{
    std::vector<std::string> problems;

    for (int b = 0; b < banks; ++b)
    {
        const char letter = (char) ('A' + b);
        for (int n = 0; n < patternsPerBank; ++n)
        {
            const int index = b * patternsPerBank + n;
            const Bank& bank = banksOfGrooves[b];
            mono(rack.mz303.track, index, bank.mz303[n], where(letter, index, "Mz303"), problems);
            mono(rack.mz101.track, index, bank.mz101[n], where(letter, index, "Mz101"), problems);
            drums(rack.mz808.track, index, tr808::Voice::count, tr808::voiceInfo, bank.mz808[n],
                  where(letter, index, "Mz808"), problems);
            drums(rack.mz909.track, index, tr909::Voice::count, tr909::voiceInfo, bank.mz909[n],
                  where(letter, index, "Mz909"), problems);
        }
    }

    // the knobs the grooves were written against
    auto knob = [] (auto& knobs, const char* id, float v) { knobs.set(knobs.indexOf(id), v); };

    knob(rack.mz303.knobs, "cutoff", 3.0f);
    knob(rack.mz303.knobs, "res", 7.5f);
    knob(rack.mz303.knobs, "envMod", 6.5f);
    knob(rack.mz303.knobs, "decay", 4.0f);

    knob(rack.mz101.knobs, "saw", 6.0f);
    knob(rack.mz101.knobs, "pulse", 5.0f);
    knob(rack.mz101.knobs, "cutoff", 5.5f);
    knob(rack.mz101.knobs, "res", 4.0f);
    knob(rack.mz101.knobs, "portaMode", 1.0f);   // AUTO: the slides glide
    knob(rack.mz101.knobs, "portaTime", 2.5f);
    knob(rack.mz101.knobs, "release", 3.5f);

    rack.channel[Rack::M101].level = 0.55f;
    rack.channel[Rack::M101].pan = 0.3f;
    rack.channel[Rack::M909].level = 0.55f;
    rack.channel[Rack::M909].pan = -0.25f;

    // the 303 through the DIST, as it always was, echoes on the leads, and the 808 squeezed
    rack.channel[Rack::M303].dist = true;
    rack.channel[Rack::M303].delay = 0.2f;
    rack.channel[Rack::M101].delay = 0.35f;
    rack.channel[Rack::M808].comp = true;
    knob(rack.fx, "distAmount", 0.4f);

    rack.tempo = 128.0f;
    rack.shuffle = 0.15f;

    return problems;
}

std::vector<std::string> writeSamplerGrooves(Mz950& s)
{
    std::vector<std::string> problems;

    // the kit's keys by sample name
    const auto spans = s.keySpans();
    auto keyOf = [&spans] (const std::string& name)
    {
        for (const auto& sp : spans)
            if (upper(trim(sp.sample)) == upper(name)) return sp.low;
        return -1;
    };

    for (int b = 0; b < banks; ++b)
    {
        for (int n = 0; n < patternsPerBank; ++n)
        {
            const int index = b * patternsPerBank + n;
            const std::string at = where((char) ('A' + b), index, "Mz950");
            auto& p = s.track.patterns[index];
            p.clear();

            for (const auto& row : split(banksOfGrooves[b].mz950[n], '|'))
            {
                const auto eq = row.find('=');
                const std::string name = trim(row.substr(0, eq));
                const std::string steps = eq == std::string::npos ? std::string() : trim(row.substr(eq + 1));

                const int r = name == "AC" ? NotePattern::accentRow : name == "TIE" ? NotePattern::tieRow : keyOf(name);
                uint32_t bits = 0;
                if (r < 0)                       problems.push_back(at + ": no sample " + name + " in the programme");
                else if (! readRow(steps, bits)) problems.push_back(at + ": " + name + " is not sixteen steps of x and .");
                else                             p.hits[r].store(bits);
            }
        }
    }

    return problems;
}

} // namespace debirth
