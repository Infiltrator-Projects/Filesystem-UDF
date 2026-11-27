# UDF: technical reference

## History and revision boundaries

Universal Disk Format is the Optical Storage Technology Association's interoperable profile of ECMA-167 / ISO 13346 volume and file structures. Revisions 1.02, 1.50, 2.00/2.01, 2.50 and 2.60 have different allowed recording and partition-map mechanisms. UDF is used on DVD, Blu-ray, optical packet-written media and also block devices. It is not ISO 9660, although bridge media can carry both. The original profile, access type and logical block size govern what may be written. Write-once, rewritable, read-only and pseudo-overwrite profiles must not be treated as interchangeable mutable disks.

## Volume recognition and anchor discovery

Distinguish a physical medium sector, volume logical sector and partition logical block. Optical media commonly use 2048-byte sectors; other media differ. The Volume Recognition Sequence starts at byte 32768 relative to the session/volume origin and uses BEA01, NSR02 or NSR03, and TEA01 identifiers in volume structure descriptors. It is not sufficient evidence of a usable namespace. Anchor Volume Descriptor Pointers (tag 2) are sought at prescribed logical sectors, normally 256, N-256 and N (N is the last sector number, with profile exceptions). An anchor carries main and reserve descriptor-sequence extent descriptors at offsets 16 and 24; each has byte length and logical-sector position. Validate both sequences and select coherent descriptors according to sequence numbers and profile rules.

## Universal descriptor tag

Every tagged ECMA descriptor begins with a 16-byte tag: identifier 0, descriptor version 2, one-byte checksum 4, reserved 5, serial 6, CRC16 8, CRC length 10, tag location 12. Integers are little-endian. The tag checksum is the sum modulo 256 of the other fifteen tag bytes. Descriptor CRC uses the ECMA CRC-16 polynomial 0x1021, initial zero, on the declared body bytes immediately following the tag. A CRC length must fit the containing descriptor and allowed extent. Tag location is the logical location appropriate to the descriptor's address space, not always the absolute device sector. Check identifier, supported version, location, checksum and CRC before following pointers; do not accept corrupt structure just because the signature is recognisable.

## Volume descriptor sequence

Volume descriptors describe physical partitions, logical volumes, character sets and integrity sequences. Principal tag IDs: 1 primary volume, 2 anchor, 3 volume descriptor pointer, 4 implementation use, 5 partition, 6 logical volume, 7 unallocated space, 8 terminating, 9 logical volume integrity. Follow valid continuation descriptors with bounds and cycle limits. A partition descriptor has sequence number 16, flags 20, partition number 22, 32-byte contents identifier 24, 128-byte contents use 56, access type 184, partition start sector 188, length 192. A logical volume descriptor has 64-byte character-set field at 20, 128-byte identifier at 84, logical block bytes at 212, domain identifier at 216, contents use at 248, map table byte length 264, map count 268, integrity-sequence extent at 432 and map records from 440. Bound every variable record by both map length and descriptor capacity.

## Partition maps and translation

A logical-block address comprises a 32-bit block number and 16-bit partition-map reference. The reference indexes the logical volume's partition-map table, not the partition number itself. A type-1 six-byte map contains type/length at 0/1, volume sequence 2 and physical partition number 4. For a simple physical map, resolve the referenced partition and add its start sector to the block position using compatible logical units. Example: with 2048-byte sectors/blocks and partition start 1000, logical block 25 begins at byte 2099200. Type-2 maps include virtual, sparable and metadata partitions, each requiring additional translation. Validate volume sequence, partition bounds and mapping depth. Never use the physical formula for a virtual or metadata partition merely because a partition descriptor has a plausible start.

The ECMA-167 tag location, CRC, extent lengths and logical partition map must agree before file-set traversal. VAT, sparing and metadata partitions are separate mapping profiles; support depends on UDF revision and recording model.

## File set and ICB bootstrap

The logical volume's contents-use area encodes a long allocation descriptor for the file set. Resolve it through the partition maps and verify File Set Descriptor tag 256. In the FSD, the root-directory ICB long descriptor begins at 400, domain identifier at 416, next extent at 448 and stream-directory ICB at 464. The root ICB leads to a File Entry (tag 261) or Extended File Entry (tag 266), possibly using admitted indirect-entry strategies. An ICB tag is twenty bytes; strategy type is at 4, maximum entries 8, file type 11, parent logical address 12 and flags 18 (relative to ICB tag). Low three flags select allocation descriptor encoding. Validate ICB strategy, type, identity and continuation limits. An object identity is its resolved ICB address in the logical volume, not the current name.

## File Entry and Extended File Entry

Both records have descriptor tag 0..15, ICB tag 16..35, UID/GID/permissions at 36/40/44, 16-bit link count 48, record format/display 50/51 and record length 52. FE: information length 56, logical blocks recorded 64, three 12-byte timestamps at 72/84/96, checkpoint 108, external EA ICB 112, implementation identifier 128, unique ID 160, EA and allocation byte lengths 168/172, variable area at 176. EFE: information length 56, object size 64, blocks recorded 72, four timestamps 80/92/104/116, checkpoint 128, EA ICB 136, stream ICB 152, implementation identifier 168, unique ID 200, EA and allocation lengths 208/212, variable area at 216. Bound the EA and allocation areas independently. Select the layout from the tag and admitted revision, not merely record size.

## Allocation descriptors and file reads

Short allocation descriptors are eight bytes: length/type word at 0 and block position 4 within the ICB's partition. Long descriptors are sixteen bytes: length/type 0, logical block 4, partition-map reference 8, implementation bytes 10. Extended descriptors add recorded and information lengths; support them only where permitted by the UDF profile. High two bits of length encode recorded/allocated (0), unrecorded/allocated (1), unrecorded/unallocated (2), or next allocation-descriptor extent (3); low 30 bits give byte length. A continuation points to allocation metadata (tag 258) rather than file payload. Inline allocation places bytes in the file entry itself. Sum logical extent coverage, resolve recorded ranges, return zero for native unrecorded extents, clip to information length and detect loops. Do not read unrecorded disk bytes.

## Directory records and names

A File Identifier Descriptor (tag 257) has a 38-byte fixed prefix: file version at 16, characteristics 18, identifier byte length 19, target ICB long descriptor 20, implementation-use length 36. Implementation bytes follow at 38, then filename and padding to a four-byte boundary. Parent entries, deleted entries, hidden and metadata entries are distinguished by characteristic flags; the parent entry normally has no filename. A 19-byte name and four-byte implementation area give align_up(38+19+4,4)=64 bytes. Names use OSTA Compressed Unicode CS0; leading compression ID 8 or 16 selects one-byte or big-endian two-byte code units. A fixed-width dstring has its encoded length in the last byte, unlike an FID name. Compare decoded names under the admitted profile, verify tag location and CRC, and resolve the target ICB instead of copying a guessed inode.

## Virtual and sparable partitions

The Virtual Allocation Table (VAT) translates virtual block indexes to physical partition positions and permits logical changes on write-once media. VAT layouts and discovery differ between revisions 1.50 and 2.x; the newest valid VAT and its previous-VAT links are essential to coherent session interpretation. An unmapped entry is not a physical all-ones address. A sparable partition relocates defective packets through validated sparing tables; map the packet start and preserve the in-packet displacement, checking table version and alignment. Duplicate sparing tables may disagree; choose only a valid, coherent generation. These maps can be layered with profile-specific restrictions. An implementation that handles only physical type-1 maps must reject or explicitly limit virtual/sparable media, not report generic UDF compatibility.

## Metadata partitions and free space

UDF 2.50 introduced metadata partitions: metadata file extents map logical metadata blocks, with optional mirror and bitmap files. Validate their special ICBs, allocation unit size, alignment and map references; ordinary payload and metadata storage may use different mappings. The logical volume integrity descriptor records integrity state, partition free-space/size tables and revision-specific implementation fields. Space bitmap descriptors use a tagged header followed by bit and byte counts and bitmap bytes; native unallocated-space tables are an alternative. Reconcile the selected partition's actual free-space representation with allocations and reserved structures. Sequential write-once media cannot reclaim old sectors by clearing a bitmap. Do not report physically unwritable capacity as generic free space.

## Mutation, durability and recovery

For rewritable media, reserve target ranges and ICBs, write and verify data, publish allocation and namespace metadata under the profile's integrity ordering, and update free-space accounting. Sequential media instead append replacement structures and publish new mapping state according to VAT or pseudo-overwrite rules. UDF descriptor CRCs detect corruption but do not constitute a universal metadata journal. A closed integrity sequence is not proof that all data is valid. A formatter must produce the correct recognition, anchors, descriptor sequences, partition maps, integrity, file set and root for its selected revision. Defragmentation requires a writable profile, verified relocation and updated ICB ownership; reject write-once and unsupported metadata-map cases rather than applying a generic block mover.

## Implementation and qualification boundary

The independent provider uses Common V2: https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md . Negotiate actual operation masks and media profiles. Distinguish unsupported, corrupt, I/O and stale-handle results. Require bounded host I/O, exclusive unmounted ownership and durable flush before mutation. Test Media and Defragmenter consume the same engine, not separate format parsers. Hosted and freestanding binaries, relocation, recovery, BOOT, kernel integration and packages require separate exact-source evidence. Current support belongs to the repository README and qualification records. The intended package paths are /System/Filesystems/ and /System/Resources/Documentation/; the reference is not an installation or release-status claim.

## Normative references and validation

ECMA-167 and OSTA's revision-specific UDF profiles are the format authority: https://ecma-international.org/publications-and-standards/technical-reports/ecma-tr-112/ . Independent pinned structure cross-check: https://github.com/torvalds/linux/blob/v6.12/fs/udf/ecma_167.h and fs/udf/osta_udf.h. Algorithms in fs/udf/super.c, partition.c, inode.c, directory.c and unicode.c. Use independent udftools mkudffs and udfinfo, optical images and revision-specific fixtures. Test corrupted anchors, conflicting descriptor sequences, CRC/tag-location failures, map indirection, VAT generations, sparing and metadata mirrors, short/long/inline extents, names, streams and interrupted writes. Exact map-variant structures, sequential-media discovery, timestamp semantics and revision-specific write constraints still need detailed normative validation. Operational provider qualification is separate.

Historical central UDF 2.01 Linux mount/create/rename/write/remount test run 39932498942 is provenance, not independent Common V2 qualification.

Retained source: https://github.com/Infiltrator-Projects/Filesystems/blob/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/udf/linux/ecma_167.h (blob 415b050b97).
