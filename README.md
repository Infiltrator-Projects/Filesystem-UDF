# Filesystem-UDF

## 1. Purpose and format scope

Universal Disk Format (UDF), based on ECMA-167. This repository is the designated independent implementation home for InfiltratorOS. **Independent read-only Common V2 development provider. No released installable package yet.**

## 2. On-disk architecture and invariants

Volume descriptors, partition maps, file entries, allocation descriptors and directory records establish logical-to-physical mapping. VAT and metadata partitions are revision-specific; do not interpret one mapping mode as another. The qualified development profile is narrower than the complete format.

## 3. Common V2 implementation and capabilities

The local implementation exports only `infiltrator_filesystem_api_v2`. CORE provides probe, mount, unmount, stat, enumeration and read; VOLUME and METADATA are negotiated from the descriptor. Hosted and import-free AMD64 firmware builds share the native engine. Single physical partition, descriptor checksums and CRCs, closed integrity, Unicode names, FE/EFE, short/long/inline allocation descriptors, continuation extents, sparse data and symbolic links. Independent genisoimage UDF 1.02 media and alternate-descriptor test overlays passed. Other revisions and partition map types are not independently qualified; signed firmware boot is pending.

## 4. Integrity, mutation, allocation and recovery

Mutation, formatting, check/repair and DEFRAG map/plan/execute/verify/recover require format-specific allocation and ordering invariants, exclusive writable host access and demonstrated interruption recovery. Until qualified, reject them before writing. Preserve native disk semantics and do not add a kernel or FUSE fallback as an alternative filesystem authority.

## 5. Build, installation and release

The target installation is `/System/Filesystems/UDF.so` and `/System/Resources/Documentation/Filesystem-UDF.pdf`. Neither file is currently distributed as a release. For development, check out the pinned Libraries revision shown in CI, then run `make carrier` (or the exact CI build steps for UDF). The intended release remains a portable amd64 DEB and CPU-native source-first RUN, with GitHub's automatic source archive.

## 6. Qualification and limitations

Exact tested source [e2faadccedd5](https://github.com/Infiltrator-Projects/Filesystem-UDF/commit/e2faadccedd561e152c2fa37e7856628ed7bfab1); [passing provider CI 45955129622](https://github.com/Infiltrator-Projects/Filesystem-UDF/actions/runs/45955129622). Single physical partition, descriptor checksums and CRCs, closed integrity, Unicode names, FE/EFE, short/long/inline allocation descriptors, continuation extents, sparse data and symbolic links. Independent genisoimage UDF 1.02 media and alternate-descriptor test overlays passed. Other revisions and partition map types are not independently qualified; signed firmware boot is pending. Formatting, writes, repair, defragmentation and release remain unqualified.

## 7. Technical references and provenance

- [Technical reference (PDF)](docs/UDF.pdf) — original-format technical reference as one integrated manual and [PDF stream source](docs/UDF.pdfstreams); not an operational manual.

- [Pinned central source baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf)
- [Native design](native/filesystems/udf/DESIGN.md)
- [Migration and qualification plan](MIGRATION_STATUS.md)
- [Original-format reference source](docs/UDF.pdfstreams) and [PDF](docs/UDF.pdf)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

Preserve inherited notices and licences on source extraction; new project-authored code uses GPL-3.0-or-later. The [technical reference](docs/UDF-TECHNICAL-REFERENCE.md) expands original-format research. The PDF preserves both older documents as historical appendices; installation remains pending. Central code or test results are not evidence of an independent provider.
