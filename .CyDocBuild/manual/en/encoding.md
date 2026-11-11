---
id: encoding
title: Encoding and line endings
description: Convert a document deliberately before saving it.
section: Using Reader
order: 40
parent: editing
revision: 3
---

## Convert {#convert}

The Encoding menu selects UTF-8, UTF-8 BOM, UTF-16 LE BOM, UTF-16 BE BOM or Windows-1252 and LF/CRLF/CR line endings. The change stays in memory and marks the document dirty. Save explicitly to write the selected representation.

Use Open Windows-1252 for legacy files that need it; Reader never guesses that encoding. Invalid input and unrepresentable output characters are rejected rather than replaced with lossy placeholders. An unchanged save preserves the original bytes; edited saves retain the detected newline convention unless explicitly changed. UTF-8 offsets in SDK text operations refer to normalized live text, not byte positions in the original UTF-16 file.


The [paged profile](doc:paged-text#save) preserves native UTF-8/BOM and line endings without encoding conversion.
