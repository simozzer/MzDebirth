// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Grooves.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <random>
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

    //
    // The mix, set to be heard at the level a record is. An 808 kick starts with one big
    // half-cycle and its body - the weight you hear - sits some 20 dB under it, so left alone
    // that one spike sets how loud everything else can go, and the whole mix plays a dozen dB
    // quieter than a record: quiet enough that the ear, which loses the bass first, hears the
    // kick as thin. So the 808 goes through the DIST as well as the COMP, soft, which shaves
    // the spike and adds the harmonics that let a kick through small speakers too; and with
    // that headroom back, the faders and the MASTER come up, LIMIT catching what's left.
    // About 10 dB louder than before, with a crest of 12 dB - Tests/KickProbe.cpp measures it.
    //
    rack.channel[Rack::M303].level = 0.93f;
    rack.channel[Rack::M101].level = 0.73f;
    rack.channel[Rack::M101].pan = 0.3f;
    rack.channel[Rack::M808].level = 0.93f;
    rack.channel[Rack::M909].level = 0.93f;   // as loud as the 808: in banks B and D it plays the kick
    rack.channel[Rack::M909].pan = -0.25f;
    rack.channel[Rack::M950].level = 0.93f;
    rack.channel[Rack::M7].level = 0.8f;
    rack.channel[Rack::M7].pan = -0.2f;
    rack.channel[Rack::M7].delay = 0.15f;
    rack.channel[Rack::M106].level = 0.7f;
    rack.channel[Rack::M106].pan = 0.2f;
    rack.channel[Rack::M106].delay = 0.3f;
    rack.master = 1.0f;

    // the 303 through the DIST, as it always was, echoes on the leads, and the 808 squeezed
    rack.channel[Rack::M303].dist = true;
    rack.channel[Rack::M303].delay = 0.2f;
    rack.channel[Rack::M101].delay = 0.35f;
    rack.channel[Rack::M808].dist = true;
    rack.channel[Rack::M808].comp = true;
    knob(rack.fx, "distAmount", 0.4f);

    rack.tempo = 128.0f;
    rack.shuffle = 0.15f;

    for (int b = 0; b < banks; ++b)
        rack.bankKey[b] = factoryKey(b).index();

    return problems;
}

Key factoryKey(int bank)
{
    static const Key keys[banks] = { { 5, false }, { 4, false }, { 8, false }, { 6, false } };
    return keys[std::clamp(bank, 0, banks - 1)];
}

namespace
{

// A sampler pattern, written from its text, the kit's keys found by their sample names.
void sampler(const std::vector<Mz950::Span>& spans, NotePattern& p, const char* text,
             const std::string& at, std::vector<std::string>& problems)
{
    auto keyOf = [&spans] (const std::string& name)
    {
        for (const auto& sp : spans)
            if (upper(trim(sp.sample)) == upper(name)) return sp.low;
        return -1;
    };

    p.clear();
    for (const auto& row : split(text, '|'))
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

} // namespace

std::vector<std::string> writeSamplerGrooves(Mz950& s)
{
    std::vector<std::string> problems;
    const auto spans = s.keySpans();

    for (int b = 0; b < banks; ++b)
        for (int n = 0; n < patternsPerBank; ++n)
        {
            const int index = b * patternsPerBank + n;
            sampler(spans, s.track.patterns[index], banksOfGrooves[b].mz950[n],
                    where((char) ('A' + b), index, "Mz950"), problems);
        }

    return problems;
}

//============================================================================== grooves in a key

const char* styleName(int style)
{
    static const char* const names[Style::count] = { "ACID", "TECHNO", "ELECTRO", "DEEP" };
    return names[std::clamp(style, 0, Style::count - 1)];
}

namespace
{

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// the key's tonic as near C as it can be: -6 to +5 semitones
int tonicOffset(const Key& k) { return k.root() <= 5 ? k.root() : k.root() - 12; }

// a degree of the key's scale, in any octave, in semitones from its tonic
int degree(const Key& k, int d)
{
    const int octave = floorDiv(d, 7);
    return k.scale()[d - 7 * octave] + 12 * octave;
}

// a step so many semitones from its machine's root note, folded into the -12 to +24 a step
// can reach
BassStep at(BassStep s, int semitones)
{
    while (semitones < -12) semitones += 12;
    while (semitones > 24) semitones -= 12;
    s.octave = semitones < 0 ? -1 : (semitones > 12 ? 1 : 0);
    s.note = semitones - 12 * s.octave;
    return s;
}

int semitonesOf(const BassStep& s) { return s.note + 12 * s.octave; }

// a 950 programme that is a kit - mostly keygroups of one key each - rather than a tuned sound
bool isKit(const std::vector<Mz950::Span>& spans)
{
    int single = 0;
    for (const auto& s : spans) single += s.low == s.high ? 1 : 0;
    return ! spans.empty() && single * 2 >= (int) spans.size();
}

//
// A pitch moved from one key into another, degree for degree; a note between degrees stays
// a semitone above the one under it. Pitches and tonics are absolute.
//
int movePitch(int pitch, int fromTonic, const Key& from, int toTonic, const Key& to)
{
    const int rel = pitch - fromTonic;
    const int octave = floorDiv(rel, 12);
    const int pc = rel - 12 * octave;

    for (int d = 0; d < 7; ++d)
        if (from.scale()[d] == pc) return toTonic + 12 * octave + to.scale()[d];
    for (int d = 0; d < 7; ++d)
        if (from.scale()[d] == pc - 1) return toTonic + 12 * octave + to.scale()[d] + 1;
    return toTonic + 12 * octave + pc;
}

// the shorter way from one tonic to another: -6 to +5 semitones
int shift(const Key& from, const Key& to)
{
    const int d = ((to.root() - from.root()) % 12 + 12) % 12;
    return d > 5 ? d - 12 : d;
}

struct Random
{
    explicit Random(unsigned seed) : g(seed) {}
    float uniform() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(g); }
    bool chance(float p) { return uniform() < p; }
    int below(int n) { return std::uniform_int_distribution<int>(0, n - 1)(g); }
    std::mt19937 g;
};

enum class Role { Groove, Fill, Breakdown };

Role roleOf(int slot) { return slot == 3 || slot == 7 ? Role::Fill : slot == 4 ? Role::Breakdown : Role::Groove; }

// the chord each slot is built on, as a degree: i i VI - i iv VII - in minor, I I IV - I vi V - in major
int chordOf(const Key& k, int slot)
{
    static const int minor[patternsPerBank] = { 0, 0, 5, 0, 0, 3, 6, 0 };
    static const int major[patternsPerBank] = { 0, 0, 3, 0, 0, 5, 4, 0 };
    return (k.major ? major : minor)[slot];
}

// a line: each step's pitch in semitones from the key's tonic, its gate, accent and slide
struct Line
{
    bool gate[stepsPerPattern] {};
    int pitch[stepsPerPattern] {};
    bool accent[stepsPerPattern] {};
    bool slide[stepsPerPattern] {};
};

void write(BassPattern& p, const Line& l, int tonic)
{
    p.clear();
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        BassStep st;
        st.gate = l.gate[s];
        st.accent = l.gate[s] && l.accent[s];
        st.slide = l.gate[s] && l.slide[s] && s + 1 < stepsPerPattern && l.gate[s + 1];
        p.set(s, l.gate[s] ? at(st, tonic + l.pitch[s]) : st);
    }
}

// a chord tone or a passing note above a chord's root, the root most of all
int chordTone(const Key& k, int chord, Random& r)
{
    // degrees above the chord's root, and how often each is picked, out of a hundred
    static const int steps[]   = { 0, 4, 2, 7, 6, 1, 3 };
    static const int weights[] = { 50, 14, 10, 10, 8, 4, 4 };
    int pick = r.below(100);
    for (int i = 0; i < 7; ++i)
    {
        if (pick < weights[i]) return degree(k, chord + steps[i]);
        pick -= weights[i];
    }
    return degree(k, chord);
}

//
// A bass line in a style over a chord. Given a line already, it keeps that line's rhythm,
// so the grooves of a bank sound like one another.
//
Line bassLine(const Key& k, int style, int chord, const Line* rhythm, Random& r)
{
    static const char* const electro[] = { "x.xx..x.x..x.xx.", "x..x..x...x.x...", "x.x...xx..x..x.." };
    static const char* const deep[]    = { "x..x..x.x..x.x..", "x.....x...x..x..", "x..x....x.x...x." };
    static const int rolls[][4]        = { { -12, 0, 0, 12 }, { -12, 0, 12, 0 }, { -12, -12, 0, 12 } };

    const char* tmpl = style == Style::Electro ? electro[r.below(3)] : style == Style::Deep ? deep[r.below(3)] : nullptr;
    const int* roll = rolls[r.below(3)];

    Line l;
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        if (rhythm != nullptr)            l.gate[s] = rhythm->gate[s];
        else if (tmpl != nullptr)         l.gate[s] = tmpl[s] == 'x';
        else if (style == Style::Techno)  l.gate[s] = true;
        else                              l.gate[s] = s == 0 || r.chance(0.78f);

        if (style == Style::Techno)
        {
            // rolling sixteenths, the root jumping octaves, now and then the fifth
            l.pitch[s] = degree(k, chord + (r.chance(0.15f) ? 4 : 0)) + roll[s % 4];
            l.accent[s] = s % 4 == 2 && r.chance(0.5f);
            l.slide[s] = r.chance(0.04f);
        }
        else
        {
            l.pitch[s] = chordTone(k, chord, r);
            if (style == Style::Acid)          l.pitch[s] += r.chance(0.2f) ? 12 : (r.chance(0.1f) ? -12 : 0);
            else if (style == Style::Electro)  l.pitch[s] += r.chance(0.35f) ? -12 : 0;
            else                               l.pitch[s] -= r.chance(0.5f) ? 12 : 0;

            const float accent = style == Style::Acid ? 0.3f : style == Style::Electro ? 0.2f : 0.05f;
            const float slide = style == Style::Acid ? 0.25f : style == Style::Electro ? 0.1f : 0.35f;
            l.accent[s] = r.chance(accent);
            l.slide[s] = r.chance(slide);
        }
    }

    // the bar starts on the chord's root
    l.gate[0] = true;
    l.pitch[0] = degree(k, chord) - (style == Style::Deep || style == Style::Techno ? 12 : 0);
    l.accent[0] = style != Style::Deep;
    return l;
}

// a line with a few of its notes changed, for a variation that is still the same groove
Line varied(const Line& l, const Key& k, int chord, Random& r)
{
    Line v = l;
    for (int n = 0; n < 3; ++n)
    {
        const int s = 1 + r.below(stepsPerPattern - 1);
        if (v.gate[s]) v.pitch[s] = chordTone(k, chord, r) + (r.chance(0.25f) ? 12 : 0);
        v.accent[s] = r.chance(0.3f);
    }
    return v;
}

// a line's second half climbing the scale to the next bar, accented
Line fill(const Line& l, const Key& k, int chord, bool high)
{
    Line f = l;
    for (int s = 8; s < stepsPerPattern; ++s)
    {
        f.gate[s] = true;
        f.pitch[s] = degree(k, chord + (s - 8)) + (high ? 12 : 0);
        f.accent[s] = s % 2 == 0;
        f.slide[s] = s >= 12;
    }
    return f;
}

// a breakdown's line: the root on the bar and half way, then the fifth, and nothing else
Line sparse(const Key& k, int chord)
{
    Line l;
    l.gate[0] = l.gate[8] = l.gate[14] = true;
    l.pitch[0] = l.pitch[8] = degree(k, chord) - 12;
    l.pitch[14] = degree(k, chord + 4) - 12;
    return l;
}

//
// A lead over a chord, in a style: acid answers in a phrase, techno stabs, electro runs an
// arpeggio, deep drops a dub stab or two for the delay to carry.
//
Line leadLine(const Key& k, int style, int chord, Random& r)
{
    Line l;
    auto tone = [&] (int i) { static const int t[] = { 0, 2, 4, 7 }; return degree(k, chord + t[i % 4]) + 12 * (i / 4); };

    switch (style)
    {
        case Style::Acid:
        {
            static const int spots[] = { 2, 3, 6, 7, 10, 11, 13, 14 };
            const int notes = 4 + r.below(3);
            for (int n = 0; n < notes; ++n)
            {
                const int s = spots[r.below(8)];
                l.gate[s] = true;
                l.pitch[s] = tone(r.below(5));
                l.slide[s] = r.chance(0.35f);
            }
            break;
        }
        case Style::Techno:
        {
            static const char* const stabs[] = { "x.....x.....x...", "....x.......x...", "..x...x...x.....", "x.......x.....x." };
            const char* t = stabs[r.below(4)];
            const int p = tone(r.chance(0.6f) ? 4 : 2);
            for (int s = 0; s < stepsPerPattern; ++s)
                if (t[s] == 'x') { l.gate[s] = true; l.pitch[s] = p; }
            break;
        }
        case Style::Electro:
        {
            // up, up and down, or a broken order through the chord, over two octaves
            static const int orders[][8] = { { 0, 1, 2, 3, 4, 3, 2, 1 }, { 0, 1, 2, 3, 2, 1, 0, 1 }, { 0, 2, 1, 3, 2, 4, 3, 1 } };
            const int* o = orders[r.below(3)];
            for (int s = 0; s < stepsPerPattern; ++s) { l.gate[s] = true; l.pitch[s] = tone(o[s % 8]) - 12; }
            break;
        }
        default:
        {
            static const int spots[] = { 2, 10, 14 };
            for (int s : spots)
                if (s != 14 || r.chance(0.5f)) { l.gate[s] = true; l.pitch[s] = tone(1 + r.below(3)); }
            break;
        }
    }
    return l;
}

//
// A drum pattern moved about a little: on half the rows that are not the beat itself, one
// off-beat sixteenth turned on or off.
//
void loosen(DrumPattern& p, int voices, const std::function<bool(int)>& isTheBeat, Random& r)
{
    for (int v = 0; v < voices; ++v)
    {
        if (isTheBeat(v) || p.rowIsEmpty(v) || ! r.chance(0.5f)) continue;
        const int s = 1 + 2 * r.below(stepsPerPattern / 2);
        p.set(v, s, ! p.get(v, s));
    }
}

bool isTheBeat(const char* code)
{
    const std::string c(code);
    return c == "BD" || c == "SD" || c == "CP";
}

} // namespace

Key detectKey(Rack& rack, int bank)
{
    double weights[12] {};
    auto count = [&weights] (Track<BassPattern>& t, int root, int index)
    {
        const auto& p = t.patterns[index];
        for (int s = 0; s < p.len(); ++s)
        {
            const BassStep st = p.get(s);
            if (! st.gate) continue;
            const int pc = ((root + semitonesOf(st)) % 12 + 12) % 12;
            weights[pc] += 1.0 + (st.accent ? 0.5 : 0.0) + (s % 4 == 0 ? 1.0 : 0.0);
        }
    };

    for (int n = 0; n < patternsPerBank; ++n)
    {
        count(rack.mz303.track, rack.mz303.rootNote, bank * patternsPerBank + n);
        count(rack.mz101.track, rack.mz101.rootNote, bank * patternsPerBank + n);
    }
    return likeliestKey(weights);
}

void transposeBank(Rack& rack, int bank, Key from, Key to)
{
    const int d = shift(from, to);

    auto move = [&] (Track<BassPattern>& t, int root)
    {
        const int fromTonic = root + tonicOffset(from);
        for (int n = 0; n < patternsPerBank; ++n)
        {
            auto& p = t.patterns[bank * patternsPerBank + n];
            for (int s = 0; s < stepsPerPattern; ++s)
            {
                const BassStep st = p.get(s);
                if (! st.gate) continue;
                const int pitch = movePitch(root + semitonesOf(st), fromTonic, from, fromTonic + d, to);
                p.set(s, at(st, pitch - root));
            }
        }
    };
    move(rack.mz303.track, rack.mz303.rootNote);
    move(rack.mz101.track, rack.mz101.rootNote);

    // the 950's notes, if what it plays is tuned: a kit's keys are its sounds, not notes
    const auto spans = rack.mz950.keySpans();
    if (! spans.empty() && ! isKit(spans))
    {
        const int fromTonic = 60 + tonicOffset(from);
        for (int n = 0; n < patternsPerBank; ++n)
        {
            auto& p = rack.mz950.track.patterns[bank * patternsPerBank + n];
            uint32_t moved[NotePattern::notes] {};
            for (int note = 0; note < NotePattern::notes; ++note)
            {
                const uint32_t bits = p.hits[note].load();
                if (bits == 0) continue;
                const int into = std::clamp(movePitch(note, fromTonic, from, fromTonic + d, to), 0, NotePattern::notes - 1);
                moved[into] |= bits;
            }
            for (int note = 0; note < NotePattern::notes; ++note) p.hits[note].store(moved[note]);
        }
    }

    // the Mz7's and the Mz106's notes, always tuned
    for (auto* track : { &rack.mz7.track, &rack.mz106.track })
    {
        const int fromTonic = 60 + tonicOffset(from);
        for (int n = 0; n < patternsPerBank; ++n)
        {
            auto& p = track->patterns[bank * patternsPerBank + n];
            uint32_t moved[NotePattern::notes] {};
            for (int note = 0; note < NotePattern::notes; ++note)
            {
                const uint32_t bits = p.hits[note].load();
                if (bits == 0) continue;
                const int into = std::clamp(movePitch(note, fromTonic, from, fromTonic + d, to), 0, NotePattern::notes - 1);
                moved[into] |= bits;
            }
            for (int note = 0; note < NotePattern::notes; ++note) p.hits[note].store(moved[note]);
        }
    }

    rack.bankKey[bank] = to.index();
}

void generateBank(Rack& rack, int bank, Key k, int style, unsigned seed)
{
    style = std::clamp(style, 0, Style::count - 1);
    Random r(seed);
    std::vector<std::string> problems;               // the factory patterns read; the check makes sure
    const Bank& templates = banksOfGrooves[style];   // each style's drums start from its factory bank

    // the bass line every groove of the bank is built from, and the lead's
    const Line base = bassLine(k, style, 0, nullptr, r);
    const Line lead = leadLine(k, style, 0, r);

    const int tonic = tonicOffset(k);   // from each machine's own root note
    const auto spans = rack.mz950.keySpans();
    const bool kit = isKit(spans);

    for (int slot = 0; slot < patternsPerBank; ++slot)
    {
        const int index = bank * patternsPerBank + slot;
        const int chord = chordOf(k, slot);
        const Role role = roleOf(slot);
        const std::string name = where((char) ('A' + bank), index, "generated");

        // the bass line and the lead
        Line bass, top;
        switch (role)
        {
            case Role::Groove:
                bass = slot == 0 ? base
                     : chord == 0 ? varied(base, k, chord, r)
                     : varied(bassLine(k, style, chord, &base, r), k, chord, r);
                top = slot == 0 ? lead
                    : slot == 1 ? Line {}                                  // the bass on its own
                    : leadLine(k, style, chord, r);
                break;
            case Role::Fill:
                bass = fill(base, k, chord, false);
                if (style == Style::Acid || style == Style::Electro)
                {
                    top = fill(Line {}, k, chord, true);
                    for (int s = 0; s < 8; ++s) top.gate[s] = false;
                }
                break;
            case Role::Breakdown:
                bass = sparse(k, chord);
                top = leadLine(k, Style::Acid, chord, r);   // the lead, alone with little under it, sings
                break;
        }
        write(rack.mz303.track.patterns[index], bass, tonic);
        write(rack.mz101.track.patterns[index], top, tonic);

        // the drums: the style's factory pattern in the same slot, moved about in a groove
        drums(rack.mz808.track, index, tr808::Voice::count, tr808::voiceInfo, templates.mz808[slot], name, problems);
        drums(rack.mz909.track, index, tr909::Voice::count, tr909::voiceInfo, templates.mz909[slot], name, problems);
        if (role == Role::Groove)
        {
            loosen(rack.mz808.track.patterns[index], tr808::Voice::count,
                   [] (int v) { return isTheBeat(tr808::voiceInfo(v).code); }, r);
            loosen(rack.mz909.track.patterns[index], tr909::Voice::count,
                   [] (int v) { return isTheBeat(tr909::voiceInfo(v).code); }, r);
        }

        // the Mz7 comps the slot's chord round middle C - stabs in most styles, a pad in Deep
        // and the breakdown, nothing in a fill - whatever voice it has
        {
            auto& q = rack.mz7.track.patterns[index];
            q.clear();
            if (role != Role::Fill)
            {
                const int home = 60 - ((60 - k.root()) % 12 + 12) % 12;
                int notes[3];
                for (int i = 0; i < 3; ++i)
                    notes[i] = std::clamp(home + degree(k, chord + 2 * i), 0, NotePattern::notes - 1);

                static const char* const stabs[Style::count] = { "....x.......x...", "..x...x...x...x.", "x.....x...x.....", "x.......x......." };
                const char* h = role == Role::Breakdown ? "x..............." : stabs[style];
                const bool pad = style == Style::Deep || role == Role::Breakdown;
                for (int s = 0; s < stepsPerPattern; ++s)
                {
                    if (h[s] != 'x' && ! pad) continue;
                    for (int n : notes) q.set(n, s, true);
                    if (h[s] != 'x') q.set(NotePattern::tieRow, s, true);
                }
            }
        }

        // the Mz106 arpeggiates the slot's chord over two octaves from middle C, as its own
        // arpeggiator would - sixteenths, eighths in Deep, every other eighth in the breakdown,
        // nothing in a fill
        {
            auto& q = rack.mz106.track.patterns[index];
            q.clear();
            if (role != Role::Fill)
            {
                const int home = 60 - ((60 - k.root()) % 12 + 12) % 12;
                const int every = role == Role::Breakdown ? 4 : style == Style::Deep ? 2 : 1;
                for (int s = 0, i = 0; s < stepsPerPattern; s += every, ++i)
                {
                    const int up = i % 6;                       // up through the chord twice
                    const int n = home + 12 * (up / 3) + degree(k, chord + 2 * (up % 3));
                    q.set(std::clamp(n, 0, NotePattern::notes - 1), s, true);
                }
            }
        }

        // the 950: a kit plays the style's percussion; a tuned programme the slot's chord
        auto& p = rack.mz950.track.patterns[index];
        if (kit)
        {
            sampler(spans, p, templates.mz950[slot], name, problems);
            continue;
        }

        p.clear();
        if (spans.empty() || role == Role::Fill) continue;

        // the chord's three notes, round the middle of the keys the programme covers
        const int middle = (spans.front().low + spans.back().high) / 2;
        const int home = middle - ((middle - k.root()) % 12 + 12) % 12;
        int notes[3];
        for (int i = 0; i < 3; ++i)
            notes[i] = std::clamp(home + degree(k, chord + 2 * i), 0, NotePattern::notes - 1);

        static const char* const hits[Style::count] = { "..x.......x.....", "..x...x...x...x.", "x..x..x...x.....", "x.......x......." };
        const char* h = role == Role::Breakdown ? "x..............." : hits[style];
        const bool held = style == Style::Deep || role == Role::Breakdown;   // a pad, tied on between its hits

        for (int s = 0; s < stepsPerPattern; ++s)
        {
            if (h[s] != 'x' && ! held) continue;
            for (int n : notes) p.set(n, s, true);
            if (h[s] != 'x') p.set(NotePattern::tieRow, s, true);
        }
    }

    rack.bankKey[bank] = k.index();
}

} // namespace debirth
