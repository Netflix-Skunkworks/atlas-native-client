[![Snapshot](https://github.com/Netflix-Skunkworks/atlas-native-client/actions/workflows/snapshot.yml/badge.svg)](https://github.com/Netflix-Skunkworks/atlas-native-client/actions/workflows/snapshot.yml) [![Release](https://github.com/Netflix-Skunkworks/atlas-native-client/actions/workflows/release.yml/badge.svg)](https://github.com/Netflix-Skunkworks/atlas-native-client/actions/workflows/release.yml)

# Atlas Client Native Library

> :warning: This library is deprecated. If you need to publish metrics from C++ projects, then you
should use the [SpectatorD](https://github.com/Netflix-Skunkworks/spectatord) service directly,
which is considered the primary metrics publishing interface.

This is the original C++ client implementation, which was intended to be a fully-featured equivalent
of the Java reference implementation, with on-instance alerts, and other items which were deprecated
from the Spectator clients when we moved to using the Light Weight Client infrastructure.

This is only used by the [atlas-node-client](https://github.com/Netflix-Skunkworks/atlas-node-client)
library, which is deprecated.

## Building

```
./run-build.sh
```
