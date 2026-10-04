export RUSTFLAGS="-C target-feature=+crt-static -C link-args=-static -C link-arg=-static-libgcc -C link-arg=-Wl,-Bstatic -C link-arg=-lucl -C link-arg=-Wl,-Bdynamic";
cargo build --release;