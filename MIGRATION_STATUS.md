# UDF Common V2 migration status

## Source and ownership

[Exact central baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf). The central Linux udf.ko passed Debian 13 linking and Linux 6.12 QEMU UDF 2.01 create/rename/write/remount tests (run 39932498942), not independent Common V2. Extract and adapt the existing implementation; preserve original licensing, attribution and provenance. New project-authored code uses GPL-3.0-or-later. No source transferred by this documentation commit.

## Qualification plan

1. Enumerate supported on-disk variants, validate geometry and features; implement bounded CORE probe.
2. Provide CORE mount/stat/list/read and VOLUME query using native ECMA-167/UDF descriptors, partition mapping, file entries, allocation descriptors, directory records, revision-specific VAT and metadata partitions.
3. Test independent mkudffs images by revision and partition type; reject corrupt descriptors, unsupported mapping and dirty state; compare independent payloads and unchanged-image hashes; inject malformed media and I/O faults.
4. Implement mutation, format, check/repair and DEFRAG map/plan/execute/verify/recover only with proven allocation, durability and interruption/recovery semantics. Unsupported operations must refuse before writing.
5. Qualify exact packaged Common V2 ABI, hosted and import-free freestanding profiles, then actual BOOT and kernel integration independently.

All independent stages, PDF manual and packaging are pending. No runtime capability is claimed.

The [design and qualification reference](docs/UDF.pdf) is available. It is not an operational manual; release and installation qualification remain pending.
