# Examples

LMScript ports of the upstream Filament C++ samples (`private implementation detail*.cpp`). Build the host
first (`private implementation detail`, `private implementation detail`), put `private implementation detail` on `PATH`, and run from the repo
root:

```sh
PATH=$PWD/private implementation detail:$PATH lmscript examples/hellopbr.lms
```

The host uses Filament's noop backend, so scenes are built and driven through the render loop without
producing pixels. See `private implementation detail` for what each port keeps and omits, and
`./test.sh` to run them all.
