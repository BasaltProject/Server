package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.FireOdds
import org.basaltmc.wire.common.Identifier

/**
 * Fire burning out and spreading.
 *
 * parity: `FireBlock.tick`, whose numbers are all vanilla's: how readily each block catches and
 * burns away comes from the two tables that class builds at bootstrap, and the rest is its own
 * arithmetic over the fire's age.
 *
 * What is not here: rain putting fire out asks whether the sky is open above rather than whether
 * this biome has rain at all, since biome precipitation is not modelled yet.
 */
public object Fires {
    /** parity: `FireBlock.getFireTickDelay`. */
    public fun tickDelay(random: LegacyRandom): Int = MIN_DELAY + random.nextInt(DELAY_SPREAD)

    public fun isFire(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "fire"

    /**
     * Lights a fire where there is room for one, answering whether it caught.
     *
     * parity: `FlintAndSteelDispenseItemBehavior` and the same check a player's flint and steel
     * makes: the space has to be empty and the fire has to have something to stand on.
     */
    public fun ignite(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val existing = world.state.getBlockState(x, y, z) ?: return false
        if (existing != Blocks.AIR.stateId) return false
        val fire = BlockStateDefinitions.stateIdOf(Identifier.vanilla("fire"), emptyMap()) ?: return false
        if (!NeighbourUpdates.isSupported(world, x, y, z, fire)) return false
        if (!world.state.setBlockState(x, y, z, fire)) return false
        schedule(world, x, y, z, LegacyRandom(world.gameTime))
        return true
    }

    /** Asks for the fire at these coordinates to act after the usual delay. */
    public fun schedule(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
    ) {
        world.state.scheduleBlockTick(x, y, z, TICK_TYPE, tickDelay(random))
    }

    /**
     * One fire's turn: age it, put it out if it has nothing left to burn, burn what is beside it
     * and try to set light to what is near. Answers every block that changed.
     */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
    ): List<BlockChange> {
        val stateId = world.state.getBlockState(x, y, z) ?: return emptyList()
        if (!isFire(stateId)) return emptyList()
        val changes = mutableListOf<BlockChange>()
        val block = BlockStateDefinitions.blockOf(stateId) ?: return emptyList()
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: 0

        if (!NeighbourUpdates.isSupported(world, x, y, z, stateId)) {
            put(world, x, y, z, Blocks.AIR.stateId, changes)
            return changes
        }
        if (rainingOn(world, x, y, z) && random.nextFloat() < RAIN_BASE + age * RAIN_PER_AGE) {
            put(world, x, y, z, Blocks.AIR.stateId, changes)
            return changes
        }

        val aged = minOf(MAX_AGE, age + random.nextInt(3) / 2)
        if (aged != age) {
            BlockStateDefinitions.stateIdOf(block, mapOf("age" to aged.toString()), stateId)?.let {
                put(world, x, y, z, it, changes)
            }
        }
        schedule(world, x, y, z, random)

        if (!burnsForever(world, x, y, z)) {
            if (!hasSomethingToBurn(world, x, y, z)) {
                val below = stateAt(world, x, y - 1, z)
                if (!BlockShapes.isFaceSturdy(below, BlockFace.UP) || aged > OLD_ENOUGH_TO_DIE) {
                    put(world, x, y, z, Blocks.AIR.stateId, changes)
                }
                return changes
            }
            if (aged == MAX_AGE && random.nextInt(4) == 0 && FireOdds.burnOdds(stateAt(world, x, y - 1, z)) == 0) {
                put(world, x, y, z, Blocks.AIR.stateId, changes)
                return changes
            }
        }

        burnOut(world, x + 1, y, z, SIDE_CHANCE, random, aged, changes)
        burnOut(world, x - 1, y, z, SIDE_CHANCE, random, aged, changes)
        burnOut(world, x, y - 1, z, VERTICAL_CHANCE, random, aged, changes)
        burnOut(world, x, y + 1, z, VERTICAL_CHANCE, random, aged, changes)
        burnOut(world, x, y, z - 1, SIDE_CHANCE, random, aged, changes)
        burnOut(world, x, y, z + 1, SIDE_CHANCE, random, aged, changes)
        spread(world, x, y, z, random, aged, changes)
        return changes
    }

    /** parity: the 3x3 column from one below to four above, with the odds thinning as it climbs. */
    private fun spread(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        age: Int,
        changes: MutableList<BlockChange>,
    ) {
        val difficulty = difficultyOf(world)
        for (dx in -1..1) {
            for (dz in -1..1) {
                for (dy in -1..4) {
                    if (dx == 0 && dy == 0 && dz == 0) continue
                    var rate = BASE_RATE
                    if (dy > 1) rate += (dy - 1) * BASE_RATE
                    val tx = x + dx
                    val ty = y + dy
                    val tz = z + dz
                    val ignite = igniteOddsAround(world, tx, ty, tz)
                    if (ignite <= 0) continue
                    val odds = (ignite + IGNITE_BONUS + difficulty * DIFFICULTY_WEIGHT) / (age + AGE_WEIGHT)
                    if (odds <= 0 || random.nextInt(rate) > odds) continue
                    if (rainingOn(world, tx, ty, tz)) continue
                    val spreadAge = minOf(MAX_AGE, age + random.nextInt(5) / 4)
                    put(world, tx, ty, tz, fireFor(world, tx, ty, tz, spreadAge) ?: continue, changes)
                    schedule(world, tx, ty, tz, random)
                }
            }
        }
    }

    /** parity: `FireBlock.checkBurnOut`, which either takes the block or leaves fire in its place. */
    private fun burnOut(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        chance: Int,
        random: LegacyRandom,
        age: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = stateAt(world, x, y, z)
        if (random.nextInt(chance) >= FireOdds.burnOdds(stateId)) return
        if (random.nextInt(age + BURN_AGE_BIAS) < BURN_AGE_LIMIT && !rainingOn(world, x, y, z)) {
            val newAge = minOf(age + random.nextInt(5) / 4, MAX_AGE)
            put(world, x, y, z, fireFor(world, x, y, z, newAge) ?: return, changes)
            schedule(world, x, y, z, random)
            return
        }
        put(world, x, y, z, Blocks.AIR.stateId, changes)
    }

    /** parity: `FireBlock.getIgniteOdds(level, pos)`: the best of the six around an empty block. */
    private fun igniteOddsAround(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        if (stateAt(world, x, y, z) != Blocks.AIR.stateId) return 0
        var odds = 0
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x, y - 1, z)))
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x, y + 1, z)))
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x, y, z - 1)))
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x, y, z + 1)))
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x - 1, y, z)))
        odds = maxOf(odds, FireOdds.igniteOdds(stateAt(world, x + 1, y, z)))
        return odds
    }

    /** parity: `FireBlock.isValidFireLocation`, which is any of the six being flammable at all. */
    private fun hasSomethingToBurn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        FireOdds.burnOdds(stateAt(world, x, y - 1, z)) > 0 ||
            FireOdds.burnOdds(stateAt(world, x, y + 1, z)) > 0 ||
            FireOdds.burnOdds(stateAt(world, x, y, z - 1)) > 0 ||
            FireOdds.burnOdds(stateAt(world, x, y, z + 1)) > 0 ||
            FireOdds.burnOdds(stateAt(world, x - 1, y, z)) > 0 ||
            FireOdds.burnOdds(stateAt(world, x + 1, y, z)) > 0

    /** parity: the dimension's `infiniburn` tag, which in the Nether is what keeps fire alight. */
    private fun burnsForever(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val below = BlockStateDefinitions.blockOf(stateAt(world, x, y - 1, z))?.path ?: return false
        return below == "netherrack" || below == "magma_block" || below == "bedrock"
    }

    /** Soul fire where the ground calls for it, ordinary fire everywhere else. */
    private fun fireFor(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        age: Int,
    ): Int? {
        if (BlockConnections.isSoulFireBase(stateAt(world, x, y - 1, z))) {
            return Blocks.REGISTRY.idOf(Identifier.vanilla("soul_fire"))
        }
        return BlockStateDefinitions.stateIdOf(Identifier.vanilla("fire"), mapOf("age" to age.toString()))
    }

    /**
     * Whether rain is falling on this block.
     *
     * parity: `Level.isRainingAt` without its biome half: it asks whether the sky is open, which is
     * the part that decides whether a roof keeps a fire alight.
     */
    private fun rainingOn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (!world.weather.raining) return false
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        return chunk.heightAt(x and 15, z and 15, org.basaltmc.server.world.Chunk.HeightmapType.MOTION_BLOCKING) <= y
    }

    private fun difficultyOf(world: LoadedWorld): Int =
        when (world.settings.difficulty) {
            "peaceful" -> 0
            "easy" -> 1
            "normal" -> 2
            else -> 3
        }

    private fun stateAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId

    private fun put(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        if (!world.state.setBlockState(x, y, z, stateId)) return
        changes += BlockChange(x, y, z, stateId)
    }

    /** What a scheduled tick on a fire is filed under. */
    public val TICK_TYPE: Identifier = Identifier.vanilla("fire")

    private const val MIN_DELAY = 30
    private const val DELAY_SPREAD = 10
    private const val MAX_AGE = 15
    private const val OLD_ENOUGH_TO_DIE = 3
    private const val RAIN_BASE = 0.2f
    private const val RAIN_PER_AGE = 0.03f
    private const val SIDE_CHANCE = 300
    private const val VERTICAL_CHANCE = 250
    private const val BASE_RATE = 100
    private const val IGNITE_BONUS = 40
    private const val DIFFICULTY_WEIGHT = 7
    private const val AGE_WEIGHT = 30
    private const val BURN_AGE_BIAS = 10
    private const val BURN_AGE_LIMIT = 5
}
