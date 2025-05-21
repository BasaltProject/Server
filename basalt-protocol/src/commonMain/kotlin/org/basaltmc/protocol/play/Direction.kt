package org.basaltmc.protocol.play

/** One of the six block faces. Ordinal order matches vanilla's own `Direction` enum exactly: both its varint id and its single-byte "3D data value" happen to equal the ordinal in this version. */
public enum class Direction {
    DOWN,
    UP,
    NORTH,
    SOUTH,
    WEST,
    EAST,
}
