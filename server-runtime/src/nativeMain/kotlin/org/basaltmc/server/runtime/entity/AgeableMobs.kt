package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier

/**
 * Entity types that inherit Minecraft 26.2's `AgeableMob` synchronized baby state.
 *
 * This is deliberately not the breeding-food table: squid, cubes and villagers are ageable too,
 * while whether Basalt has implemented a particular mating interaction is a separate question.
 */
public object AgeableMobs {
    /** parity: `AgeableMob.DATA_BABY_ID`, after Entity/LivingEntity/PathfinderMob data. */
    public const val BABY_DATA_INDEX: Int = 16

    /** parity: `AgeableMob.BABY_START_AGE`. */
    public const val BABY_START_AGE: Int = -24_000

    public fun isAgeable(type: Identifier): Boolean = isAgeable(type.path)

    public fun isAgeable(typePath: String): Boolean = typePath in TYPES

    private val TYPES =
        setOf(
            "armadillo",
            "axolotl",
            "bee",
            "camel",
            "camel_husk",
            "cat",
            "chicken",
            "cow",
            "dolphin",
            "donkey",
            "fox",
            "frog",
            "glow_squid",
            "goat",
            "happy_ghast",
            "hoglin",
            "horse",
            "llama",
            "magma_cube",
            "mooshroom",
            "mule",
            "nautilus",
            "ocelot",
            "panda",
            "parrot",
            "pig",
            "polar_bear",
            "rabbit",
            "sheep",
            "skeleton_horse",
            "slime",
            "sniffer",
            "squid",
            "strider",
            "sulfur_cube",
            "trader_llama",
            "turtle",
            "villager",
            "wandering_trader",
            "wolf",
            "zombie_horse",
            "zombie_nautilus",
        )
}
