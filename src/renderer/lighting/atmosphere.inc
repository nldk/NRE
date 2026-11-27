// The atmosphere's constants, in one place for the C++ side (LightingMath::Atmosphere in lighting/lightingMath.h)
// and the built-in shaders (shaders/atmosphere.glsl, which names them ATM_<name>). Settled with
// tests/tools/skyPrototype.py (docs/superpowers/notes/2026-11-26-sky-prototype.md): Hillaire 2020 values, lengths in
// kilometres, coefficients in 1/km. The includer defines the three macros first:
//   NRE_ATMOSPHERE_SCALAR(name, value)   NRE_ATMOSPHERE_RGB(name, r, g, b)   NRE_ATMOSPHERE_INT(name, value)
NRE_ATMOSPHERE_SCALAR(planetRadius, 6360.0)
NRE_ATMOSPHERE_SCALAR(topRadius, 6460.0)
NRE_ATMOSPHERE_SCALAR(viewerHeight, 0.0015)          // 1.5 m
NRE_ATMOSPHERE_RGB(rayleighScatter, 5.802e-3, 13.558e-3, 33.1e-3)
NRE_ATMOSPHERE_SCALAR(rayleighScaleHeight, 8.0)
NRE_ATMOSPHERE_SCALAR(mieScatter, 3.996e-3)
NRE_ATMOSPHERE_SCALAR(mieAbsorb, 4.40e-3)
NRE_ATMOSPHERE_SCALAR(mieScaleHeight, 1.2)
NRE_ATMOSPHERE_SCALAR(mieG, 0.8)                     // Cornette-Shanks asymmetry
NRE_ATMOSPHERE_RGB(ozoneAbsorb, 0.650e-3, 1.881e-3, 0.085e-3)
NRE_ATMOSPHERE_SCALAR(ozoneCenter, 25.0)             // tent profile: 1 at the centre, 0 at +-ozoneHalfWidth
NRE_ATMOSPHERE_SCALAR(ozoneHalfWidth, 15.0)
NRE_ATMOSPHERE_INT(transmittanceSteps, 64)           // midpoint rule with quadratic spacing t = tMax * u^2
NRE_ATMOSPHERE_INT(viewSteps, 32)                    // the sky's view ray, same spacing
NRE_ATMOSPHERE_SCALAR(skyScale, 10.0)                // single-scatter radiance gain (stands in for multiple scattering)
NRE_ATMOSPHERE_SCALAR(sunAngularRadius, 0.0046690)   // 0.2675 deg in radians
// The sun's light fades out as the disk sinks: factor 0 at sunFadeLowDeg and below, 1 at sunFadeHighDeg and above
// (smoothstep). Wide enough (about two disk diameters) that a running sunset never pops; full strength at the horizon.
NRE_ATMOSPHERE_SCALAR(sunFadeLowDeg, -0.95)
NRE_ATMOSPHERE_SCALAR(sunFadeHighDeg, 0.0)
// The setSun intensity of the standard look. The sunlit sky scales with intensity / standardIntensity.
NRE_ATMOSPHERE_SCALAR(standardIntensity, 3.0)
// Linear radiance of the night sky, added always (invisible by day). It doesn't scale with the sun's intensity, so a
// sunless Landscape scene shows night, not black.
NRE_ATMOSPHERE_RGB(nightFloor, 0.0025, 0.0045, 0.0100)
// The ground below the horizon of an empty world (the scene's own ground normally covers it): albedo, the sky light
// on it, groundAmbient * max(sin(sun elevation) + 0.15, 0), and how much of the night floor it shows.
NRE_ATMOSPHERE_SCALAR(groundAlbedo, 0.2)
NRE_ATMOSPHERE_RGB(groundAmbient, 0.04, 0.056, 0.08)
NRE_ATMOSPHERE_SCALAR(groundNightFloor, 0.3)
// Auto exposure: 1 at >= exposureHighDeg, exp(ln(exposureMax) * smoothstep) down to exposureLowDeg and below.
NRE_ATMOSPHERE_SCALAR(exposureMax, 5.0)
NRE_ATMOSPHERE_SCALAR(exposureHighDeg, 30.0)
NRE_ATMOSPHERE_SCALAR(exposureLowDeg, -10.0)
