# Examples

LMScript ports of the upstream Filament C++ samples (`third_party/filament/samples/*.cpp`). Build the host
first (`scripts/build-filament.sh`, `scripts/build.sh`), put `build/host` on `PATH`, and run from the repo
root:

```sh
PATH=$PWD/build/host:$PATH lmscript examples/hellopbr.lms
```

The host uses Filament's noop backend, so scenes are built and driven through the render loop without
producing pixels. See `durable-memory/06-upstream-sample-map.md` for what each port keeps and omits, and
`./test.sh` to run them all.
