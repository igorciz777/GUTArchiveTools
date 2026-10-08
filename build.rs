fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    #[cfg(target_os = "windows")]
    {
        println!("cargo:rustc-link-lib=static=ucl");
        return;
    }

    let has_static = ["/usr/lib", "/usr/lib64", "/usr/local/lib", "/lib"]
        .iter()
        .any(|dir| std::path::Path::new(dir).join("libucl.a").exists());

    if has_static {
        println!("cargo:rustc-link-lib=static=ucl");
    } else {
        println!("cargo:rustc-link-lib=dylib=ucl");
    }
}