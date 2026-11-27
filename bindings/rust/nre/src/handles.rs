//! Handles: small `Copy` values (index + generation) naming an object inside the renderer. Like the C++ API they
//! are destroyed explicitly with the matching `destroy_*` function; dropping one does nothing. `NONE` is the empty
//! handle (nre.h's `{0}`); functions that accept "no texture" / "no skybox" take an `Option` instead.

macro_rules! handles {
    ($($(#[$meta:meta])* $name:ident($c:ident);)*) => {$(
        $(#[$meta])*
        #[repr(transparent)]
        #[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
        pub struct $name(pub(crate) nre_sys::$c);

        impl $name {
            /// The empty handle. Calls given it fail with `ErrorCode::InvalidHandle`.
            pub const NONE: Self = Self(nre_sys::$c { index: 0, generation: 0 });

            /// The C handle, for calling `nre_sys` directly.
            pub const fn to_raw(self) -> nre_sys::$c {
                self.0
            }

            /// Wraps a C handle from `nre_sys`.
            pub const fn from_raw(raw: nre_sys::$c) -> Self {
                Self(raw)
            }
        }

        impl Default for $name {
            fn default() -> Self {
                Self::NONE
            }
        }
    )*};
}

handles! {
    /// A compiled SPIR-V shader module.
    Shader(NreShader);
    Pipeline(NrePipeline);
    /// A loaded model file (meshes, GPU buffers, materials). Not drawn by itself: draw it through [`Instance`]s
    /// or `draw_model`.
    Model(NreModel);
    /// A copy of a model with its own transform; all instances share the model's data.
    Instance(NreInstance);
    Light(NreLight);
    Scene(NreScene);
    Texture(NreTexture);
    Material(NreMaterial);
    Cubemap(NreCubemap);
}
