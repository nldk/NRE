#pragma once

namespace NRE::DefaultAssets {
    // The PNG bytes are compiled into librenderer.so with #embed, so they're always there,
    // wherever the library ends up. Editing the .png rebuilds whatever includes this header.
    // Decode with stbi_load_from_memory(defaultTexturePng, sizeof(defaultTexturePng), ...).
    inline constexpr unsigned char defaultTexturePng[] = {
        #embed "defaultTexture.png"
    };
}
