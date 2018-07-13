To build benchmarks:

```sh
# At the top dir
mkdir -p build-bench
cd build-bench
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_BENCHMARKS=ON ..
make -j8
```

That will build `build-bench/bench/*` binaries. You'll need to symlink some
resources for some benchmarks (`curl -o subs.json .../api/v1/expressions`)
