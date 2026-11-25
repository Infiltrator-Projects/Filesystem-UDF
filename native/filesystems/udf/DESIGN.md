# UDF — native filesystem design

## Format scope

ECMA-167/UDF local block and optical storage; supported revisions and partition types explicit. This is a design contract, not a claim of implemented capabilities.

## On-disk structures and ownership

Anchor and descriptor sequences, descriptor tags/CRC, logical volume and partition maps, file-set and file entries, allocation descriptors, directory identifiers, VAT, sparable and metadata partition variants. The single native C/C++ engine owns these format semantics; consumers provide bounded Common V2 host services, never competing parsers.

## Integrity and mutation invariants

Validate tag locations and CRCs, logical-to-physical mappings, extent bounds and integrity state. Do not treat write-once, packet-written and random-writable media as interchangeable. Qualify revision-specific allocation and update rules before mutations. All arithmetic, offsets and lengths are checked before I/O. Writable operations require exclusive access, explicit ordering, flush and interruption/recovery evidence. Read-only operations leave the medium unchanged.

## Common V2 boundary

Only `infiltrator_filesystem_api_v2` is public. Target the genuine PROBE/MOUNT/UNMOUNT/STAT/LIST/READ operations first; negotiate actual supported operation masks. VOLUME, METADATA and DEFRAG operations (including MAP, PLAN, EXECUTE, VERIFY, RECOVER) are individually qualified or refused. Test Media uses the same provider's qualified format and namespace operations. No FUSE, legacy ABI, private driver or kernel proxy. Hosted and import-free freestanding profiles share the format engine, with generic environment-specific host services.

## Qualification

Use mkudffs images by revision and partition type, malformed descriptors and corrupt mappings. Historical central UDF 2.01 mount/create/rename/write/remount test run 39932498942 is kernel evidence, not independent Common V2 qualification. Compare file bytes against an independent oracle, hash unchanged read-only images, inject short I/O, corruption and interruption; validate exact published module separately in BOOT and kernel. Use disposable media only. No new independent binary or test pass is claimed.

## Source and migration

[Central source and historical design, pinned revision](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf). Adapt the retained implementation without erasing inherited notices or licences. New project-authored code uses GPL-3.0-or-later. See [migration status](../../../MIGRATION_STATUS.md), [philosophy](../../../docs/PHILOSOPHY.md) and [Common V2 contract](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md). Source migration is a separate step.
