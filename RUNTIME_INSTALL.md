# UDF native installation

The runtime release builds the read-only Common V2 provider at the pinned source revision and installs `/System/Filesystems/UDF.so` and `/System/Resources/Documentation/Filesystem-UDF.pdf`. Unsupported operations are not advertised. Native compilation uses CPU-specific optimisation; no PGO or physical firmware qualification is claimed.
The native build uses genisoimage only as an independent test oracle; it is not a production filesystem implementation or runtime dependency.
