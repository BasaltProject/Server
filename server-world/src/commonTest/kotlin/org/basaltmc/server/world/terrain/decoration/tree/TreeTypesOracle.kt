package org.basaltmc.server.world.terrain.decoration.tree

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the tree feature-size bands. */
class TreeTypesOracle {
    @Test
    fun dump() {
        // The exact shapes the real configurations use, over the whole height range each can see.
        val twoLayers =
            listOf(
                Triple(1, 0, 1) to null,
                Triple(2, 0, 2) to null,
                Triple(1, 0, 2) to 4,
                Triple(3, 0, 2) to null,
            )
        val two = StringBuilder()
        for ((shape, clipped) in twoLayers) {
            val size = FeatureSizeTwoLayers(shape.first, shape.second, shape.third, clipped)
            for (treeHeight in listOf(4, 7, 12, 20)) {
                for (yo in 0..treeHeight) two.append(size.getSizeAtHeight(treeHeight, yo))
                two.append('|')
            }
            two.append("clip=").append(size.minClippedHeight).append(';')
        }
        println("ORACLE two=$two")

        val threeLayers =
            listOf(
                listOf(1, 1, 0, 1, 2),
                listOf(0, 0, 1, 2, 1),
                listOf(2, 3, 0, 2, 1),
            )
        val three = StringBuilder()
        for (shape in threeLayers) {
            val size = FeatureSizeThreeLayers(shape[0], shape[1], shape[2], shape[3], shape[4])
            for (treeHeight in listOf(4, 7, 12, 20)) {
                for (yo in 0..treeHeight) three.append(size.getSizeAtHeight(treeHeight, yo))
                three.append('|')
            }
            three.append("clip=").append(size.minClippedHeight).append(';')
        }
        println("ORACLE three=$three")
    }
}
