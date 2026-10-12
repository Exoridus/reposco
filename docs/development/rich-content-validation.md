# Rich clipboard validation

The experimental Windows slice preserves Unicode text, registered `HTML Format` (CF_HTML), `Rich Text Format`, `PNG`, and bounded `CF_DIB`/`CF_DIBV5`. Enter offers original supported representations, Shift+Enter offers plain text, Ctrl+Enter copies originals. Destination applications choose formats; successful input dispatch does not prove universal application compatibility. Image-only items have no plain-text replay. HTML-only items derive a text fallback without modifying stored HTML.

## Admission and preview limits

- Total original payload: 32 MiB; plain text: 16 MiB; each HTML/RTF representation: 1 MiB.
- CF_HTML byte offsets and UTF-8 must be valid. HTML import is bounded to 512 tags, depth 32, 4096 characters per tag and spans 1-6. Declarations, comments, backslashes in tags and malformed closing tags are rejected. Malformed color attributes are rejected before Qt can log their values.
- Images: maximum 8192 pixels per dimension and 16 million total pixels. PNG ends at IEND; DIB ends at its validated pixel extent. Allocation padding is not stored. Supported DIB headers are 40/108/124 bytes, 16/24/32-bit RGB or supported bitfields; palettes, RLE and embedded color profiles are rejected.
- RTF requires an accompanying admitted plain/HTML text representation and balanced groups of depth <=64. Embedded objects, binary data, fields and file tables are rejected. RTF preview is plain text, not a full RTF renderer.
- Exclusion markers and native executable rules precede payload reads. Exact-code filtering applies to accompanying plain text and derived HTML text before fingerprints, preview, indexing and persistence. No arbitrary password recognition is claimed.
- Preview fallback: 8192 characters; rendered markup: 32768 characters and 420x2048 pixels; image thumbnails: 960x960; table grid: first 12 rows, 6 columns, 128 characters per cell. Resource loading never delegates to file/network loaders. Rendering uses contrast-safe text colors without altering originals.

## Storage compatibility

Schema v1 migrates transactionally to v2, retaining IDs, text fingerprints, recency and originals. The encrypted database receives additional format/kind columns. Migration failure rolls back; unknown schemas remain locked. An older v1 binary refuses the migrated v2 database. There is no automatic downgrade or plaintext export; keep an independent encrypted profile backup before manually switching back to an older binary.

Policy changes pause admission and remain pending until the worker drains preceding operations and durably saves settings. A final admission check rolls back obsolete transactions, including dedup/eviction. A COMMIT already admitted may finish before the acknowledgement; no obsolete capture finishes after the new policy is confirmed effective. Existing entries require separate confirmed deletion.

## Reproducible checks

Use `./tools/dev.ps1 quick` for the default inactive clipboard suite and `./tools/dev.ps1 full` once for integrated validation. Local builds default to six jobs; CI uses all available processors. `content_tests` checks migration, raw-format preservation, OTP HTML boundaries, offline resource blocking, parser/import limits and bitmap/image bounds. `history_flow_tests` exercises policy acknowledgement and rich original/plain replay through the worker. QML and desktop fixtures verify rich preview, source fallback and keyboard selection.

Active native tests require isolation or explicit current desktop coordination. The coordinated `richFormatsRoundTripWithoutRecapture` fixture writes synthetic Unicode/HTML/RTF/3x2 PNG formats, verifies OS publication and raw capture, and verifies own writes do not recapture. `exactCodesStayInPrivateClipboardAndOutOfHistory` verifies synthetic OTP exclusion while OS contents remain intact. Neither test reads or saves the user's previous clipboard contents.

Real Word/Excel/browser formatting fidelity, arbitrary RTF, image paste into third-party editors, actual Bitwarden extension behavior, multiple displays, virtual desktops and all DPI combinations still require manual testing. Use throwaway credentials only. Never infer protection for unmarked passwords from OTP tests or encryption at rest.
