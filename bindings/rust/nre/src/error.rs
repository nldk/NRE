use std::ffi::{CStr, c_char};
use std::fmt;

use crate::ErrorCode;

/// `std::result::Result` with this crate's [`Error`].
pub type Result<T> = std::result::Result<T, Error>;

/// A failed call: the error code and the message `nreGetLastErrorMessage()` gave right after it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    pub code: ErrorCode,
    pub message: String,
}

impl Error {
    /// The error for a non-`NRE_OK` return value, reading this thread's last error message.
    pub(crate) fn from_last_call(raw: nre_sys::NreErrorCode) -> Self {
        Self {
            code: ErrorCode::from_raw(raw).unwrap_or(ErrorCode::Unknown),
            message: crate::get_last_error_message(),
        }
    }

    pub(crate) fn new(code: ErrorCode, message: impl Into<String>) -> Self {
        Self { code, message: message.into() }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.code, self.message)
    }
}

impl std::error::Error for Error {}

/// The library's name for the code, e.g. "FileNotFound" (see [`crate::error_code_to_string`]).
impl fmt::Display for ErrorCode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(crate::error_code_to_string(*self))
    }
}

/// Copies a C string the library returned; NULL becomes "".
///
/// # Safety
/// `ptr` is NULL or points to a NUL-terminated string that stays valid during the call.
pub(crate) unsafe fn copy_c_str(ptr: *const c_char) -> String {
    if ptr.is_null() {
        String::new()
    } else {
        // SAFETY: the caller guarantees ptr is a valid NUL-terminated string.
        unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned()
    }
}
