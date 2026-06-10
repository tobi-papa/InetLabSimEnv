"""Import shim for the compiled C++ premerge_kernel module."""
import os, sys
_BUILD = os.environ.get("PREMERGE_BUILD_DIR",
    os.path.join(os.path.dirname(__file__), "..", "..", "graph-entropy-sandbox", "build"))
sys.path.insert(0, os.path.abspath(_BUILD))
import premerge_kernel as k  # noqa: E402
h1 = k.h1; h_partition = k.h_partition; decompose = k.decompose
merge = k.merge; overlaps = k.overlaps; h2_min = k.h2_min
build_message = k.build_message; reconstruct = k.reconstruct; bias_bounds = k.bias_bounds
