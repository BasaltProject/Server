package org.basaltmc.server.world.structure

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the spawn overrides and the mineshaft block set. */
class StructureTablesOracle {
    @Test
    fun dumpSpawnOverrides() {
        val rows = StringBuilder()
        val categories =
            listOf(
                "monster",
                "creature",
                "ambient",
                "axolotls",
                "underground_water_creature",
                "water_creature",
                "water_ambient",
                "misc",
            )
        for (structure in StructureSpawnOverrides.STRUCTURES.sorted()) {
            for (category in categories) {
                val override = StructureSpawnOverrides.of(structure, category) ?: continue
                rows
                    .append(structure)
                    .append('.')
                    .append(category)
                    .append('=')
                rows.append(override.bounds.name).append(':')
                rows.append(
                    override.spawns.joinToString(",") { "${it.type}/${it.weight}/${it.minCount}/${it.maxCount}" },
                )
                rows.append(';')
            }
        }
        println(
            "ORACLE overrides structures=${StructureSpawnOverrides.STRUCTURES.sorted().joinToString(",")} rows=$rows",
        )
        println(
            "ORACLE overrideMisses unknownStructure=${StructureSpawnOverrides.of("igloo", "monster") == null} " +
                "unknownCategory=${StructureSpawnOverrides.of("fortress", "misc") == null}",
        )
    }

    @Test
    fun dumpMineshaftBlocks() {
        for (type in MineshaftType.entries) {
            val blocks = MineshaftBlocks(type)
            println(
                "ORACLE mineshaft_${type.name} log=${blocks.log} planks=${blocks.planks} fence=${blocks.fence} " +
                    "west=${blocks.fenceWest} east=${blocks.fenceEast}",
            )
        }
    }
}
