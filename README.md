# Filesystem-UDF

Independent Universal Disk Format provider for InfiltratorOS. Target: one native C/C++ engine and one distributable `UDF.so`.

**Status: documentation only; source extraction pending.** [Central source/design](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf). The central Linux udf.ko passed Debian 13 linking and Linux 6.12 QEMU UDF 2.01 create/rename/write/remount tests (run 39932498942), not independent Common V2. No independent driver or release exists.

## Common V2

Use only `infiltrator_filesystem_api_v2` from [Libraries](https://github.com/Infiltrator-Projects/Libraries). Own ECMA-167/UDF descriptors, partition mapping, file entries, allocation descriptors, directory records, revision-specific VAT and metadata partitions in the same engine for hosted and freestanding execution. No FUSE, separate per-format kernel/EFI driver or legacy ABI.

## Build and install

Planned: `/System/Filesystems/UDF.so` and `/System/Resources/Documentation/Filesystem-UDF.pdf`. Generic amd64 .deb, source-downloading CPU-native .run and GitHub-generated source ZIP only. No build or installer exists yet.

See [migration status](MIGRATION_STATUS.md) and [engineering philosophy](docs/PHILOSOPHY.md).
