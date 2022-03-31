[![Build Status](https://travis-ci.com/Netflix-Skunkworks/atlas-native-client.svg?branch=master)](https://travis-ci.com/Netflix-Skunkworks/atlas-native-client)

# Atlas Client Native Library

> :warning: This library is deprecated. C++ projects should migrate to
[spectator-cpp](https://github.com/Netflix/spectator-cpp) for publishing Atlas
metrics.

This is the original C++ client implementation, which was intended to be a fully-featured equivalent
of the Java reference implementation, with on-instance alerts, and other items which were deprecated
from the Spectator clients when we moved to using the Light Weight Client infrastructure.

This is only used by the [atlas-node-client](https://github.com/Netflix-Skunkworks/atlas-node-client)
library, which is deprecated.

## Building

```
./run-build.sh
./build/runtests
```
