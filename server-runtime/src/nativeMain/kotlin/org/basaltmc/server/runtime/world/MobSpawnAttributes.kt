package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.world.MobSpawnVariantData.PANDA_GENES_BY_NAME
import org.basaltmc.wire.nbt.NbtCompound

/** Random attribute bases selected by concrete mob finalizers before the common mob layer. */
internal object MobSpawnAttributes {
    fun write(
        type: String,
        random: BasaltRandom,
        data: NbtCompound,
        attributes: MutableMap<String, Double>,
    ) {
        when (type) {
            "horse" -> {
                attributes[MAX_HEALTH] = horseMaxHealth(random)
                attributes[MOVEMENT_SPEED] = horseSpeed(random)
                attributes[JUMP_STRENGTH] = horseJumpStrength(random)
            }
            "donkey", "llama", "mule", "trader_llama" ->
                attributes[MAX_HEALTH] = horseMaxHealth(random)
            "skeleton_horse" -> attributes[JUMP_STRENGTH] = horseJumpStrength(random)
            "panda" -> writePanda(data, attributes)
        }
    }

    fun pandaGene(roll: Int): PandaGene =
        when (roll) {
            0 -> PandaGene.LAZY
            1 -> PandaGene.WORRIED
            2 -> PandaGene.PLAYFUL
            4 -> PandaGene.AGGRESSIVE
            in 0..8 -> PandaGene.WEAK
            9, 10 -> PandaGene.BROWN
            else -> PandaGene.NORMAL
        }

    private fun writePanda(
        data: NbtCompound,
        attributes: MutableMap<String, Double>,
    ) {
        val main = PANDA_GENES_BY_NAME[data.getString("MainGene")] ?: PandaGene.NORMAL
        val hidden = PANDA_GENES_BY_NAME[data.getString("HiddenGene")] ?: PandaGene.NORMAL
        val expressed = if (main.recessive && main != hidden) PandaGene.NORMAL else main
        when (expressed) {
            PandaGene.WEAK -> attributes[MAX_HEALTH] = PANDA_WEAK_HEALTH
            PandaGene.LAZY -> attributes[MOVEMENT_SPEED] = PANDA_LAZY_SPEED
            else -> Unit
        }
    }

    private fun horseMaxHealth(random: BasaltRandom): Double =
        (HORSE_BASE_HEALTH + random.nextInt(8) + random.nextInt(9)).toDouble()

    private fun horseSpeed(random: BasaltRandom): Double =
        (0.45f + random.nextDouble() * 0.3 + random.nextDouble() * 0.3 + random.nextDouble() * 0.3) * 0.25

    private fun horseJumpStrength(random: BasaltRandom): Double =
        0.4f + random.nextDouble() * 0.2 + random.nextDouble() * 0.2 + random.nextDouble() * 0.2

    private const val MAX_HEALTH = "minecraft:max_health"
    private const val MOVEMENT_SPEED = "minecraft:movement_speed"
    private const val JUMP_STRENGTH = "minecraft:jump_strength"
    private const val HORSE_BASE_HEALTH = 15
    private const val PANDA_WEAK_HEALTH = 10.0
    private const val PANDA_LAZY_SPEED = 0.07000000029802322
}
