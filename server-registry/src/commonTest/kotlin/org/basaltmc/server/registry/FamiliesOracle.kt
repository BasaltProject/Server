package org.basaltmc.server.registry

import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the state-family arithmetic, over every block state rather
 * than a sample: these are offsets from a family's default, so an off-by-one shows up in one state
 * out of thirty thousand and nowhere else.
 */
class FamiliesOracle {
    private fun fold(name: String, of: (Int) -> Long) {
        var checksum = -3750763034362895579L
        var hits = 0
        for (stateId in 0 until Blocks.stateCount) {
            val value = of(stateId)
            checksum = (checksum xor value) * 1099511628211L
            if (value != SENTINEL) hits++
        }
        println("ORACLE $name=$checksum hits=$hits")
    }

    @Test
    fun dump() {
        fold("isLeaves") { if (Blocks.isLeavesState(it)) 1L else SENTINEL }
        fold("leafDistance") { Blocks.leafDistance(it)?.toLong() ?: SENTINEL }
        fold("withLeafDistance5") { if (Blocks.isLeavesState(it)) Blocks.withLeafDistance(it, 5)!!.toLong() else SENTINEL }
        fold("isDoublePlant") { if (Blocks.isDoublePlantState(it)) 1L else SENTINEL }
        fold("upperDoublePlant") { Blocks.upperDoublePlantStateId(it)?.toLong() ?: SENTINEL }
        fold("snowyTrue") { Blocks.withSnowy(it, true)?.toLong() ?: SENTINEL }
        fold("snowyFalse") { Blocks.withSnowy(it, false)?.toLong() ?: SENTINEL }
        fold("isKelp") { if (Blocks.isKelpState(it)) 1L else SENTINEL }
        fold("vineHead7") { Blocks.growingVineHeadStateId(it, 7)?.toLong() ?: SENTINEL }
        fold("isSeaPickle") { if (Blocks.isSeaPickleState(it)) 1L else SENTINEL }
        fold("isCoralPlant") { if (Blocks.isCoralPlantState(it)) 1L else SENTINEL }
        fold("coralWallFan") { Blocks.coralWallFanStateId(it, "west", true)?.toLong() ?: SENTINEL }
        fold("isHugeMushroom") { if (Blocks.isHugeMushroomState(it)) 1L else SENTINEL }
        fold("hugeMushroom") {
            Blocks.hugeMushroomStateId(it, down = true, east = false, north = true, south = false, up = true, west = false)
                ?.toLong() ?: SENTINEL
        }
        fold("isWildflowers") { if (Blocks.isWildflowersState(it)) 1L else SENTINEL }
        fold("isPinkPetals") { if (Blocks.isPinkPetalsState(it)) 1L else SENTINEL }
        fold("isGlowLichen") { if (Blocks.isGlowLichenState(it)) 1L else SENTINEL }
        fold("isVine") { if (Blocks.isVineState(it)) 1L else SENTINEL }
        fold("isCocoa") { if (Blocks.isCocoaState(it)) 1L else SENTINEL }
        fold("isPaleHangingMoss") { if (Blocks.isPaleHangingMossState(it)) 1L else SENTINEL }
        fold("isCreakingHeart") { if (Blocks.isCreakingHeartState(it)) 1L else SENTINEL }
        fold("isMangrovePropagule") { if (Blocks.isMangrovePropaguleState(it)) 1L else SENTINEL }
        fold("isLeafLitter") { if (Blocks.isLeafLitterState(it)) 1L else SENTINEL }
        fold("isBeeNest") { if (Blocks.isBeeNestState(it)) 1L else SENTINEL }

        val chorus = StringBuilder()
        for (mask in 0 until 64) {
            chorus.append(
                Blocks.chorusPlantStateId(
                    mask and 32 != 0, mask and 16 != 0, mask and 8 != 0,
                    mask and 4 != 0, mask and 2 != 0, mask and 1 != 0,
                ),
            ).append(',')
        }
        for (age in 0..5) chorus.append(Blocks.chorusFlowerStateId(age)).append(',')
        for (mask in 0 until 16) {
            chorus.append(
                Blocks.ironBarsStateId(mask and 8 != 0, mask and 4 != 0, mask and 2 != 0, mask and 1 != 0),
            ).append(',')
        }
        println("ORACLE fixed=$chorus")
    }

    private companion object {
        const val SENTINEL = Long.MIN_VALUE
    }
}
