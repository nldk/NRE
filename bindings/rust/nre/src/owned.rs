//! [`Owned`]: an optional handle wrapper that destroys the object when dropped.

use std::fmt;
use std::marker::PhantomData;
use std::ops::Deref;

use crate::{Context, Cubemap, Instance, Light, Material, Model, Pipeline, Result, Scene, Shader, Texture};

mod sealed {
    pub trait Destroy: Copy {
        fn destroy(self) -> crate::Result<()>;
    }
}

/// A handle type [`Owned`] can wrap: every handle (they all have a `destroy_*` function). Sealed.
pub trait Resource: sealed::Destroy + fmt::Debug {}

macro_rules! resources {
    ($($handle:ident => $destroy:ident,)*) => {$(
        impl sealed::Destroy for $handle {
            fn destroy(self) -> Result<()> {
                crate::$destroy(self)
            }
        }
        impl Resource for $handle {}
    )*};
}

resources! {
    Shader => destroy_shader,
    Pipeline => destroy_pipeline,
    Model => destroy_model,
    Instance => destroy_instance,
    Light => destroy_light,
    Scene => destroy_scene,
    Texture => destroy_texture,
    Material => destroy_material,
    Cubemap => destroy_cubemap,
}

/// A handle that's destroyed (`destroy_*`) when dropped, from [`Context::own`]. Optional: plain `Copy` handles
/// are the default, and you can [`release`](Self::release) an `Owned` back into one.
///
/// It borrows the [`Context`], so it can't outlive the renderer, and it stays on the renderer's thread. Deref
/// (`*owned`) or [`handle`](Self::handle) gives the `Copy` handle to pass to the API. The renderer's rules for
/// destroying still apply: a texture should be swapped out of the materials using it first, and instances of a
/// destroyed model draw nothing. Dropping ignores the destroy error (the library prints it); use
/// [`destroy`](Self::destroy) to get it.
///
/// Borrowed from the [`Renderer`](crate::Renderer) itself, an `Owned` keeps [`Renderer::frame`] from being
/// called (it needs `&mut`), so for handles that live across frames, borrow from the context
/// [`Renderer::split`] returns.
///
/// ```no_run
/// # fn main() -> nre::Result<()> {
/// let mut renderer = nre::Renderer::<nre::DrawMode>::new(800, 600, "owned")?;
/// let (nre, mut frames) = renderer.split();
/// let model = nre.own(nre.create_model("police.glb")?);
/// let frame = frames.frame()?;
/// frame.draw_model(*model, None)?;
/// # Ok(())
/// # }
/// ```
///
/// It can't outlive the renderer:
///
/// ```compile_fail,E0505
/// # fn main() -> nre::Result<()> {
/// let nre = nre::Renderer::<nre::SceneMode>::new(800, 600, "owned")?;
/// let model = nre.own(nre.create_model("police.glb")?);
/// drop(nre); // still borrowed by `model`
/// drop(model);
/// # Ok(())
/// # }
/// ```
///
/// ```compile_fail,E0515
/// fn leak() -> nre::Result<nre::Owned<'static, nre::Model>> {
///     let nre = nre::Renderer::<nre::SceneMode>::new(800, 600, "owned")?;
///     Ok(nre.own(nre.create_model("police.glb")?))
/// }
/// ```
///
/// [`Renderer::frame`]: crate::Renderer::frame
/// [`Renderer::split`]: crate::Renderer::split
pub struct Owned<'r, T: Resource> {
    handle: T,
    _context: PhantomData<&'r Context>,
}

impl<T: Resource> Owned<'_, T> {
    pub(crate) fn new(handle: T) -> Self {
        Self { handle, _context: PhantomData }
    }

    /// The `Copy` handle (still owned by this).
    pub fn handle(&self) -> T {
        self.handle
    }

    /// Gives up ownership without destroying: the plain handle, to destroy yourself (or never).
    pub fn release(self) -> T {
        let handle = self.handle;
        std::mem::forget(self);
        handle
    }

    /// Destroys it now and returns the error, which dropping would ignore.
    pub fn destroy(self) -> Result<()> {
        sealed::Destroy::destroy(self.release())
    }
}

impl<T: Resource> Deref for Owned<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.handle
    }
}

impl<T: Resource> Drop for Owned<'_, T> {
    fn drop(&mut self) {
        let _ = sealed::Destroy::destroy(self.handle);
    }
}

impl<T: Resource> fmt::Debug for Owned<'_, T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Owned").field(&self.handle).finish()
    }
}
