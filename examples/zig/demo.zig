// Syntax sample only: no compiler or language server is bundled.
const std = @import("std");

/// Unicode remains source text: été 🛠
pub fn clamp(value: i24) i24 {
    return if (value < 0) 0 else value;
}

const message =
    \\A raw string containing é and 🛠
    \\const is text on this line
;
const @"quoted name" = 0x2a;
