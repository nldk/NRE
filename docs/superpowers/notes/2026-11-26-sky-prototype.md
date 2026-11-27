# Sky prototype: settled atmosphere constants and auto exposure

Prototype: `tests/tools/skyPrototype.py` (numpy + Pillow, ~20 s). Run `python3 tests/tools/skyPrototype.py out.png`.
It renders 512x256 equirectangular full-sphere images (sun at the image centre azimuth, horizon at mid-height) for
sun elevations 60, 30, 15, 5, 2, 0, -4, -10, -20 degrees, applies `autoExposure`, the ACES fit and sRGB, and tiles
them into a labelled sheet. The C++ side is in `src/renderer/lighting/lightingMath.h`
(`NRE::LightingMath::Atmosphere`, `sunTransmittance`, `autoExposure`); the Python and C++ values agree to 6 digits.

Sheet (not committed): `/tmp/claude-1000/-home-niel-Code-VulkanRenderer/db8cf2f1-6abb-4ff5-a512-b72a54f33d40/scratchpad/sky-prototype/sheet.png`

## Result of the look check
Noon is clear blue with a pale horizon band; golden hour (15 degrees) is warm-white near the sun; 5 degrees is
yellow-white at the horizon; 2 and 0 degrees give a yellow-to-red horizon line with a blue-grey / violet sky overhead;
-4 degrees is a dim navy sky with a faint red glow at the horizon; -10 and -20 degrees are a dim blue floor, visible
but clearly night, never black. Ground in the sheet is a flat 0.2 albedo plane, only there for contrast.

## Constants (lengths in km, coefficients in 1/km, Hillaire 2020 values)
| Name | Value |
|---|---|
| planet radius / atmosphere top radius | 6360 / 6460 |
| viewer height | 0.0015 (1.5 m) |
| Rayleigh scattering RGB | (5.802e-3, 13.558e-3, 33.1e-3), density exp(-h/8) |
| Mie scattering / absorption | 3.996e-3 / 4.40e-3 (grey), density exp(-h/1.2) |
| Mie phase | Cornette-Shanks, g = 0.8; Rayleigh phase 3/(16 pi) (1 + cos^2) |
| Ozone absorption RGB | (0.650e-3, 1.881e-3, 0.085e-3), tent density, centre 25, half width 15 |
| Extinction | Rayleigh scatter + Mie (scatter + absorb) + ozone absorb, all per channel |
| Optical depth integration | 64 midpoint steps, quadratic spacing t = tMax u^2 (dt = 2 tMax u / 64) |
| Sky scatter gain (`skyScale`) | 10 (applied to single-scatter radiance, stands in for multiple scattering; stays at intensity 3, scales with intensity / 3) |
| Night floor (linear radiance) | (0.0025, 0.0045, 0.0100), added to the sky always (invisible by day), at any sun intensity (0.3 of it below the horizon) |

Sky radiance (view march: 32 quadratic steps, 8 light-ray steps per sample in the prototype):
`L = skyScale * (intensity / 3) * sum(sunTransmittanceAtSample * (rayleighPhase * betaR(h) + miePhase * betaM(h)) * viewTransmittance * dt) + nightFloor`.
The sun is white with unit irradiance inside the integral; the library multiplies the sunlit part by `intensity / 3`. The night floor does not scale with the intensity (Task 8 fix round 1, ruling R36): a sunless Landscape scene (intensity 0) shows night, with the night exposure.
Samples where the sample-to-sun ray hits the planet get zero sun light.

## Sun
- Angular radius 0.2675 degrees (0.00467 rad, solid angle 6.85e-5 sr).
- Disk radiance = `intensity * sunTransmittance(elevation) / solidAngle`, about 43800 * T at intensity 3, i.e. it
  saturates through ACES; no limb darkening, no bloom. Sky pass should coverage-blend the disk edge over one pixel.
- Object lighting radiance = `sunColor * intensity * sunTransmittance(elevation)` (Landscape), as in the plan.
- Horizon fade (single definition: `LightingMath::sunTransmittance`, constants `Atmosphere::sunFadeLowDeg = -0.95`,
  `sunFadeHighDeg = 0.0`): the optical depth is evaluated at max(elevation, 0) (never NaN/inf) and multiplied by
  `smoothstep(-0.95deg, 0deg, elevation)`, so the sun's light is full strength with the disk centre on the horizon,
  fades smoothly, and is exactly 0 from -0.95 degrees down. Task 8 uses `sunTransmittance` as is, with no separate
  fade factor (sun light, sun disk and shadow-off threshold all follow it). The sky's own scattering is not faded
  (the sky glow persists below the horizon, as in the -4 degree panel). Chosen over the suggested -0.6..+0.6 range
  because the physical transmittance already falls ~8% per 0.05 degree near 0 degrees; a fade centred there would
  break the "no step above 10%" test (`sunFadesSmoothly`, checked on the red channel; green/blue fall steeply by physics).

Cross-check values (identical in Python and `lightingMath.h`):
| elevation | T (r, g, b) |
|---|---|
| 90 | 0.935917, 0.863548, 0.758685 |
| 60 | 0.926414, 0.844254, 0.727049 |
| 30 | 0.876418, 0.746730, 0.576760 |
| 10 | 0.693868, 0.447149, 0.215676 |
| 2 | 0.293224, 0.075599, 0.005459 (r/b = 53.7) |
| 0.5 | 0.126757, 0.016571, 0.000230 |
| 0 | 0.069089, 0.006220, 0.000034 |
| -0.25 | 0.057253, 0.005154, 0.000028 |
| -0.5 | 0.031820, 0.002865, 0.000016 |
| -0.95 and below | 0, 0, 0 |

## Auto exposure
`t = smoothstep(30deg, -10deg, elevation)` (clamped), `autoExposure = exp(ln(5) * t) = 5^t`.
1 at >= 30 degrees, 5 at <= -10 degrees (night level), non-increasing in elevation, continuous with zero slope at both ends.

| elevation | 90 / 45 / 30 | 25 | 20 | 15 | 10 | 5 | 0 | -5 | -10 and below |
|---|---|---|---|---|---|---|---|---|---|
| exposure | 1 | 1.072 | 1.286 | 1.664 | 2.236 | 3.005 | 3.888 | 4.666 | 5 |

The rise (about 4x at the horizon) is far smaller than the drop in light (sun x0.07 red), so dusk reads darker and
warmer, not normalised to noon. Sky brightness (linear, before exposure, intensity 3), zenith / just above horizon:
60 deg (0.062, 0.114, 0.227) / (0.33, 0.43, 0.44); 5 deg (0.023, 0.035, 0.057) / (0.46, 0.28, 0.09);
0 deg (0.014, 0.014, 0.023) / (0.67, 0.085, 0.012); -4 deg (0.003, 0.005, 0.011); night = the floor (0.0025, 0.0045, 0.0100).
At exposure 5 the night floor becomes sRGB (16, 29, 59) after ACES (an earlier version of this note said (13, 20, 38), a miscalculation; the prototype and the GPU both give (16, 29, 59)).

## What Task 8's GPU implementation should match
- Same constants and units (copy `Atmosphere::` values into `transmittanceLut.comp` / the sky shader), same quadratic
  midpoint optical-depth integration, same ozone tent profile, phase functions and `skyScale`.
- `FrameData.skyParams.z` = the procedural sky's scattering brightness, `intensity / 3` (ruling R37). The sky cubemap (and so the IBL source) already holds the finished radiance: the sunlit part times it, plus the night floor, which does not scale.
- Tone-map exposure in Landscape = `setExposure * autoExposure(elevation)` with the procedural sky (with a skybox it is `setExposure` alone, ruling R38); sun below the horizon gives no sun light and no sun shadows.
- Sun disk and sun light use the radiance above; the horizon rows near the sun can reach ~4 in linear radiance at 2 degrees (ACES clamps them).
- No stars or moon. The prototype ground is not a requirement; ground comes from the scene.
- The twilight glow at -4 degrees comes only from single scattering by sunlit upper layers; the GPU version should look the same without extra terms.
