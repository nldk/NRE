// Links libnre_c. With NRE_LIB_DIR set (an absolute path), the linker also looks in that directory, and the
// directory is passed on to crates depending on this one as DEP_NRE_C_LIB_DIR (the `nre` crate uses it to add
// an rpath to its examples and tests). Without it, the platform's normal library search is used.
fn main() {
    println!("cargo::rerun-if-env-changed=NRE_LIB_DIR");
    if let Some(dir) = std::env::var_os("NRE_LIB_DIR") {
        let dir = dir.to_string_lossy();
        println!("cargo::rustc-link-search=native={dir}");
        println!("cargo::metadata=lib_dir={dir}");
    }
    println!("cargo::rustc-link-lib=dylib=nre_c");
}
