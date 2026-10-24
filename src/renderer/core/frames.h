#pragma once

namespace NRE {
    // How many frames the CPU can record ahead of the GPU. Anything the CPU writes every frame
    // (command buffers, per-frame UBOs/SSBOs) needs this many copies.
    // Its own header so gpu/ code can use it without including core/context.h.
    constexpr int max_frames_in_flight = 2;
}
