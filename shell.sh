#!/bin/bash

podman run --rm -it -v $(pwd):/usr/src/project:Z my-switch-builder /bin/bash

