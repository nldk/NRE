// Package nre is the Go binding of NRE, a Vulkan renderer, over its C API (include/nre.h, libnre_c).
//
// The functions mirror the C++ API in include/renderer.h (NRE::createModel is nre.CreateModel, ...);
// see renderer.h for what each one does. Every function that can fail returns an error, an *Error that
// carries the ErrorCode and the renderer's message:
//
//	model, err := nre.CreateModel("box.glb")
//	if errors.Is(err, nre.ErrFileNotFound) { ... }
//
// Handles (Model, Instance, ...) are small comparable values to copy around. Their zero value is the
// empty handle, e.g. nre.Texture{} for "no texture" or nre.Cubemap{} for "no skybox". Nothing is freed
// automatically: destroy what you create with the matching DestroyXxx, like in C++.
//
// GLFW must run on the main OS thread: lock it in an init function of your main package, before
// calling SetRenderConfig/Init, and make every nre call from the main goroutine:
//
//	func init() { runtime.LockOSThread() }
//
// Linking: the package links -lnre_c. Point the build at the library with
//
//	CGO_LDFLAGS="-L$NRE_LIB_DIR -Wl,-rpath,$NRE_LIB_DIR" go build ...
//
// (see the README). Init and SetRenderConfig check that the library's version is 0.3.x, the ABI these
// bindings target, and fail with ErrLibraryLoadFailed otherwise.
package nre

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include
#cgo LDFLAGS: -lnre_c
#include "nre.h"
*/
import "C"

import (
	"fmt"
	"runtime"
	"sync"
)

// The library version these bindings are written against: any 0.3.x.
const (
	TargetVersionMajor = 0
	TargetVersionMinor = 3
)

// call runs one C call and turns a non-OK result into an *Error. libnre_c keeps the last error message
// per OS thread, so the goroutine is locked to its thread for the call and the message read after it
// (Go may otherwise move it to another thread between the two cgo calls).
func call(f func() C.NreErrorCode) error {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	if code := f(); code != C.NRE_OK {
		return newError(code, C.GoString(C.nreGetLastErrorMessage()))
	}
	return nil
}

var (
	versionOnce sync.Once
	versionErr  error
)

// checkVersion fails if the loaded libnre_c isn't a 0.3.x release. Runs once.
func checkVersion() error {
	versionOnce.Do(func() { versionErr = versionError(GetVersion()) })
	return versionErr
}

// versionError is nil for a 0.3.x version, else the ErrLibraryLoadFailed error checkVersion returns.
func versionError(major, minor, patch uint32) error {
	if major != TargetVersionMajor || minor != TargetVersionMinor {
		return &Error{
			Code: ErrLibraryLoadFailed,
			Message: fmt.Sprintf("libnre_c is version %d.%d.%d, but these Go bindings need %d.%d.x",
				major, minor, patch, TargetVersionMajor, TargetVersionMinor),
		}
	}
	return nil
}

// GetVersion returns the version of the loaded libnre_c.
func GetVersion() (major, minor, patch uint32) {
	var ma, mi, pa C.uint32_t
	C.nreGetVersion(&ma, &mi, &pa)
	return uint32(ma), uint32(mi), uint32(pa)
}

// GetLastErrorMessage returns the message of the most recent failed call on the current OS thread
// ("" if none failed yet). The returned errors already carry it, so this is rarely needed; the result is
// only meaningful if the goroutine is locked to its thread (runtime.LockOSThread), as on the main thread.
func GetLastErrorMessage() string {
	return C.GoString(C.nreGetLastErrorMessage())
}

// ErrorCodeToString returns the renderer's name for code: "Ok", "FileNotFound", ...
func ErrorCodeToString(code ErrorCode) string {
	return C.GoString(C.nreErrorCodeToString(C.NreErrorCode(code)))
}
