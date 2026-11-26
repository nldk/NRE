# Skyboxes

Six faces per folder, for `createCubemap(right, left, top, bottom, front, back)`:

```cpp
std::string dir = ASSET_DIR "/skyboxes/ocean/";
auto sky = createCubemap(dir + "right.png", dir + "left.png", dir + "top.png",
                         dir + "bottom.png", dir + "front.png", dir + "back.png");
```

All by Screaming Brain Studios, released under **CC0 1.0** (public domain, no credit required),
split from their 4x3 cross images into separate 512x512 faces:

| Folder        | Source image             | Pack |
|---------------|--------------------------|------|
| `clear_day`   | `Cubemap_Sky_06`         | [Cloudy Skyboxes](https://opengameart.org/content/cloudy-skyboxes-0) |
| `sunset`      | `Cubemap_Sky_21`         | [Cloudy Skyboxes](https://opengameart.org/content/cloudy-skyboxes-0) |
| `ocean`       | `Cubemap_Ocean_01`       | [Planet Surface Skyboxes](https://opengameart.org/content/planet-surface-skyboxes) |
| `plains`      | `Cubemap_Plains_01`      | [Planet Surface Skyboxes](https://opengameart.org/content/planet-surface-skyboxes) |
| `snowy`       | `Cubemap_Snowy_01`       | [Planet Surface Skyboxes](https://opengameart.org/content/planet-surface-skyboxes) |
| `desert_dusk` | `Cubemap_Desert_02`      | [Planet Surface Skyboxes](https://opengameart.org/content/planet-surface-skyboxes) |

License: https://creativecommons.org/publicdomain/zero/1.0/
