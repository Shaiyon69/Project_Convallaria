# Synthesised sound effects (stdlib only): writes assets/sfx/<name>.wav, 22050 Hz mono.
# Run from the repo root: python tools/sounds.py
# Each effect is a sum of voices: an oscillator or noise, a pitch sweep, a filter and
# an envelope. Tweak the recipes and rerun.
import math
import random
import struct
import wave

RATE = 22050
OUT = "assets/sfx/"
rng = random.Random(7)


def sweep(f0, f1, t, dur, curve=1.0):
    """Frequency gliding from f0 to f1 over dur (exponential when both > 0)."""
    u = min(1.0, t / dur) ** curve
    return f0 * (f1 / f0) ** u if f0 > 0 and f1 > 0 else f0 + (f1 - f0) * u


def env(t, dur, attack=0.005, power=1.5):
    if t < attack:
        return t / attack
    return max(0.0, 1 - (t - attack) / (dur - attack)) ** power


def osc(kind, phase):
    x = phase % 1.0
    if kind == "sine":
        return math.sin(2 * math.pi * x)
    if kind == "square":
        return 1.0 if x < 0.5 else -1.0
    if kind == "saw":
        return 2 * x - 1
    if kind == "tri":
        return 4 * abs(x - 0.5) - 1
    raise ValueError(kind)


def tone(dur, f0, f1=None, kind="sine", vol=0.5, attack=0.005, power=1.5, vibrato=0, curve=1.0, delay=0.0):
    f1 = f1 or f0
    out, phase = [0.0] * int((dur + delay) * RATE), 0.0
    for i in range(int(dur * RATE)):
        t = i / RATE
        f = sweep(f0, f1, t, dur, curve) * (1 + vibrato * math.sin(2 * math.pi * 6 * t))
        phase += f / RATE
        out[int(delay * RATE) + i] = osc(kind, phase) * env(t, dur, attack, power) * vol
    return out


def noise(dur, vol=0.5, lp=1.0, hp=0.0, attack=0.002, power=1.5, lp_end=None, delay=0.0, grains=0):
    """White noise through a one-pole low-pass (lp: 0..1, gliding to lp_end) and high-pass;
    grains > 0 chops it into that many crackly bursts."""
    out = [0.0] * int((dur + delay) * RATE)
    low = high_prev = high = 0.0
    n = int(dur * RATE)
    for i in range(n):
        t = i / RATE
        a = lp if lp_end is None else lp + (lp_end - lp) * t / dur
        x = rng.uniform(-1, 1)
        low += a * (x - low)
        high = (1 - hp) * (high + low - high_prev) if hp else low
        high_prev = low
        g = 1.0
        if grains:
            g = 1.0 if (int(t / dur * grains * 2) % 2 == 0 and rng.random() < 0.9) else 0.15
        out[int(delay * RATE) + i] = high * env(t, dur, attack, power) * vol * g
    return out


def mix(*parts):
    n = max(len(p) for p in parts)
    out = [0.0] * n
    for p in parts:
        for i, v in enumerate(p):
            out[i] += v
    return out


def am(sig, rate, depth=1.0):
    return [v * (1 - depth + depth * (0.5 + 0.5 * math.sin(2 * math.pi * rate * i / RATE))) for i, v in enumerate(sig)]


def save(name, sig, gain=0.9):
    peak = max(1e-6, max(abs(v) for v in sig))
    with wave.open(OUT + name + ".wav", "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(v / peak * gain * 32767)) for v in sig))


def notes(seq, step, kind, vol=0.4, length=None):
    return mix(*[tone(length or step * 1.6, f, kind=kind, vol=vol, power=2, delay=i * step) for i, f in enumerate(seq)])


SOUNDS = {
    # Petal Blade: an airy swish, bright to dull.
    "swish": lambda: noise(0.18, lp=0.9, lp_end=0.15, hp=0.6, attack=0.03, power=1.2),
    # Woodcutter's Axe: a spinning whirr.
    "whirl": lambda: mix(am(noise(0.32, lp=0.5, hp=0.3, attack=0.02, power=1), 22), tone(0.32, 180, 140, "tri", 0.15)),
    # Seed Mortar launch: a hollow pop.
    "thump": lambda: mix(tone(0.16, 160, 55, "sine", 0.9, power=2), noise(0.03, 0.4, lp=0.6)),
    # Seed pod landing: a soft burst.
    "boom": lambda: mix(noise(0.45, 0.8, lp=0.35, lp_end=0.05, power=2), tone(0.3, 90, 40, "sine", 0.7, power=2)),
    # Lightning: a buzzing crack.
    "zap": lambda: mix(noise(0.22, 0.6, lp=1.0, hp=0.2, grains=9, power=2), tone(0.22, 1400, 300, "saw", 0.25, power=2, vibrato=0.3)),
    # Brambles sprouting: rustling leaves.
    "rustle": lambda: noise(0.35, 0.7, lp=0.5, hp=0.4, grains=6, attack=0.04, power=1),
    # Pumpkin Shell blocking: a hollow knock and ring.
    "block": lambda: mix(tone(0.25, 620, 600, "sine", 0.4, power=3), tone(0.25, 987, 960, "sine", 0.25, power=3),
                         tone(0.1, 200, 120, "sine", 0.6, power=2)),
    # Revive: a rising chime.
    "revive": lambda: mix(notes([523, 659, 784, 1047, 1319], 0.09, "tri", 0.35), noise(0.6, 0.05, lp=1, hp=0.8, attack=0.2)),
    # Freeze: an icy crackle and a falling glint.
    "freeze": lambda: mix(noise(0.3, 0.5, lp=1.0, hp=0.85, grains=7, power=2), tone(0.3, 2600, 1300, "sine", 0.2, power=2)),
    # Unlock: a little fanfare.
    "unlock": lambda: mix(notes([392, 523, 659, 784], 0.1, "square", 0.18, 0.18), tone(0.5, 1047, kind="tri", vol=0.3, power=2, delay=0.4)),
    # Toad leap: a two-pulse croak.
    "croak": lambda: mix(am(tone(0.14, 110, 90, "saw", 0.6, power=1), 45, 0.7), am(tone(0.18, 120, 85, "saw", 0.6, power=1, delay=0.17), 40, 0.7)),
    # Crow swoop: a harsh caw.
    "caw": lambda: mix(am(tone(0.28, 720, 480, "saw", 0.5, power=1.2, vibrato=0.04), 30, 0.5), noise(0.28, 0.15, lp=0.6, hp=0.3, power=1.2)),
    # Mole surfacing: gritty digging.
    "dig": lambda: mix(noise(0.3, 0.8, lp=0.25, grains=5, power=1.2), tone(0.12, 120, 60, "sine", 0.4, power=2)),
    # Sprinkler: a spray with drips.
    "splash": lambda: mix(noise(0.45, 0.6, lp=1.0, hp=0.6, attack=0.02, power=1.5),
                          *[tone(0.06, 900 + 300 * k, 1500 + 300 * k, "sine", 0.2, power=2, delay=0.08 + 0.07 * k) for k in range(4)]),
    # A guardian rises: a low growl.
    "roar": lambda: mix(am(tone(1.0, 75, 55, "saw", 0.6, attack=0.1, power=1, vibrato=0.05), 11, 0.5),
                        noise(1.0, 0.35, lp=0.15, attack=0.1, power=1)),
    # Something heavy lands.
    "thud": lambda: mix(tone(0.22, 95, 38, "sine", 0.9, power=2), noise(0.12, 0.4, lp=0.2, power=2)),

    # Enemy voices, <sprite>_hit (short, it repeats a lot) and <sprite>_die.
    # Shroom: soft spore puffs.
    "shroom_hit": lambda: mix(noise(0.1, 0.7, lp=0.35, hp=0.2, power=2), tone(0.08, 260, 180, "sine", 0.3, power=2)),
    "shroom_die": lambda: mix(noise(0.4, 0.8, lp=0.5, lp_end=0.08, hp=0.15, attack=0.01, power=1.6),
                              tone(0.06, 500, 900, "sine", 0.5, power=2), tone(0.25, 200, 90, "tri", 0.3, power=2, delay=0.05)),
    # Boar: grunts, and a squeal going down.
    "boar_hit": lambda: am(tone(0.14, 150, 105, "saw", 0.6, power=1.2), 32, 0.6),
    "boar_die": lambda: mix(am(tone(0.35, 720, 340, "saw", 0.45, power=1.3, vibrato=0.05), 24, 0.4),
                            am(tone(0.2, 120, 70, "saw", 0.5, power=1.5, delay=0.3), 28, 0.6)),
    # Beetle: chitin clicks, and a crunch.
    "beetle_hit": lambda: mix(noise(0.025, 0.8, lp=1.0, hp=0.7, power=3), noise(0.025, 0.6, lp=1.0, hp=0.7, power=3, delay=0.045),
                              tone(0.05, 1800, 1200, "square", 0.08, power=3)),
    "beetle_die": lambda: mix(noise(0.22, 0.8, lp=0.8, hp=0.4, grains=7, power=1.5),
                              *[noise(0.02, 0.6, lp=1.0, hp=0.8, power=3, delay=0.2 + 0.05 * k) for k in range(3)]),
    # Bee: an angry buzz, then a buzz winding down.
    "bee_hit": lambda: am(tone(0.12, 240, 280, "saw", 0.35, attack=0.01, power=1), 70, 0.5),
    "bee_die": lambda: am(tone(0.4, 300, 70, "saw", 0.4, attack=0.01, power=1.3, curve=0.6), 60, 0.6),
    # Golem: a stone knock, and a crumbling rumble.
    "golem_hit": lambda: mix(tone(0.12, 190, 150, "sine", 0.7, power=2.5), noise(0.08, 0.6, lp=0.25, hp=0.1, power=2)),
    "golem_die": lambda: mix(noise(0.8, 0.9, lp=0.2, lp_end=0.05, grains=10, power=1.2), tone(0.6, 70, 35, "sine", 0.8, power=1.5),
                             *[noise(0.05, 0.5, lp=0.4, power=2, delay=0.15 + 0.13 * k) for k in range(4)]),
    # Hare: squeaks.
    "hare_hit": lambda: tone(0.07, 1300, 1700, "sine", 0.5, power=2),
    "hare_die": lambda: mix(tone(0.22, 1600, 650, "sine", 0.5, power=1.5, vibrato=0.04), tone(0.22, 3200, 1300, "sine", 0.08, power=2)),
    # Toad: a blip, and a croak deflating.
    "toad_hit": lambda: am(tone(0.09, 170, 130, "saw", 0.5, power=1.5), 50, 0.7),
    "toad_die": lambda: mix(am(tone(0.45, 160, 55, "saw", 0.55, power=1.2), 38, 0.7), noise(0.3, 0.15, lp=0.3, power=2, delay=0.15)),
    # Spider: a hiss, and a skitter.
    "spider_hit": lambda: noise(0.1, 0.6, lp=1.0, hp=0.75, attack=0.01, power=1.8),
    "spider_die": lambda: mix(noise(0.25, 0.6, lp=1.0, hp=0.7, attack=0.01, power=1.5),
                              *[noise(0.015, 0.7, lp=1.0, hp=0.6, power=3, delay=0.12 + 0.035 * k) for k in range(6)]),
    # Snail: wet squelches.
    "snail_hit": lambda: mix(am(noise(0.14, 0.6, lp=0.3, power=1.5), 25, 0.8), tone(0.14, 300, 180, "sine", 0.3, power=2, vibrato=0.2)),
    "snail_die": lambda: mix(am(noise(0.5, 0.7, lp=0.35, lp_end=0.1, power=1.3), 18, 0.8),
                             tone(0.5, 380, 110, "sine", 0.4, power=1.5, vibrato=0.15)),
    # Crow: a clipped caw, and a caw with a flutter.
    "crow_hit": lambda: am(tone(0.1, 820, 640, "saw", 0.4, power=1.5), 34, 0.5),
    "crow_die": lambda: mix(am(tone(0.3, 760, 380, "saw", 0.4, power=1.3, vibrato=0.06), 30, 0.5),
                            am(noise(0.35, 0.4, lp=0.5, hp=0.3, power=1.2, delay=0.15), 16, 0.9)),
    # Mole: low squeaks, and one with a puff of dirt.
    "mole_hit": lambda: tone(0.08, 700, 900, "tri", 0.5, power=2),
    "mole_die": lambda: mix(tone(0.2, 900, 420, "tri", 0.5, power=1.5), noise(0.3, 0.6, lp=0.25, grains=5, power=1.5, delay=0.1)),
}

if __name__ == "__main__":
    for name, make in SOUNDS.items():
        save(name, make())
    print("sounds ok")
