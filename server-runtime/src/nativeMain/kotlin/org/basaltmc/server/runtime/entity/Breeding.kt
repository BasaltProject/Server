package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier

/**
 * Feeding two animals and what comes of it.
 *
 * parity: `Animal`: the right food puts one in love for six hundred ticks, two in love within eight
 * blocks walk together, and after sixty ticks within three blocks they breed. The parents then wait
 * five minutes before they can again, and the calf is a baby for twenty minutes.
 */
public object Breeding {
    /** parity: `Animal.setInLove`, thirty seconds of it. */
    public const val LOVE_TICKS: Int = 600

    /** parity: `PARENT_AGE_AFTER_BREEDING`, five minutes before either can breed again. */
    public const val PARENT_COOLDOWN: Int = 6000

    /** parity: `AgeableMob.BABY_START_AGE`, twenty minutes of being small. */
    public const val BABY_AGE: Int = -24000

    /** parity: `BreedGoal`: partners are looked for within eight blocks. */
    public const val PARTNER_RANGE: Double = 8.0

    /** parity: `BreedGoal.tick`, which needs sixty ticks together and three blocks between them. */
    public const val COURTSHIP_TICKS: Int = 60
    public const val BREED_DISTANCE_SQUARED: Double = 9.0

    /**
     * parity: the `isFood` of each animal class. Only the ones this build can actually spawn are
     * listed; anything else refuses the food rather than pretending.
     */
    private val FOOD: Map<String, Set<String>> =
        mapOf(
            "cow" to setOf("wheat"),
            "mooshroom" to setOf("wheat"),
            "sheep" to setOf("wheat"),
            "goat" to setOf("wheat"),
            "pig" to setOf("carrot", "potato", "beetroot"),
            "chicken" to setOf("wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds", "torchflower_seeds"),
            "rabbit" to setOf("carrot", "golden_carrot", "dandelion"),
            "horse" to setOf("golden_carrot", "golden_apple", "enchanted_golden_apple"),
            "donkey" to setOf("golden_carrot", "golden_apple", "enchanted_golden_apple"),
            "llama" to setOf("hay_block"),
            "wolf" to setOf("beef", "porkchop", "chicken", "mutton", "rabbit"),
            "cat" to setOf("cod", "salmon"),
            "ocelot" to setOf("cod", "salmon"),
            "fox" to setOf("sweet_berries", "glow_berries"),
            "panda" to setOf("bamboo"),
            "turtle" to setOf("seagrass"),
            "bee" to setOf("dandelion", "poppy", "sunflower", "allium"),
            "axolotl" to setOf("tropical_fish_bucket"),
            "frog" to setOf("slime_ball"),
            "camel" to setOf("cactus"),
            "sniffer" to setOf("torchflower_seeds"),
            "armadillo" to setOf("spider_eye"),
            "strider" to setOf("warped_fungus"),
            "hoglin" to setOf("crimson_fungus"),
        )

    /** Whether this animal eats this, which is what puts it in love. */
    public fun isFood(
        type: Identifier,
        item: Identifier,
    ): Boolean = FOOD[type.path]?.contains(item.path) == true

    /**
     * parity: `Animal.mobInteract`'s own gate: `age == 0 && canFallInLove()`. A calf is too young and
     * a parent that just bred is counting down, and neither is zero.
     */
    public fun canFallInLove(animal: ServerEntity): Boolean = animal.loveTicks <= 0 && animal.age == 0

    /** parity: `setInLove`, which is what a fed animal comes away with. */
    public fun feed(animal: ServerEntity) {
        animal.loveTicks = LOVE_TICKS
    }

    /** parity: `canMate`: both in love, the same kind, and not the same animal twice. */
    public fun canMate(
        animal: ServerEntity,
        other: ServerEntity,
    ): Boolean =
        animal.entityId != other.entityId &&
            animal.type == other.type &&
            animal.loveTicks > 0 &&
            other.loveTicks > 0

    /**
     * One tick of being in love. Answers the partner it should breed with now, or null.
     *
     * parity: `Animal.aiStep`'s countdown and `BreedGoal.tick`'s sixty ticks: they have to be near
     * each other for three seconds before anything comes of it.
     */
    public fun tick(
        animal: ServerEntity,
        nearby: List<ServerEntity>,
    ): ServerEntity? {
        if (animal.breedCooldown > 0) animal.breedCooldown--
        // parity: a baby's love is cleared every tick, so feeding one does nothing at all.
        if (animal.age != 0) {
            animal.loveTicks = 0
            growUp(animal)
            return null
        }
        if (animal.loveTicks <= 0) {
            animal.courtshipTicks = 0
            return null
        }
        animal.loveTicks--

        val partner =
            nearby
                .filter { canMate(animal, it) }
                .minByOrNull { MobBrain.distanceTo(it, animal.x, animal.y, animal.z) }
        if (partner == null || MobBrain.distanceTo(partner, animal.x, animal.y, animal.z) > PARTNER_RANGE) {
            animal.courtshipTicks = 0
            return null
        }
        animal.courtshipTicks++
        val distance = MobBrain.distanceTo(partner, animal.x, animal.y, animal.z)
        if (animal.courtshipTicks < COURTSHIP_TICKS || distance * distance >= BREED_DISTANCE_SQUARED) return null
        return partner
    }

    /** parity: `spawnChildFromBreeding`: both parents come out of love and on cooldown. */
    public fun afterBreeding(
        animal: ServerEntity,
        partner: ServerEntity,
    ) {
        for (parent in listOf(animal, partner)) {
            parent.loveTicks = 0
            parent.courtshipTicks = 0
            parent.age = PARENT_COOLDOWN
            parent.breedCooldown = PARENT_COOLDOWN
        }
    }

    /** parity: `AgeableMob.ageUp`, one tick at a time until it is grown. */
    public fun growUp(animal: ServerEntity) {
        if (animal.age < 0) {
            animal.age++
        } else if (animal.age > 0) {
            animal.age--
        }
    }
}
