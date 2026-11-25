# Filesystem-UDF

## 1. Purpose and format scope

Universal Disk Format (UDF), based on ECMA-167. This repository is the designated independent implementation home for InfiltratorOS. **Documentation and migration scaffold only: no independent runtime provider or release is present.**

## 2. On-disk architecture and invariants

Volume descriptors, partition maps, file entries, allocation descriptors and directory records establish logical-to-physical mapping. VAT and metadata partitions are revision-specific; do not interpret one mapping mode as another. These are design requirements for the native implementation, not claims about code already present in this repository.

## 3. Common V2 implementation and capabilities

No local Common V2 implementation or advertised operation set has yet been qualified. The target entry point is `infiltrator_filesystem_api_v2`, using bounded, partition-relative host I/O. Implement native probe first, then mount, stat, enumeration, read and volume query where applicable. Consumers must negotiate supported groups and profiles; an unsupported operation must not report success or delegate to a private driver.

## 4. Integrity, mutation, allocation and recovery

Mutation, formatting, check/repair and DEFRAG map/plan/execute/verify/recover require format-specific allocation and ordering invariants, exclusive writable host access and demonstrated interruption recovery. Until qualified, reject them before writing. Preserve native disk semantics and do not add a kernel or FUSE fallback as an alternative filesystem authority.

## 5. Build, installation and release

The target installation is `/System/Filesystems/UDF.so` and `/System/Resources/Documentation/Filesystem-UDF.pdf`. Neither file is currently distributed by this repository. Once the implementation qualifies, the intended release is a portable amd64 DEB and CPU-native source-first RUN, with GitHub's automatic source archive. No install or build command is advertised before an executable build exists.

## 6. Qualification and limitations

Independent mkudffs images across admitted revisions and partition types, corrupt descriptors, unsupported mappings and dirty state. Historical central Linux tests do not qualify the independent V2 provider. Compare independent payloads and unchanged-image hashes; inject truncation, corruption and I/O faults. Establish exact-revision ABI, hosted and import-free freestanding qualification, then independently test BOOT and kernel integration. No local build, runtime or packaging qualification is claimed.

## 7. Technical references and provenance

- [Pinned central source baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf)
- [Native design](native/filesystems/udf/DESIGN.md)
- [Migration and qualification plan](MIGRATION_STATUS.md)
- [Original-format reference source](docs/UDF.pages) and [PDF](docs/UDF.pdf)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

Preserve inherited notices and licences on source extraction; new project-authored code uses GPL-3.0-or-later. The original-format reference is available here; installation of the PDF remains pending. Central code or test results are not evidence of an independent provider.
