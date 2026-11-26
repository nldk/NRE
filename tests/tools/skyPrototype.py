#!/usr/bin/env python3
"""Offline prototype of the procedural sky (single scattering Rayleigh + Mie + ozone absorption).

Settles the constants and the auto-exposure curve used by LightingMath (lightingMath.h) and by the Task 8 GPU
shaders. Run: python3 tests/tools/skyPrototype.py [output.png]. Needs numpy and Pillow.
All lengths are kilometres. Sun azimuth is the image centre; y is up.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw

# ---- Atmosphere constants (keep identical to NRE::LightingMath::Atmosphere in lightingMath.h) ----
PLANET_RADIUS = 6360.0            # km
ATMOSPHERE_TOP = 6460.0           # km (radius of the top of the atmosphere)
VIEWER_HEIGHT = 0.0015            # km (1.5 m)
RAYLEIGH_SCATTER = np.array([5.802e-3, 13.558e-3, 33.1e-3])  # 1/km at sea level
RAYLEIGH_SCALE_HEIGHT = 8.0       # km
MIE_SCATTER = 3.996e-3            # 1/km at sea level
MIE_ABSORB = 4.40e-3              # 1/km at sea level
MIE_SCALE_HEIGHT = 1.2            # km
MIE_G = 0.8                       # Cornette-Shanks asymmetry
OZONE_ABSORB = np.array([0.650e-3, 1.881e-3, 0.085e-3])     # 1/km at the layer centre
OZONE_CENTER = 25.0               # km
OZONE_HALF_WIDTH = 15.0           # km (tent profile)
TRANSMITTANCE_STEPS = 64          # midpoint steps, quadratic spacing (same in C++)
SUN_ANGULAR_RADIUS = np.radians(0.2675)  # 0.535 deg diameter
# ---- Look constants ----
SKY_SCALE = 10.0                   # multiplies single-scatter radiance (stand-in for multiple scattering)
NIGHT_FLOOR = np.array([0.0025, 0.0045, 0.0100])   # linear radiance of the night sky at intensity 3
GROUND_ALBEDO = 0.2
SUN_INTENSITY = 3.0               # setSun intensity: 3 is the standard look
# Sun light fade at the horizon (SUN_FADE_*_DEG match Atmosphere::sunFade*Deg)
SUN_FADE_LOW_DEG = -0.95
SUN_FADE_HIGH_DEG = 0.0
# autoExposure(elevation): 1 at >= 30 deg, exp(ln(MAX) * smoothstep) down to -10 deg and below
EXPOSURE_MAX = 5.0
EXPOSURE_HIGH_DEG = 30.0
EXPOSURE_LOW_DEG = -10.0


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def auto_exposure(elev):
    t = smoothstep(EXPOSURE_HIGH_DEG, EXPOSURE_LOW_DEG, np.degrees(elev))
    return float(np.exp(np.log(EXPOSURE_MAX) * t))


def densities(h):
    r = np.exp(-h / RAYLEIGH_SCALE_HEIGHT)
    m = np.exp(-h / MIE_SCALE_HEIGHT)
    o = np.maximum(0.0, 1.0 - np.abs(h - OZONE_CENTER) / OZONE_HALF_WIDTH)
    return r, m, o


def extinction_sum(h):
    """Per-channel extinction coefficient (1/km) at height h (arrays broadcast to h.shape + (3,))."""
    r, m, o = densities(h)
    return (r[..., None] * RAYLEIGH_SCATTER + m[..., None] * (MIE_SCATTER + MIE_ABSORB) + o[..., None] * OZONE_ABSORB)


def ray_sphere_exit(py, dy, radius):
    """Distance along a ray from (0, py) with y-component dy (and horizontal component sqrt(1-dy^2)) to a sphere of
    `radius` about the origin: returns (t_near, t_far) with nan when missed. Ray start is inside/outside generic."""
    # |p + t d|^2 = R^2 with p=(0,py), p.d = py*dy
    b = py * dy
    c = py * py - radius * radius
    disc = b * b - c
    hit = disc >= 0
    s = np.sqrt(np.where(hit, disc, 0.0))
    return np.where(hit, -b - s, np.nan), np.where(hit, -b + s, np.nan)


def optical_depth(py, dy, n=TRANSMITTANCE_STEPS):
    """Optical depth (3,) channels from a point at radial position (0, py) along a direction whose component along
    the radial axis is dy, up to the top of the atmosphere. Returns (depth, blocked_by_planet). Vectorised on py, dy."""
    py = np.asarray(py, dtype=np.float64)
    dy = np.asarray(dy, dtype=np.float64)
    _, t_top = ray_sphere_exit(py, dy, ATMOSPHERE_TOP)
    t_planet, _ = ray_sphere_exit(py, dy, PLANET_RADIUS)
    blocked = (~np.isnan(t_planet)) & (t_planet > 0)
    depth = np.zeros(py.shape + (3,))
    u = (np.arange(n) + 0.5) / n
    for ui in u:
        t = t_top * ui * ui
        dt = t_top * 2.0 * ui / n
        # position radius: p + t d, with horizontal comp = t*sqrt(1-dy^2) (p is purely radial)
        px = t * np.sqrt(np.maximum(1 - dy * dy, 0.0))
        pr = np.sqrt(px * px + (py + t * dy) ** 2)
        h = np.maximum(pr - PLANET_RADIUS, 0.0)
        depth += extinction_sum(h) * dt[..., None]
    return depth, blocked


def sun_transmittance(elev):
    """RGB transmittance of the sun's light reaching the viewer (same as LightingMath::sunTransmittance): optical
    depth at max(elevation, 0), times a smoothstep fade from 0 at SUN_FADE_LOW_DEG to 1 at SUN_FADE_HIGH_DEG."""
    py = PLANET_RADIUS + VIEWER_HEIGHT
    fade = float(smoothstep(SUN_FADE_LOW_DEG, SUN_FADE_HIGH_DEG, np.degrees(elev)))
    if fade <= 0.0:
        return np.zeros(3)
    depth, _ = optical_depth(np.array(py), np.array(np.sin(max(elev, 0.0))))
    return fade * np.exp(-depth)


def render(elev, width=512, height=256, view_steps=32, light_steps=8):
    az = (np.arange(width) + 0.5) / width * 2 * np.pi - np.pi
    el = np.pi / 2 - (np.arange(height) + 0.5) / height * np.pi
    AZ, EL = np.meshgrid(az, el)
    d = np.stack([np.cos(EL) * np.sin(AZ), np.sin(EL), np.cos(EL) * np.cos(AZ)], -1).reshape(-1, 3)
    sun = np.array([0.0, np.sin(elev), np.cos(elev)])
    cos_sun = d @ sun
    n = d.shape[0]
    p0 = np.array([0.0, PLANET_RADIUS + VIEWER_HEIGHT, 0.0])
    # ray vs atmosphere / planet in 3D
    b = d @ p0
    c = p0 @ p0
    t_top = -b + np.sqrt(b * b - (c - ATMOSPHERE_TOP ** 2))
    disc_p = b * b - (c - PLANET_RADIUS ** 2)
    hits_planet = (disc_p >= 0) & (-b - np.sqrt(np.maximum(disc_p, 0)) > 0)
    t_planet = np.where(hits_planet, -b - np.sqrt(np.maximum(disc_p, 0)), t_top)
    t_max = np.minimum(t_top, t_planet)

    rayleigh_phase = 3.0 / (16.0 * np.pi) * (1.0 + cos_sun ** 2)
    g = MIE_G
    mie_phase = 3.0 / (8.0 * np.pi) * ((1 - g * g) * (1 + cos_sun ** 2)) / ((2 + g * g) * (1 + g * g - 2 * g * cos_sun) ** 1.5)

    radiance = np.zeros((n, 3))
    view_depth = np.zeros((n, 3))
    for i in range(view_steps):
        u0, u1 = i / view_steps, (i + 1) / view_steps
        um = 0.5 * (u0 + u1)
        t = t_max * um * um
        dt = t_max * (u1 * u1 - u0 * u0)
        pos = p0 + t[:, None] * d
        pr = np.linalg.norm(pos, axis=1)
        h = np.maximum(pr - PLANET_RADIUS, 0.0)
        r, m, o = densities(h)
        ext = extinction_sum(h)
        view_depth_mid = view_depth + ext * (0.5 * dt)[:, None]
        # sun transmittance at the sample
        up = pos / pr[:, None]
        mu = up @ sun
        ld, blocked = optical_depth(pr, mu, n=light_steps)
        t_sun = np.where(blocked[:, None], 0.0, np.exp(-ld))
        scat = (r[:, None] * RAYLEIGH_SCATTER * rayleigh_phase[:, None] + m[:, None] * MIE_SCATTER * mie_phase[:, None])
        radiance += scat * t_sun * np.exp(-view_depth_mid) * dt[:, None]
        view_depth += ext * dt[:, None]
    radiance *= SKY_SCALE * (SUN_INTENSITY / 3.0)

    # ground
    ground_pos = p0 + t_max[:, None] * d
    gup = ground_pos / np.linalg.norm(ground_pos, axis=1)[:, None]
    gmu = gup @ sun
    gld, gblocked = optical_depth(np.linalg.norm(ground_pos, axis=1), gmu, n=light_steps)
    g_sun = np.where(gblocked[:, None], 0.0, np.exp(-gld)) * SUN_INTENSITY * np.maximum(gmu, 0)[:, None] / np.pi
    g_amb = 0.08 * np.array([0.5, 0.7, 1.0]) * (SUN_INTENSITY / 3.0) * max(np.sin(elev) + 0.15, 0.0)
    ground = GROUND_ALBEDO * (g_sun + g_amb) * np.exp(-view_depth)
    radiance += np.where(hits_planet[:, None], ground, 0.0)

    # sun disk (area-weighted over one pixel), radiance = E / solid angle * transmittance
    ang = np.arccos(np.clip(cos_sun, -1, 1))
    pix = 2 * np.pi / width
    cover = np.clip((SUN_ANGULAR_RADIUS - ang) / pix + 0.5, 0, 1) * (~hits_planet)
    disk = sun_transmittance(elev) * SUN_INTENSITY / (np.pi * SUN_ANGULAR_RADIUS ** 2)
    radiance += cover[:, None] * disk[None, :]
    # night floor (fades in only where the scattered sky is dim anyway)
    radiance += NIGHT_FLOOR * (SUN_INTENSITY / 3.0) * np.where(hits_planet, 0.3, 1.0)[:, None]
    return radiance.reshape(height, width, 3)


def aces(x):
    return np.clip((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0, 1)


def srgb(x):
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(np.maximum(x, 0), 1 / 2.4) - 0.055)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "sheet.png"
    elevations = [60, 30, 15, 5, 2, 0, -0.5, -4, -10, -20]
    w, h = 512, 256
    cols = 4
    rows = (len(elevations) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * (h + 22)), (20, 20, 20))
    dr = ImageDraw.Draw(sheet)
    for k, deg in enumerate(elevations):
        e = np.radians(deg)
        rad = render(e, w, h)
        exp = auto_exposure(e)
        img = (srgb(aces(rad * exp)) * 255 + 0.5).astype(np.uint8)
        x, y = (k % cols) * w, (k // cols) * (h + 22)
        sheet.paste(Image.fromarray(img), (x, y + 22))
        t = sun_transmittance(e)
        dr.text((x + 4, y + 4), f"elev {deg:+g} deg  exposure {exp:.2f}  T=({t[0]:.3f},{t[1]:.3f},{t[2]:.3f})", fill=(255, 255, 255))
        top = rad[:h // 8].mean(axis=(0, 1))
        hor = rad[h // 2 - 4:h // 2].mean(axis=(0, 1))
        print(f"{deg:+5g}  exp {exp:7.3f}  zenith {np.round(top,4)}  horizon {np.round(hor,4)}")
    sheet.save(out)
    print("wrote", out)
    print("sun transmittance check:")
    for deg in (90, 60, 30, 10, 2, 0.5, 0, -0.25, -0.5, -0.95, -5):
        print(deg, np.round(sun_transmittance(np.radians(deg)), 6))
    print("autoExposure table:")
    for deg in (90, 45, 30, 25, 20, 15, 10, 5, 0, -5, -10, -45):
        print(deg, round(auto_exposure(np.radians(deg)), 4))


if __name__ == "__main__":
    main()
