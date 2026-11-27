// When nre-sys was pointed at a directory with NRE_LIB_DIR, bake that directory into this crate's examples and
// tests as an rpath, so `cargo run --example orbit` and `cargo test` find libnre_c without LD_LIBRARY_PATH.
// (Link args from nre-sys's build script only apply to nre-sys's own targets, hence this second script.)
// Your own binaries don't get this: set an rpath yourself, or LD_LIBRARY_PATH / DYLD_LIBRARY_PATH / PATH.
fn main() {
    println!("cargo::rerun-if-env-changed=NRE_LIB_DIR");
    let windows = std::env::var("CARGO_CFG_TARGET_OS").is_ok_and(|os| os == "windows");
    if let (Ok(dir), false) = (std::env::var("DEP_NRE_C_LIB_DIR"), windows) {
        // All of this package's linked targets: examples, integration tests and the lib's unit tests (which
        // rustc-link-arg-tests doesn't cover). The lib itself is an rlib, so dependents never see this.
        println!("cargo::rustc-link-arg=-Wl,-rpath,{dir}");
    }
}
