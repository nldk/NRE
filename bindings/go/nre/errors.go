package nre

/*
#include "nre.h"
*/
import "C"

import "fmt"

// ErrorCode is NreErrorCode from nre.h. Each code is itself an error, so the constants work as sentinels:
//
//	if errors.Is(err, nre.ErrNotInitialized) { ... }
//
// and errors.As(err, &code) extracts the code from an *Error.
type ErrorCode int32

// The error codes. The values match nre.h and never change; new ones are added at the end.
const (
	OK                                 ErrorCode = C.NRE_OK
	ErrUnsupportedGPU                  ErrorCode = C.NRE_ERROR_UNSUPPORTED_GPU
	ErrCreatingVulkanInstance          ErrorCode = C.NRE_ERROR_CREATING_VULKAN_INSTANCE
	ErrCreatingSwapchain               ErrorCode = C.NRE_ERROR_CREATING_SWAPCHAIN
	ErrRecreatingSwapchain             ErrorCode = C.NRE_ERROR_RECREATING_SWAPCHAIN
	ErrCreatingPipeline                ErrorCode = C.NRE_ERROR_CREATING_PIPELINE
	ErrWindowCreationFailed            ErrorCode = C.NRE_ERROR_WINDOW_CREATION_FAILED
	ErrFileNotFound                    ErrorCode = C.NRE_ERROR_FILE_NOT_FOUND
	ErrWrongMode                       ErrorCode = C.NRE_ERROR_WRONG_MODE
	ErrNotInitialized                  ErrorCode = C.NRE_ERROR_NOT_INITIALIZED
	ErrAlreadyInitialized              ErrorCode = C.NRE_ERROR_ALREADY_INITIALIZED
	ErrInvalidHandle                   ErrorCode = C.NRE_ERROR_INVALID_HANDLE // empty, destroyed, or never created
	ErrUnsupportedFormat               ErrorCode = C.NRE_ERROR_UNSUPPORTED_FORMAT
	ErrModelLoadFailed                 ErrorCode = C.NRE_ERROR_MODEL_LOAD_FAILED
	ErrFailedToCreateAllocator         ErrorCode = C.NRE_ERROR_FAILED_TO_CREATE_ALLOCATOR
	ErrBufferCreationFailed            ErrorCode = C.NRE_ERROR_BUFFER_CREATION_FAILED
	ErrUploadFailed                    ErrorCode = C.NRE_ERROR_UPLOAD_FAILED
	ErrNoPipelineBound                 ErrorCode = C.NRE_ERROR_NO_PIPELINE_BOUND
	ErrNoSceneBound                    ErrorCode = C.NRE_ERROR_NO_SCENE_BOUND
	ErrCreatingTexture                 ErrorCode = C.NRE_ERROR_CREATING_TEXTURE
	ErrImageCreationFailed             ErrorCode = C.NRE_ERROR_IMAGE_CREATION_FAILED
	ErrImageViewCreationFailed         ErrorCode = C.NRE_ERROR_IMAGE_VIEW_CREATION_FAILED
	ErrIndexOutOfRange                 ErrorCode = C.NRE_ERROR_INDEX_OUT_OF_RANGE
	ErrImpossibleTypeForPerFrameBuffer ErrorCode = C.NRE_ERROR_IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER
	ErrBufferWithSizeNull              ErrorCode = C.NRE_ERROR_BUFFER_WITH_SIZE_NULL
	ErrSizeOfBufferTooBig              ErrorCode = C.NRE_ERROR_SIZE_OF_BUFFER_TOO_BIG
	ErrDescriptorCreationFailed        ErrorCode = C.NRE_ERROR_DESCRIPTOR_CREATION_FAILED
	ErrShaderCreationFailed            ErrorCode = C.NRE_ERROR_SHADER_CREATION_FAILED
	ErrUnknown                         ErrorCode = C.NRE_ERROR_UNKNOWN // also: a C++ exception inside the renderer
	ErrLibraryLoadFailed               ErrorCode = C.NRE_ERROR_LIBRARY_LOAD_FAILED
	ErrInvalidArgument                 ErrorCode = C.NRE_ERROR_INVALID_ARGUMENT // e.g. a negative shadow distance, or picking in Render2D

	lastKnownErrorCode = ErrInvalidArgument
)

// String returns the renderer's name for the code ("FileNotFound", ...), see ErrorCodeToString.
func (c ErrorCode) String() string { return ErrorCodeToString(c) }

// Error makes an ErrorCode usable as a sentinel error.
func (c ErrorCode) Error() string { return "nre: " + c.String() }

// Error is the error every failing nre function returns: the code plus the message the renderer gave.
type Error struct {
	Code    ErrorCode
	Message string
}

func (e *Error) Error() string {
	if e.Message == "" {
		return "nre: " + e.Code.String()
	}
	return "nre: " + e.Code.String() + ": " + e.Message
}

// Unwrap returns the code, so errors.Is(err, nre.ErrXxx) and errors.As(err, &code) work.
func (e *Error) Unwrap() error { return e.Code }

// newError builds the *Error for a C return code. A code this version doesn't know (from a newer library)
// becomes ErrUnknown (the raw value is kept in the message), so callers only ever see the constants above.
func newError(code C.NreErrorCode, message string) *Error {
	if v := int64(code); v <= 0 || v > int64(lastKnownErrorCode) {
		return &Error{Code: ErrUnknown, Message: fmt.Sprintf("(unknown error code %d) %s", v, message)}
	}
	return &Error{Code: ErrorCode(code), Message: message}
}
