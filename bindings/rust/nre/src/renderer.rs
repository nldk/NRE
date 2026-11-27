//! The typed layer's owner and frame types: [`Renderer`], [`Frame`], [`Frames`] and the API mode types.

use std::fmt;
use std::marker::PhantomData;
use std::ops::Deref;
use std::sync::atomic::{AtomicBool, Ordering};

use crate::{ApiMode, Context, Error, ErrorCode, Instance, Model, RenderErrorPolicy, RenderMode, Result, Transform};

mod sealed {
    pub trait Sealed {}
}

/// renderer.h's `ApiMode` as a type: [`SceneMode`] or [`DrawMode`], the `M` of [`Renderer<M>`] and [`Frame<M>`].
/// Sealed: there are only these two.
pub trait Mode: sealed::Sealed + 'static {
    /// The [`ApiMode`] the renderer is configured with.
    const API_MODE: ApiMode;
}

/// [`ApiMode::Scene`] as a type: [`Renderer::frame`] draws the bound scene's instances. Frames have no draw calls.
#[derive(Debug)]
pub enum SceneMode {}

/// [`ApiMode::FunctionDraw`] as a type: [`Renderer::frame`] starts a frame, you draw with
/// [`Frame::draw_instance`] / [`Frame::draw_model`], and dropping (or [`end`](Frame::end)ing) the frame ends it.
/// The camera, lights and pipeline still come from the bound scene (init creates one); its instances aren't drawn.
#[derive(Debug)]
pub enum DrawMode {}

impl sealed::Sealed for SceneMode {}
impl sealed::Sealed for DrawMode {}

impl Mode for SceneMode {
    const API_MODE: ApiMode = ApiMode::Scene;
}

impl Mode for DrawMode {
    const API_MODE: ApiMode = ApiMode::FunctionDraw;
}

// ---------------------------------------------------------------------------
// One renderer at a time
// ---------------------------------------------------------------------------

/// Whether a [`Renderer`] exists (the C side has one global renderer).
static CLAIMED: AtomicBool = AtomicBool::new(false);

/// The right to configure and own the C renderer. Taken before touching C; released when dropped.
#[derive(Debug)]
struct Claim(());

impl Claim {
    fn take() -> Result<Self> {
        if CLAIMED.swap(true, Ordering::AcqRel) {
            Err(Error::new(
                ErrorCode::AlreadyInitialized,
                "a nre::Renderer already exists in this process; drop it or call shutdown() first",
            ))
        } else {
            Ok(Self(()))
        }
    }
}

impl Drop for Claim {
    fn drop(&mut self) {
        CLAIMED.store(false, Ordering::Release);
    }
}

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------

/// The renderer, configured and initialized, in API mode `M` ([`SceneMode`] or [`DrawMode`]).
///
/// [`new`](Self::new) / [`new_with`](Self::new_with) call `set_render_config` and `init`, and dropping it calls
/// `shutdown` (or call [`shutdown`](Self::shutdown) to get its error). The API functions are methods of
/// [`Context`], which `Renderer` derefs to: `nre.create_model(path)` is [`crate::create_model`]. Starting a frame
/// takes `&mut self` ([`frame`](Self::frame)), so frames can't overlap and the renderer can't be shut down during
/// one.
///
/// - **One at a time:** the C renderer is global, so while a `Renderer` exists, another `new` fails with
///   [`ErrorCode::AlreadyInitialized`] without touching the library. After a `Renderer` is dropped a new one can
///   be created. Don't mix in the low-level `set_render_config` / `init` / `shutdown` functions.
/// - **Threads:** GLFW needs the main thread, so create it in `main`'s thread. It's neither `Send` nor `Sync`, so
///   it (and everything borrowed from it) stays on the thread that created it.
///
/// ```no_run
/// use nre::prelude::*;
///
/// fn main() -> nre::Result<()> {
///     let mut nre = Renderer::<DrawMode>::new(500, 500, "test")?;
///     let car = nre.create_instance(nre.create_model("police.glb")?)?;
///     // ... create and bind a pipeline ...
///     while !nre.window_should_close()? {
///         nre.poll_events()?;
///         let frame = nre.frame()?; // renderFrame
///         frame.draw_instance(car)?;
///     } // frame dropped: endFrame
///     Ok(())
/// } // nre dropped: shutdown
/// ```
///
/// It can't leave its thread:
///
/// ```compile_fail,E0277
/// fn assert_send<T: Send>() {}
/// assert_send::<nre::Renderer<nre::DrawMode>>();
/// ```
pub struct Renderer<M: Mode> {
    context: Context,
    shut_down: bool,
    _claim: Claim,
    _mode: PhantomData<fn() -> M>,
}

impl<M: Mode> Renderer<M> {
    /// [`new_with`](Self::new_with) with renderer.h's defaults: 3D, [`RenderErrorPolicy::Return`], automatic
    /// presentation and submission, a resizable window with the FPS in its title.
    pub fn new(width: i32, height: i32, title: &str) -> Result<Self> {
        Self::new_with(
            width,
            height,
            title,
            RenderMode::default(),
            RenderErrorPolicy::default(),
            false,
            true,
            true,
        )
    }

    /// Configures the renderer ([`crate::set_render_config_with`], with `M`'s API mode) and initializes it
    /// ([`crate::init`]), which opens the window. Main thread only (GLFW).
    ///
    /// Fails with [`ErrorCode::AlreadyInitialized`] while another `Renderer` exists. If `init` fails, the C side
    /// keeps its half-made context (shutting it down isn't safe), so no other `Renderer` can be created in this
    /// process.
    ///
    /// With `manual_frame_presentation_and_submission`, the frame guard submits and presents the frame when it
    /// ends; see [`Frame`].
    #[allow(clippy::too_many_arguments)]
    pub fn new_with(
        width: i32,
        height: i32,
        title: &str,
        render_mode: RenderMode,
        error_policy: RenderErrorPolicy,
        manual_frame_presentation_and_submission: bool,
        resizable: bool,
        show_fps_in_title: bool,
    ) -> Result<Self> {
        let claim = Claim::take()?;
        // On failure the claim is dropped (released): set_render_config failing leaves no context behind.
        crate::set_render_config_with(
            width,
            height,
            title,
            render_mode,
            M::API_MODE,
            error_policy,
            manual_frame_presentation_and_submission,
            resizable,
            show_fps_in_title,
        )?;
        if let Err(error) = crate::init() {
            std::mem::forget(claim);
            return Err(error);
        }
        Ok(Self {
            context: Context::new(manual_frame_presentation_and_submission),
            shut_down: false,
            _claim: claim,
            _mode: PhantomData,
        })
    }

    /// Starts a frame ([`crate::render_frame`]); dropping the returned [`Frame`] ends it. In [`SceneMode`] this
    /// already draws the bound scene; in [`DrawMode`] it opens the frame for draw calls.
    ///
    /// Takes `&mut self`, so there's one frame at a time. The [`Frame`] derefs to the [`Context`], so the
    /// `&self` methods (camera, instances, lights, input, ...) still work while it's alive.
    pub fn frame(&mut self) -> Result<Frame<'_, M>> {
        Frame::begin(&self.context)
    }

    /// The [`Context`] and the frame starter separately, so [`Owned`](crate::Owned) handles borrowed from the
    /// context can live across frames. (Borrowed from the renderer itself, they'd keep [`frame`](Self::frame)
    /// from taking `&mut self`.)
    ///
    /// ```no_run
    /// # fn main() -> nre::Result<()> {
    /// let mut renderer = nre::Renderer::<nre::SceneMode>::new(800, 600, "split")?;
    /// let (nre, mut frames) = renderer.split();
    /// let sky = nre.own(nre.create_cubemap("r.png", "l.png", "t.png", "b.png", "f.png", "k.png")?);
    /// nre.set_skybox(Some(*sky))?;
    /// while !nre.window_should_close()? {
    ///     nre.poll_events()?;
    ///     frames.frame()?;
    /// }
    /// # Ok(())
    /// # }
    /// ```
    pub fn split(&mut self) -> (&Context, Frames<'_, M>) {
        (&self.context, Frames { context: &self.context, _mode: PhantomData })
    }

    /// Shuts the renderer down ([`crate::shutdown`]) and returns its error. Dropping the renderer does the same
    /// but can't report it (the library still prints it to stderr). Afterwards a new `Renderer` can be created.
    pub fn shutdown(mut self) -> Result<()> {
        self.shut_down = true;
        crate::shutdown()
        // self drops here, releasing the claim.
    }
}

impl<M: Mode> Deref for Renderer<M> {
    type Target = Context;

    fn deref(&self) -> &Context {
        &self.context
    }
}

impl<M: Mode> Drop for Renderer<M> {
    fn drop(&mut self) {
        if !self.shut_down {
            // Nothing to do with the error here; the library has already printed it as "[NRE] ...".
            let _ = crate::shutdown();
        }
    }
}

impl<M: Mode> fmt::Debug for Renderer<M> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Renderer").field("mode", &M::API_MODE).field("context", &self.context).finish()
    }
}

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

/// Starts frames: the frame half of [`Renderer::split`]. [`frame`](Self::frame) is [`Renderer::frame`].
pub struct Frames<'r, M: Mode> {
    context: &'r Context,
    _mode: PhantomData<fn() -> M>,
}

impl<M: Mode> Frames<'_, M> {
    /// [`Renderer::frame`]: starts a frame; dropping it ends it. One at a time (`&mut self`).
    pub fn frame(&mut self) -> Result<Frame<'_, M>> {
        Frame::begin(self.context)
    }
}

impl<M: Mode> fmt::Debug for Frames<'_, M> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Frames").field("mode", &M::API_MODE).finish()
    }
}

/// A frame in progress, from [`Renderer::frame`] or [`Frames::frame`]. Dropping it ends the frame; [`end`](Self::end)
/// does the same and returns the error.
///
/// What ending does depends on the mode:
///
/// | | automatic presentation (default) | `manual_frame_presentation_and_submission` |
/// |---|---|---|
/// | [`SceneMode`] | nothing: `render_frame` already drew, submitted and presented it | `submit_frame`, `present_frame` |
/// | [`DrawMode`] | `end_frame` (records, submits, presents) | `end_frame`, `submit_frame`, `present_frame` |
///
/// It derefs to the [`Context`], so the `&self` methods work during the frame. In [`DrawMode`],
/// [`draw_instance`](Self::draw_instance) uses the instance's transform at the time of the call, so instances
/// can be moved between draws. The camera and lights were read when the frame started: changes to them show
/// from the next frame on.
///
/// In [`SceneMode`] a frame has no draw calls:
///
/// ```compile_fail,E0599
/// # fn main() -> nre::Result<()> {
/// let mut nre = nre::Renderer::<nre::SceneMode>::new(800, 600, "scene")?;
/// let car = nre.create_instance(nre.create_model("police.glb")?)?;
/// let frame = nre.frame()?;
/// frame.draw_instance(car)?; // no draw_instance on Frame<SceneMode>
/// # Ok(())
/// # }
/// ```
///
/// Frames can't overlap: while one is alive, the renderer is mutably borrowed.
///
/// ```compile_fail,E0499
/// # fn main() -> nre::Result<()> {
/// let mut nre = nre::Renderer::<nre::DrawMode>::new(800, 600, "draw")?;
/// let first = nre.frame()?;
/// let second = nre.frame()?; // nre is still borrowed by `first`
/// drop(first);
/// # Ok(())
/// # }
/// ```
///
/// Nor can the renderer be shut down (or dropped) during one:
///
/// ```compile_fail,E0505
/// # fn main() -> nre::Result<()> {
/// let mut nre = nre::Renderer::<nre::DrawMode>::new(800, 600, "draw")?;
/// let frame = nre.frame()?;
/// nre.shutdown()?; // can't move nre while `frame` borrows it
/// drop(frame);
/// # Ok(())
/// # }
/// ```
///
/// A Renderer's frame only exists in its own scope: it can't be returned past the renderer either.
///
/// ```compile_fail,E0515
/// fn leak() -> nre::Result<nre::Frame<'static, nre::DrawMode>> {
///     let mut nre = nre::Renderer::<nre::DrawMode>::new(800, 600, "draw")?;
///     nre.frame()
/// }
/// ```
///
/// **Known renderer limitation (manual presentation):** when `render_frame` skips a frame (the swapchain was
/// out of date and got recreated) it still returns OK, and there's no way to tell. The guard then submits and
/// presents a frame that wasn't recorded, as C++ code calling `submitFrame` / `presentFrame` would. Likewise, in
/// [`SceneMode`] without a bound scene `render_frame` records an empty frame but returns
/// [`ErrorCode::NoSceneBound`], and the guard doesn't submit it. Bind a scene before the first frame.
pub struct Frame<'r, M: Mode> {
    context: &'r Context,
    ended: bool,
    _mode: PhantomData<fn() -> M>,
}

impl<'r, M: Mode> Frame<'r, M> {
    fn begin(context: &'r Context) -> Result<Self> {
        if let Err(error) = crate::render_frame() {
            // In FunctionDrawMode a frame that failed to start is still open (the next render_frame would fail
            // with WrongMode in Debug), so close it. Nothing was acquired, so there's nothing to submit.
            if M::API_MODE == ApiMode::FunctionDraw {
                let _ = crate::end_frame();
            }
            return Err(error);
        }
        Ok(Self { context, ended: false, _mode: PhantomData })
    }

    /// Ends the frame (see the table above) and returns the first error. Dropping the frame does the same.
    pub fn end(mut self) -> Result<()> {
        self.finish()
    }

    fn finish(&mut self) -> Result<()> {
        if self.ended {
            return Ok(());
        }
        self.ended = true;
        let ended = if M::API_MODE == ApiMode::FunctionDraw { crate::end_frame() } else { Ok(()) };
        if !self.context.manual_frame_presentation_and_submission() {
            return ended;
        }
        // Like the renderer's own automatic path, submit and present even if recording reported an error.
        let submitted = crate::submit_frame();
        let presented = crate::present_frame();
        ended.and(submitted).and(presented)
    }
}

impl Frame<'_, DrawMode> {
    /// [`crate::draw_instance`]: draws the instance with its current transform. A hidden instance isn't drawn.
    pub fn draw_instance(&self, instance: Instance) -> Result<()> {
        crate::draw_instance(instance)
    }

    /// [`crate::draw_model`]: draws the model once with this transform (`None`: the identity), no instance needed.
    pub fn draw_model(&self, model: Model, transform: Option<&Transform>) -> Result<()> {
        crate::draw_model(model, transform)
    }
}

impl<M: Mode> Deref for Frame<'_, M> {
    type Target = Context;

    fn deref(&self) -> &Context {
        self.context
    }
}

impl<M: Mode> Drop for Frame<'_, M> {
    fn drop(&mut self) {
        // As with Renderer: the library prints the error itself. Use end() to get it.
        let _ = self.finish();
    }
}

impl<M: Mode> fmt::Debug for Frame<'_, M> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Frame").field("mode", &M::API_MODE).field("ended", &self.ended).finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // The only test in this binary touching CLAIMED, so nothing else races it. Never reaches the C library:
    // the claim is checked before set_render_config.
    #[test]
    fn only_one_renderer_at_a_time() {
        let claim = Claim::take().expect("nothing holds the claim yet");
        assert_eq!(Claim::take().unwrap_err().code, ErrorCode::AlreadyInitialized);

        // Renderer::new checks the claim before calling into C, so this neither configures the renderer nor
        // opens a window (or aborts, as a C call before set_render_config would).
        let error = Renderer::<DrawMode>::new(100, 100, "second").unwrap_err();
        assert_eq!(error.code, ErrorCode::AlreadyInitialized);
        assert!(error.message.contains("already exists"), "{error}");
        let error = Renderer::<SceneMode>::new_with(
            100,
            100,
            "second",
            RenderMode::Render2D,
            RenderErrorPolicy::Panic,
            true,
            false,
            false,
        )
        .unwrap_err();
        assert_eq!(error.code, ErrorCode::AlreadyInitialized);
        assert!(CLAIMED.load(Ordering::Acquire), "a failed new must not release someone else's claim");

        drop(claim);
        assert!(!CLAIMED.load(Ordering::Acquire));
        let again = Claim::take().expect("free again after the drop");
        drop(again);
    }

    #[test]
    fn mode_constants() {
        assert_eq!(SceneMode::API_MODE, ApiMode::Scene);
        assert_eq!(DrawMode::API_MODE, ApiMode::FunctionDraw);
    }
}
