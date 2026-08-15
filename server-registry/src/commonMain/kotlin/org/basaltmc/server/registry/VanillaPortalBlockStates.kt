package org.basaltmc.server.registry

/**
 * // parity: the state definitions of the two portal blocks, from the official `blocks.json`
 * report. A portal is the one place where a property changes what the block *does*, not only how it
 * looks: a Nether portal's axis is which way it faces, and an end portal frame's eye is whether the
 * ring is armed.
 */
internal val PORTAL_BLOCK_STATE_SPECS: List<String> =
    listOf(
        "7017|nether_portal|axis=x,z",
        "9469|end_portal_frame|eye=true,false;facing=north,south,west,east",
    )
