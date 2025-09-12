package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The catalogues name the templates by building a string, so a wrong bound picks a room that does
 * not exist and the mansion comes out with a hole where it should be. These are the jar's own
 * ranges.
 */
class MansionRoomsTest {
    private fun random() = WorldgenRandom(LegacyRandom(12345L))

    @Test
    fun `each shape draws from the range its floor has`() {
        val ground = MansionRoomCollection.BY_FLOOR[0]
        val upper = MansionRoomCollection.BY_FLOOR[1]
        val random = random()
        val ground1x1 = (1..400).map { ground.room1X1(random) }.toSet()
        assertEquals((1..5).map { "1x1_a$it" }.toSet(), ground1x1)
        val upper1x1 = (1..400).map { upper.room1X1(random) }.toSet()
        assertEquals((1..5).map { "1x1_b$it" }.toSet(), upper1x1)
        assertEquals((1..4).map { "1x1_as$it" }.toSet(), (1..400).map { ground.room1X1Secret(random) }.toSet())
        assertEquals(
            (1..9).map { "1x2_a$it" }.toSet(),
            (1..900).map { ground.room1X2SideEntrance(random, false) }.toSet(),
        )
        assertEquals(
            (1..5).map { "1x2_b$it" }.toSet(),
            (1..500).map { ground.room1X2FrontEntrance(random, false) }.toSet(),
        )
        assertEquals((1..2).map { "1x2_s$it" }.toSet(), (1..200).map { ground.room1X2Secret(random) }.toSet())
        assertEquals((1..4).map { "2x2_a$it" }.toSet(), (1..400).map { ground.room2X2(random) }.toSet())
        assertEquals(
            (1..4).map { "1x2_c$it" }.toSet(),
            (1..400).map { upper.room1X2SideEntrance(random, false) }.toSet(),
        )
        assertEquals(
            (1..5).map { "1x2_d$it" }.toSet(),
            (1..500).map { upper.room1X2FrontEntrance(random, false) }.toSet(),
        )
        assertEquals(setOf("1x2_se1"), (1..50).map { upper.room1X2Secret(random) }.toSet())
        assertEquals((1..5).map { "2x2_b$it" }.toSet(), (1..500).map { upper.room2X2(random) }.toSet())
        assertEquals("2x2_s1", ground.room2X2Secret())
    }

    @Test
    fun `the stairs room is a fixed template and only on the upper floors`() {
        val random = random()
        assertEquals("1x2_c_stairs", MansionRoomCollection.BY_FLOOR[1].room1X2SideEntrance(random, true))
        assertEquals("1x2_d_stairs", MansionRoomCollection.BY_FLOOR[1].room1X2FrontEntrance(random, true))
        // The ground floor has no stairs room, so the flag changes nothing there.
        assertTrue(MansionRoomCollection.BY_FLOOR[0].room1X2SideEntrance(random, true).startsWith("1x2_a"))
    }

    @Test
    fun `the tower reuses the upper floor's catalogue unchanged`() {
        assertTrue(MansionRoomCollection.BY_FLOOR[1] !== MansionRoomCollection.BY_FLOOR[2])
        val a = random()
        val b = random()
        repeat(
            20,
        ) { assertEquals(MansionRoomCollection.BY_FLOOR[1].room1X1(a), MansionRoomCollection.BY_FLOOR[2].room1X1(b)) }
    }

    @Test
    fun `a secret one by one takes a secret template`() {
        val filler = MansionRoomFiller(random())
        val ordinary = mutableListOf<MansionPiece>()
        filler.addRoom1X1(ordinary, StructurePos(0, 64, 0), StructureRotation.NONE, StructureDirection.EAST, 0)
        assertEquals(1, ordinary.size)
        assertTrue(ordinary[0].template.startsWith("1x1_a"), "an ordinary room took a secret template")

        val secret = mutableListOf<MansionPiece>()
        MansionRoomFiller(
            random(),
        ).addRoom1X1(secret, StructurePos(0, 64, 0), StructureRotation.NONE, StructureDirection.UP, 0)
        assertEquals(1, secret.size)
        assertTrue(secret[0].template.startsWith("1x1_as"), "a secret room took an ordinary template")
    }
}
