# Performance baseline (before the lighting system)

Release build, `cmake-build-release/bin/nre_benchmark --frames 600 static move` (5000 boxes, 600 measured frames
after 60 warm-up frames, medians in ms; p99 is of the total). Recorded 2026-11-26 on the `lighting-system` branch
at commit a75decb, before any lighting changes.

| scenario | variant      | update | render | total | p99   |
|----------|--------------|-------:|-------:|------:|------:|
| static   | SceneMode    | 0.000  | 0.530  | 0.530 | 0.911 |
| static   | drawInstance | 0.000  | 0.506  | 0.506 | 1.059 |
| static   | drawModel    | 0.000  | 0.530  | 0.531 | 1.267 |
| move     | SceneMode    | 0.053  | 0.495  | 0.547 | 1.027 |
| move     | drawInstance | 0.055  | 0.509  | 0.563 | 0.971 |
| move     | drawModel    | 0.037  | 0.524  | 0.558 | 0.934 |

Run-to-run noise is a few percent: compare later numbers against these with that in mind.
