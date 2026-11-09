#!/usr/bin/env python3
"""
make_knob.py - flower-shaped volume knob for the EC11 encoder (STL).

    python3 tools/make_knob.py knob.stl

Standard library only. All sizes are parameters below; measure your encoder
and adjust before printing. The knob is one closed, watertight solid:

  - outline: 6 rounded petals, r(t) = R_MID + R_WAVE * cos(6 t)
             (diameter 46 mm at the petals, 30 mm between them)
  - top edge rounded with radius TOP_ROUND
  - underside recess that covers the encoder nut and bushing
  - bore for the 6 mm shaft, closed at the top:
      knurled split shaft (EC11 default): round, slightly undersize, so the
      knurls bite and the split shaft springs against the wall
      D-shaft: set SHAFT_TYPE = "D"
  - small bump on top of one petal: the pointer

Child safety: 46 mm is larger than the 31.7 mm small-parts cylinder, and the
knob should be glued onto the shaft (a drop of CA glue on the flat) so it
cannot be pulled off.
"""

import math
import sys

# ---- Parameters (mm) ------------------------------------------------------
R_MID      = 19.0    # mean outline radius
R_WAVE     = 4.0     # petal depth: radius 15..23
PETALS     = 6
HEIGHT     = 16.0    # total height
TOP_ROUND  = 3.0     # rounding radius of the top edge

RECESS_R   = 7.5     # underside recess: covers the M7 nut (about 11 mm wide)
RECESS_H   = 5.0     # nut + bushing end above the panel + 1 mm air

SHAFT_TYPE = "KNURLED"  # "KNURLED" (18-tooth, split) or "D"
KNURL_R    = 2.9     # knurled: bore 5.8 mm for a 6.0 mm shaft (press fit)
SHAFT_R    = 3.05    # D-shaft: bore radius (6.0 mm + print allowance)
SHAFT_FLAT = 1.55    # D-shaft: centre to flat (4.5 mm across flat -> 1.5)
BORE_H     = 8.0     # bore depth above the recess ceiling (knurl is 9 mm)

POINTER_R  = 2.5     # pointer bump radius
POINTER_H  = 1.2

N          = 180     # samples around the outline

# ---- Contours (polar, sampled at the same angles) -----------------------


def angle(i):
    return 2.0 * math.pi * i / N


def flower(t, inset=0.0):
    return R_MID + R_WAVE * math.cos(PETALS * t) - inset


def d_bore(t):
    if SHAFT_TYPE == "KNURLED":
        return KNURL_R
    s = math.sin(t)
    if s < 0.0:                       # flat on the -y side
        return min(SHAFT_R, SHAFT_FLAT / -s)
    return SHAFT_R


def ring(radius_fn, z):
    return [(radius_fn(angle(i)) * math.cos(angle(i)),
             radius_fn(angle(i)) * math.sin(angle(i)), z) for i in range(N)]


# ---- Mesh helpers ---------------------------------------------------------

tris = []


def quad(a, b, c, d):
    """Quad a-b-c-d, counter-clockwise seen from outside the solid."""
    tris.append((a, b, c))
    tris.append((a, c, d))


def wall(lower, upper, outward):
    """Side wall between two rings; outward=False for hole walls."""
    for i in range(N):
        j = (i + 1) % N
        if outward:
            quad(lower[i], lower[j], upper[j], upper[i])
        else:
            quad(lower[j], lower[i], upper[i], upper[j])


def annulus(outer, inner, facing_up):
    for i in range(N):
        j = (i + 1) % N
        if facing_up:
            quad(inner[j], inner[i], outer[i], outer[j])
        else:
            quad(inner[i], inner[j], outer[j], outer[i])


def cap(rng, centre, facing_up):
    for i in range(N):
        j = (i + 1) % N
        if facing_up:
            tris.append((centre, rng[i], rng[j]))
        else:
            tris.append((centre, rng[j], rng[i]))


# ---- Build the knob -------------------------------------------------------

def build():
    z_top_wall = HEIGHT - TOP_ROUND
    z_ceiling = RECESS_H
    z_bore_top = RECESS_H + BORE_H

    out_bottom = ring(flower, 0.0)
    rec_bottom = ring(lambda t: RECESS_R, 0.0)
    rec_top = ring(lambda t: RECESS_R, z_ceiling)
    bore_bot = ring(d_bore, z_ceiling)
    bore_top = ring(d_bore, z_bore_top)

    annulus(out_bottom, rec_bottom, facing_up=False)       # bottom face
    wall(rec_bottom, rec_top, outward=False)               # recess wall
    annulus(rec_top, bore_bot, facing_up=False)            # recess ceiling
    wall(bore_bot, bore_top, outward=False)                # bore wall
    cap(bore_top, (0.0, 0.0, z_bore_top), facing_up=False) # bore end

    # outer wall, then the rounded top edge in steps
    prev = out_bottom
    steps = 6
    for k in range(steps + 1):
        phi = (math.pi / 2.0) * k / steps
        inset = TOP_ROUND * (1.0 - math.cos(phi))
        z = z_top_wall + TOP_ROUND * math.sin(phi)
        cur = ring(lambda t, s=inset: flower(t, s), z)
        wall(prev, cur, outward=True)
        prev = cur
    cap(prev, (0.0, 0.0, HEIGHT), facing_up=True)

    # pointer: small closed cylinder on top of the petal at t = 90 deg
    px, py = 0.0, flower(math.pi / 2.0) - TOP_ROUND - POINTER_R - 1.0
    m = 32
    lo = [(px + POINTER_R * math.cos(2 * math.pi * i / m),
           py + POINTER_R * math.sin(2 * math.pi * i / m), HEIGHT - 0.5)
          for i in range(m)]
    hi = [(x, y, HEIGHT + POINTER_H) for x, y, _ in lo]
    for i in range(m):
        j = (i + 1) % m
        quad(lo[i], lo[j], hi[j], hi[i])
        tris.append(((px, py, HEIGHT - 0.5), lo[j], lo[i]))
        tris.append(((px, py, HEIGHT + POINTER_H), hi[i], hi[j]))


def normal(a, b, c):
    u = [b[k] - a[k] for k in range(3)]
    v = [c[k] - a[k] for k in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
         u[0] * v[1] - u[1] * v[0])
    length = math.sqrt(sum(x * x for x in n)) or 1.0
    return tuple(x / length for x in n)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "knob.stl"
    build()
    with open(path, "w") as f:
        f.write("solid knob\n")
        for a, b, c in tris:
            f.write("facet normal %.5f %.5f %.5f\n outer loop\n" % normal(a, b, c))
            for p in (a, b, c):
                f.write("  vertex %.4f %.4f %.4f\n" % p)
            f.write(" endloop\nendfacet\n")
        f.write("endsolid knob\n")
    print("%s: %d triangles, diameter %.0f mm, height %.0f mm"
          % (path, len(tris), 2 * (R_MID + R_WAVE), HEIGHT))


if __name__ == "__main__":
    main()
