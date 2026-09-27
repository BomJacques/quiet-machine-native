#pragma once

#include <utility>
#include <vector>

namespace qm::factory
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
    std::vector<int> notes;
};

// Values are native parameter units, not normalised values. Before applying,
// reset editor-owned sound controls and all source gains/mutes, then set master
// gain to 0.65. Notes are chord offsets for both autoNotes and midiChordNotes.
// The first four entries intentionally retain the original sparse overrides.
inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> bank {
        { "Soft foundation", {
            { "oscillatorWave", 2 }, { "tapeDrive", 0.16f }, { "lowpassHz", 1800 },
            { "ampAttack", 1.2f }, { "pianoGain", 0.2f }, { "reverbMix", 0.5f }
        }, { 0, 7, 14, 19 } },
        { "Worn fifths", {
            { "tapeSpeed", 0.72f }, { "tapeWear", 0.5f }, { "wowDepth", 0.5f },
            { "wowRateHz", 0.32f }, { "hissGain", 0.06f }, { "ampRelease", 6 }, { "pianoGain", 0.15f }
        }, { 0, 7, 12, 19 } },
        { "Glass air", {
            { "oscillatorWave", 3 }, { "lowpassHz", 7800 }, { "highpassHz", 240 },
            { "subGain", 0.12f }, { "rumbleGain", 0.1f }, { "tapeWear", 0.02f },
            { "reverbMix", 0.8f }, { "ampAttack", 2 }, { "ampRelease", 8 }
        }, { 0, 11, 16, 23 } },
        { "Low tide", {
            { "oscillatorWave", 3 }, { "masterPitchSemitones", -12 }, { "lowpassHz", 850 },
            { "pianoGain", 0.08f }, { "rumbleGain", 0.3f }, { "tapeSpeed", 0.5f },
            { "ampAttack", 3 }, { "ampRelease", 10 }, { "reverbMix", 0.6f }
        }, { 0, 3, 7, 14 } },

        // Slow triangle bed with an open major ninth and almost no transient layer.
        { "Dawn linen", {
            { "tape1Gain", 0.42f }, { "tape2Gain", 0.18f }, { "tape3Gain", 0.12f }, { "tape4Gain", 0 }, { "tape5Gain", 0.08f },
            { "swellGain", 0.28f }, { "rumbleGain", 0.04f }, { "pianoGain", 0.06f }, { "droneGain", 0.32f }, { "subGain", 0.16f }, { "staticGain", 0 }, { "hissGain", 0.015f },
            { "oscillatorWave", 2 }, { "lowpassHz", 1600 }, { "highpassHz", 65 }, { "filterSweepDepth", 0.08f },
            { "ampAttack", 4.2f }, { "ampDecay", 3 }, { "ampSustain", 0.72f }, { "ampRelease", 9 },
            { "tapeSpeed", 0.82f }, { "tapeAttack", 0.65f }, { "wowRateHz", 0.18f }, { "wowDepth", 0.12f }, { "warpDepth", 0.08f }, { "tapeDrive", 0.08f }, { "reverbMix", 0.62f }
        }, { 0, 4, 11, 14 } },
        // Piano-led, low sustain: individual notes leave long, clean reflections.
        { "Porcelain rain", {
            { "tape1Gain", 0.06f }, { "tape2Gain", 0 }, { "tape3Gain", 0.2f }, { "tape4Gain", 0.08f }, { "tape5Gain", 0 },
            { "swellGain", 0.08f }, { "rumbleGain", 0 }, { "pianoGain", 0.52f }, { "droneGain", 0.08f }, { "subGain", 0.04f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "lowpassHz", 5400 }, { "highpassHz", 180 }, { "filterSweepDepth", 0.03f }, { "glideSeconds", 0.1f },
            { "ampAttack", 0.035f }, { "ampDecay", 2.8f }, { "ampSustain", 0.24f }, { "ampRelease", 7.5f },
            { "tapeSpeed", 1.18f }, { "tapeAttack", 0.08f }, { "wowDepth", 0.04f }, { "warpDepth", 0.02f }, { "tapeWear", 0.01f }, { "tapeDrive", 0.04f }, { "reverbMix", 0.76f }
        }, { 0, 7, 14, 21 } },
        // Low sine/drone emphasis, minor ninth; minimal upper tape activity.
        { "Abyssal choir", {
            { "tape1Gain", 0.12f }, { "tape2Gain", 0 }, { "tape3Gain", 0.08f }, { "tape4Gain", 0 }, { "tape5Gain", 0 },
            { "swellGain", 0.18f }, { "rumbleGain", 0.16f }, { "pianoGain", 0 }, { "droneGain", 0.45f }, { "subGain", 0.28f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "masterPitchSemitones", -12 }, { "lowpassHz", 620 }, { "highpassHz", 35 }, { "filterSweepDepth", 0.02f },
            { "ampAttack", 6 }, { "ampDecay", 4 }, { "ampSustain", 0.78f }, { "ampRelease", 13 },
            { "tapeSpeed", 0.42f }, { "tapeAttack", 0.85f }, { "wowRateHz", 0.09f }, { "wowDepth", 0.1f }, { "warpDepth", 0.06f }, { "tapeDrive", 0.03f }, { "reverbMix", 0.48f }, { "delayEnabled", 0 }
        }, { 0, 7, 15, 26 } },
        // Filtered saw tapes, stronger ageing and a close, dry stereo bed.
        { "Faded postcard", {
            { "tape1Gain", 0.28f }, { "tape2Gain", 0.36f }, { "tape3Gain", 0.05f }, { "tape4Gain", 0.16f }, { "tape5Gain", 0.04f },
            { "swellGain", 0.06f }, { "rumbleGain", 0.03f }, { "pianoGain", 0.18f }, { "droneGain", 0.12f }, { "subGain", 0.06f }, { "staticGain", 0.008f }, { "hissGain", 0.025f },
            { "oscillatorWave", 0 }, { "lowpassHz", 1100 }, { "highpassHz", 160 }, { "lowpassQ", 0.42f }, { "filterSweepDepth", 0.04f },
            { "ampAttack", 0.8f }, { "ampDecay", 2.2f }, { "ampSustain", 0.58f }, { "ampRelease", 4.8f },
            { "tapeSpeed", 0.66f }, { "tapeAttack", 0.44f }, { "wowRateHz", 0.23f }, { "wowDepth", 0.32f }, { "warpDepth", 0.18f }, { "tapeWear", 0.42f }, { "tapeDrive", 0.12f }, { "reverbMix", 0.32f }, { "delayEnabled", 0 }
        }, { 0, 4, 7, 9 } },
        // High sine tapes, no bass or noise, raised register and suspended harmony.
        { "Ice lanterns", {
            { "tape1Gain", 0 }, { "tape2Gain", 0.08f }, { "tape3Gain", 0.3f }, { "tape4Gain", 0.1f }, { "tape5Gain", 0.28f },
            { "swellGain", 0.1f }, { "rumbleGain", 0 }, { "pianoGain", 0.22f }, { "droneGain", 0.1f }, { "subGain", 0 }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "masterPitchSemitones", 12 }, { "lowpassHz", 6200 }, { "highpassHz", 420 }, { "filterSweepDepth", 0 },
            { "ampAttack", 1.4f }, { "ampDecay", 4.5f }, { "ampSustain", 0.42f }, { "ampRelease", 11 },
            { "tapeSpeed", 1.35f }, { "tapeAttack", 0.2f }, { "wowDepth", 0.03f }, { "warpDepth", 0.01f }, { "tapeWear", 0 }, { "tapeDrive", 0.02f }, { "reverbMix", 0.82f }
        }, { 0, 5, 14, 19 } },
        // Midrange square pad softened by a low cutoff and very slow attack.
        { "Reed sanctuary", {
            { "tape1Gain", 0.2f }, { "tape2Gain", 0.1f }, { "tape3Gain", 0.08f }, { "tape4Gain", 0.24f }, { "tape5Gain", 0.06f },
            { "swellGain", 0.22f }, { "rumbleGain", 0.02f }, { "pianoGain", 0 }, { "droneGain", 0.34f }, { "subGain", 0.12f }, { "staticGain", 0 }, { "hissGain", 0.008f },
            { "oscillatorWave", 1 }, { "lowpassHz", 1250 }, { "highpassHz", 100 }, { "lowpassQ", 0.4f }, { "filterSweepDepth", 0.07f },
            { "ampAttack", 5.5f }, { "ampDecay", 3.5f }, { "ampSustain", 0.7f }, { "ampRelease", 8.5f },
            { "tapeSpeed", 0.9f }, { "tapeAttack", 0.75f }, { "wowRateHz", 0.12f }, { "wowDepth", 0.08f }, { "warpDepth", 0.04f }, { "tapeDrive", 0.05f }, { "reverbMix", 0.58f }, { "delayEnabled", 0 }
        }, { 0, 5, 9, 16 } },
        // Swell-led texture with a small noise floor and widely spaced fifths.
        { "Distant surf", {
            { "tape1Gain", 0.1f }, { "tape2Gain", 0.04f }, { "tape3Gain", 0 }, { "tape4Gain", 0.06f }, { "tape5Gain", 0 },
            { "swellGain", 0.44f }, { "rumbleGain", 0.14f }, { "pianoGain", 0 }, { "droneGain", 0.14f }, { "subGain", 0.08f }, { "staticGain", 0.018f }, { "hissGain", 0.03f },
            { "oscillatorWave", 3 }, { "lowpassHz", 1900 }, { "highpassHz", 120 }, { "filterSweepDepth", 0.16f },
            { "ampAttack", 7.5f }, { "ampDecay", 5 }, { "ampSustain", 0.6f }, { "ampRelease", 14 },
            { "tapeSpeed", 0.56f }, { "tapeAttack", 0.9f }, { "wowRateHz", 0.07f }, { "wowDepth", 0.14f }, { "warpDepth", 0.12f }, { "tapeWear", 0.18f }, { "tapeDrive", 0.03f }, { "reverbMix", 0.68f }, { "delayEnabled", 0 }
        }, { 0, 7, 19, 26 } },
        // Slow minor-sixth piano with a dark, short tape backing.
        { "Midnight felt", {
            { "tape1Gain", 0.16f }, { "tape2Gain", 0 }, { "tape3Gain", 0.06f }, { "tape4Gain", 0 }, { "tape5Gain", 0 },
            { "swellGain", 0.04f }, { "rumbleGain", 0.02f }, { "pianoGain", 0.56f }, { "droneGain", 0.12f }, { "subGain", 0.1f }, { "staticGain", 0 }, { "hissGain", 0.012f },
            { "oscillatorWave", 2 }, { "lowpassHz", 1400 }, { "highpassHz", 75 }, { "filterSweepDepth", 0.01f }, { "glideSeconds", 0.04f },
            { "ampAttack", 0.09f }, { "ampDecay", 3.8f }, { "ampSustain", 0.18f }, { "ampRelease", 5.2f },
            { "tapeSpeed", 0.78f }, { "tapeAttack", 0.14f }, { "wowRateHz", 0.28f }, { "wowDepth", 0.16f }, { "warpDepth", 0.05f }, { "tapeWear", 0.22f }, { "tapeDrive", 0.06f }, { "reverbMix", 0.4f }, { "delayEnabled", 0 }
        }, { 0, 3, 9, 14 } },
        // Upper tape lanes against a clean drone, luminous major-seventh voicing.
        { "Orbital windows", {
            { "tape1Gain", 0.04f }, { "tape2Gain", 0.08f }, { "tape3Gain", 0.18f }, { "tape4Gain", 0.3f }, { "tape5Gain", 0.32f },
            { "swellGain", 0.14f }, { "rumbleGain", 0 }, { "pianoGain", 0.05f }, { "droneGain", 0.26f }, { "subGain", 0.04f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "lowpassHz", 4200 }, { "highpassHz", 260 }, { "filterSweepDepth", 0.12f }, { "glideSeconds", 4.4f },
            { "ampAttack", 3.8f }, { "ampDecay", 5.5f }, { "ampSustain", 0.62f }, { "ampRelease", 12 },
            { "tapeSpeed", 1.6f }, { "tapeAttack", 0.55f }, { "wowRateHz", 0.4f }, { "wowDepth", 0.06f }, { "warpDepth", 0.04f }, { "tapeWear", 0.02f }, { "tapeDrive", 0.03f }, { "reverbMix", 0.78f }
        }, { 0, 11, 19, 28 } },
        // Warm low triangle loops, restrained reverb and a stable tonal centre.
        { "Moss chamber", {
            { "tape1Gain", 0.38f }, { "tape2Gain", 0.24f }, { "tape3Gain", 0 }, { "tape4Gain", 0.05f }, { "tape5Gain", 0 },
            { "swellGain", 0.12f }, { "rumbleGain", 0.1f }, { "pianoGain", 0.08f }, { "droneGain", 0.28f }, { "subGain", 0.18f }, { "staticGain", 0 }, { "hissGain", 0.006f },
            { "oscillatorWave", 2 }, { "masterPitchSemitones", -5 }, { "lowpassHz", 920 }, { "highpassHz", 55 }, { "filterSweepDepth", 0.06f },
            { "ampAttack", 2.6f }, { "ampDecay", 2.4f }, { "ampSustain", 0.76f }, { "ampRelease", 6.5f },
            { "tapeSpeed", 0.62f }, { "tapeAttack", 0.6f }, { "wowRateHz", 0.16f }, { "wowDepth", 0.18f }, { "warpDepth", 0.1f }, { "tapeWear", 0.26f }, { "tapeDrive", 0.1f }, { "reverbMix", 0.36f }, { "delayEnabled", 0 }
        }, { 0, 7, 10, 17 } },
        // Sparse decaying sine tapes and piano, with generous delay space.
        { "Silver droplets", {
            { "tape1Gain", 0 }, { "tape2Gain", 0.16f }, { "tape3Gain", 0 }, { "tape4Gain", 0.32f }, { "tape5Gain", 0.12f },
            { "swellGain", 0 }, { "rumbleGain", 0 }, { "pianoGain", 0.38f }, { "droneGain", 0.04f }, { "subGain", 0 }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "masterPitchSemitones", 7 }, { "lowpassHz", 4800 }, { "highpassHz", 320 }, { "filterSweepDepth", 0 }, { "glideSeconds", 0 },
            { "ampAttack", 0.015f }, { "ampDecay", 1.8f }, { "ampSustain", 0.08f }, { "ampRelease", 4.2f },
            { "tapeSpeed", 1.85f }, { "tapeAttack", 0.04f }, { "wowDepth", 0.02f }, { "warpDepth", 0 }, { "tapeWear", 0 }, { "tapeDrive", 0.02f }, { "reverbMix", 0.66f }
        }, { 0, 2, 9, 16 } },
        // Broad, filtered saw swell with a slowly moving low-pass edge.
        { "Velvet horizon", {
            { "tape1Gain", 0.18f }, { "tape2Gain", 0.14f }, { "tape3Gain", 0.18f }, { "tape4Gain", 0.12f }, { "tape5Gain", 0.1f },
            { "swellGain", 0.32f }, { "rumbleGain", 0.03f }, { "pianoGain", 0 }, { "droneGain", 0.3f }, { "subGain", 0.14f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 0 }, { "lowpassHz", 1450 }, { "highpassHz", 90 }, { "lowpassQ", 0.45f }, { "filterSweepDepth", 0.22f },
            { "ampAttack", 8 }, { "ampDecay", 6 }, { "ampSustain", 0.68f }, { "ampRelease", 15 },
            { "tapeSpeed", 0.88f }, { "tapeAttack", 0.82f }, { "wowRateHz", 0.11f }, { "wowDepth", 0.09f }, { "warpDepth", 0.06f }, { "tapeWear", 0.06f }, { "tapeDrive", 0.04f }, { "reverbMix", 0.72f }
        }, { 0, 4, 9, 16 } },
        // Narrow, worn square loops and faint radio texture; little low end.
        { "Paper radio", {
            { "tape1Gain", 0.08f }, { "tape2Gain", 0.3f }, { "tape3Gain", 0.12f }, { "tape4Gain", 0.04f }, { "tape5Gain", 0.16f },
            { "swellGain", 0.04f }, { "rumbleGain", 0 }, { "pianoGain", 0.12f }, { "droneGain", 0.08f }, { "subGain", 0 }, { "staticGain", 0.025f }, { "hissGain", 0.018f },
            { "oscillatorWave", 1 }, { "lowpassHz", 1800 }, { "highpassHz", 650 }, { "lowpassQ", 0.38f }, { "filterSweepDepth", 0.02f },
            { "ampAttack", 0.6f }, { "ampDecay", 1.9f }, { "ampSustain", 0.48f }, { "ampRelease", 3.8f },
            { "tapeSpeed", 1.08f }, { "tapeAttack", 0.28f }, { "wowRateHz", 0.48f }, { "wowDepth", 0.22f }, { "warpDepth", 0.14f }, { "tapeWear", 0.55f }, { "tapeDrive", 0.09f }, { "reverbMix", 0.28f }, { "delayEnabled", 0 }
        }, { 0, 5, 10, 14 } },
        // Held sine harmonics with no piano or noise, nearly motionless tape.
        { "Still water", {
            { "tape1Gain", 0.06f }, { "tape2Gain", 0 }, { "tape3Gain", 0.08f }, { "tape4Gain", 0 }, { "tape5Gain", 0.04f },
            { "swellGain", 0.12f }, { "rumbleGain", 0 }, { "pianoGain", 0 }, { "droneGain", 0.52f }, { "subGain", 0.18f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "lowpassHz", 2400 }, { "highpassHz", 70 }, { "filterSweepDepth", 0 }, { "glideSeconds", 6 },
            { "ampAttack", 9 }, { "ampDecay", 7 }, { "ampSustain", 0.8f }, { "ampRelease", 18 },
            { "tapeSpeed", 0.3f }, { "tapeAttack", 0.95f }, { "wowRateHz", 0.05f }, { "wowDepth", 0.01f }, { "warpDepth", 0 }, { "tapeWear", 0 }, { "tapeDrive", 0 }, { "reverbMix", 0.54f }, { "delayEnabled", 0 }
        }, { 0, 7, 16, 23 } },
        // Faster tape motion, triangle warmth and a gently decaying sixth chord.
        { "Clockwork garden", {
            { "tape1Gain", 0.14f }, { "tape2Gain", 0.32f }, { "tape3Gain", 0.24f }, { "tape4Gain", 0.18f }, { "tape5Gain", 0.08f },
            { "swellGain", 0.04f }, { "rumbleGain", 0 }, { "pianoGain", 0.24f }, { "droneGain", 0.08f }, { "subGain", 0.06f }, { "staticGain", 0 }, { "hissGain", 0.005f },
            { "oscillatorWave", 2 }, { "lowpassHz", 2900 }, { "highpassHz", 140 }, { "filterSweepDepth", 0.09f }, { "glideSeconds", 0.3f },
            { "ampAttack", 0.18f }, { "ampDecay", 2.1f }, { "ampSustain", 0.32f }, { "ampRelease", 4.6f },
            { "tapeSpeed", 1.72f }, { "tapeAttack", 0.12f }, { "wowRateHz", 0.65f }, { "wowDepth", 0.07f }, { "warpDepth", 0.03f }, { "tapeWear", 0.08f }, { "tapeDrive", 0.05f }, { "reverbMix", 0.46f }
        }, { 0, 4, 9, 14 } },
        // Dark low-register tapes with a soft rumble halo and no piano.
        { "Buried observatory", {
            { "tape1Gain", 0.24f }, { "tape2Gain", 0.06f }, { "tape3Gain", 0.18f }, { "tape4Gain", 0.04f }, { "tape5Gain", 0 },
            { "swellGain", 0.16f }, { "rumbleGain", 0.2f }, { "pianoGain", 0 }, { "droneGain", 0.3f }, { "subGain", 0.2f }, { "staticGain", 0.006f }, { "hissGain", 0.008f },
            { "oscillatorWave", 2 }, { "masterPitchSemitones", -12 }, { "lowpassHz", 740 }, { "highpassHz", 45 }, { "lowpassQ", 0.44f }, { "filterSweepDepth", 0.04f },
            { "ampAttack", 4.8f }, { "ampDecay", 5.2f }, { "ampSustain", 0.64f }, { "ampRelease", 11.5f },
            { "tapeSpeed", 0.36f }, { "tapeAttack", 0.72f }, { "wowRateHz", 0.14f }, { "wowDepth", 0.24f }, { "warpDepth", 0.16f }, { "tapeWear", 0.34f }, { "tapeDrive", 0.07f }, { "reverbMix", 0.56f }, { "delayEnabled", 0 }
        }, { 0, 3, 10, 17 } },
        // Elevated, open fourths with a swelling upper-tape wash.
        { "Cloud staircase", {
            { "tape1Gain", 0 }, { "tape2Gain", 0.12f }, { "tape3Gain", 0.22f }, { "tape4Gain", 0.16f }, { "tape5Gain", 0.36f },
            { "swellGain", 0.3f }, { "rumbleGain", 0 }, { "pianoGain", 0.08f }, { "droneGain", 0.18f }, { "subGain", 0 }, { "staticGain", 0 }, { "hissGain", 0.01f },
            { "oscillatorWave", 2 }, { "masterPitchSemitones", 5 }, { "lowpassHz", 3600 }, { "highpassHz", 350 }, { "filterSweepDepth", 0.14f }, { "glideSeconds", 3.2f },
            { "ampAttack", 5 }, { "ampDecay", 4 }, { "ampSustain", 0.56f }, { "ampRelease", 12.5f },
            { "tapeSpeed", 1.24f }, { "tapeAttack", 0.68f }, { "wowRateHz", 0.21f }, { "wowDepth", 0.11f }, { "warpDepth", 0.07f }, { "tapeWear", 0.04f }, { "tapeDrive", 0.04f }, { "reverbMix", 0.8f }
        }, { 0, 5, 12, 17 } },
        // Intimate piano/drone duet, no tape loops, minimal modulation.
        { "Empty reading room", {
            { "tape1Gain", 0 }, { "tape2Gain", 0 }, { "tape3Gain", 0 }, { "tape4Gain", 0 }, { "tape5Gain", 0 },
            { "swellGain", 0.03f }, { "rumbleGain", 0 }, { "pianoGain", 0.48f }, { "droneGain", 0.24f }, { "subGain", 0.08f }, { "staticGain", 0 }, { "hissGain", 0 },
            { "oscillatorWave", 3 }, { "lowpassHz", 2200 }, { "highpassHz", 110 }, { "filterSweepDepth", 0 }, { "glideSeconds", 0.2f },
            { "ampAttack", 0.28f }, { "ampDecay", 4.2f }, { "ampSustain", 0.36f }, { "ampRelease", 6 },
            { "tapeSpeed", 1 }, { "tapeAttack", 0.1f }, { "wowDepth", 0 }, { "warpDepth", 0 }, { "tapeWear", 0 }, { "tapeDrive", 0.02f }, { "reverbMix", 0.24f }, { "delayEnabled", 0 }
        }, { 0, 3, 7, 12 } },
        // Modestly resonant filtered saw, slow glides and a suspended ninth.
        { "Aurora drift", {
            { "tape1Gain", 0.12f }, { "tape2Gain", 0.18f }, { "tape3Gain", 0.28f }, { "tape4Gain", 0.08f }, { "tape5Gain", 0.22f },
            { "swellGain", 0.2f }, { "rumbleGain", 0.02f }, { "pianoGain", 0.04f }, { "droneGain", 0.24f }, { "subGain", 0.1f }, { "staticGain", 0 }, { "hissGain", 0.008f },
            { "oscillatorWave", 0 }, { "lowpassHz", 2600 }, { "highpassHz", 180 }, { "lowpassQ", 0.65f }, { "filterSweepDepth", 0.28f }, { "glideSeconds", 5.2f },
            { "ampAttack", 3.4f }, { "ampDecay", 4.8f }, { "ampSustain", 0.6f }, { "ampRelease", 10.5f },
            { "tapeSpeed", 1.12f }, { "tapeAttack", 0.58f }, { "wowRateHz", 0.3f }, { "wowDepth", 0.15f }, { "warpDepth", 0.09f }, { "tapeWear", 0.12f }, { "tapeDrive", 0.04f }, { "reverbMix", 0.74f }
        }, { 0, 2, 7, 17 } },
        // Sparse low sine octaves beneath a fading piano, long release, no delay.
        { "Last light", {
            { "tape1Gain", 0.2f }, { "tape2Gain", 0 }, { "tape3Gain", 0 }, { "tape4Gain", 0 }, { "tape5Gain", 0.06f },
            { "swellGain", 0.08f }, { "rumbleGain", 0.04f }, { "pianoGain", 0.28f }, { "droneGain", 0.36f }, { "subGain", 0.22f }, { "staticGain", 0 }, { "hissGain", 0.004f },
            { "oscillatorWave", 3 }, { "masterPitchSemitones", -7 }, { "lowpassHz", 1050 }, { "highpassHz", 50 }, { "filterSweepDepth", 0 }, { "glideSeconds", 3.6f },
            { "ampAttack", 2.2f }, { "ampDecay", 8 }, { "ampSustain", 0.3f }, { "ampRelease", 17 },
            { "tapeSpeed", 0.48f }, { "tapeAttack", 0.5f }, { "wowRateHz", 0.08f }, { "wowDepth", 0.05f }, { "warpDepth", 0.02f }, { "tapeWear", 0.08f }, { "tapeDrive", 0.02f }, { "reverbMix", 0.64f }, { "delayEnabled", 0 }
        }, { 0, 7, 12, 24 } }
    };
    return bank;
}
}
