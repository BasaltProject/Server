package org.basaltmc.server.registry

/** One row of the generated hardness table. */
internal class BlockHardnessEntry(
    val path: String,
    val destroyTime: Float,
    val requiresCorrectTool: Boolean,
    val explosionResistance: Float,
)
