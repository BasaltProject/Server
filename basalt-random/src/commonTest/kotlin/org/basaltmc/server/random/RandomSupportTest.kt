package org.basaltmc.server.random

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`net.minecraft.util.Mth.getSeed` and `net.minecraft.world.level.levelgen.RandomSupport`)
 * through a small JVM harness, not by re-deriving the algorithms from memory.
 */
class RandomSupportTest {
    @Test
    fun getSeedMatchesVanilla() {
        assertEquals(0L, PositionalHashing.getSeed(0, 0, 0))
        assertEquals(-33674130277896L, PositionalHashing.getSeed(1, 2, 3))
        assertEquals(-131967588646891L, PositionalHashing.getSeed(-5, 64, 100))
        assertEquals(-49831574764955L, PositionalHashing.getSeed(123, -45, 678))
        assertEquals(52603072207110L, PositionalHashing.getSeed(1000000, 0, -1000000))
    }

    @Test
    fun mixStafford13MatchesVanilla() {
        assertEquals(0L, RandomSupport.mixStafford13(0L))
        assertEquals(6238072747940578789L, RandomSupport.mixStafford13(1L))
        assertEquals(-6387817139659442654L, RandomSupport.mixStafford13(42L))
        assertEquals(-5417735806833148549L, RandomSupport.mixStafford13(-1L))
        assertEquals(-1000775671988880032L, RandomSupport.mixStafford13(123456789L))
    }

    @Test
    fun upgradeSeedTo128bitMatchesVanilla() {
        assertEquals(Seed128bit(3847398142028685078L, 7192185014346937746L), RandomSupport.upgradeSeedTo128bit(0L))
        assertEquals(Seed128bit(5272463233947570727L, 1927618558350093866L), RandomSupport.upgradeSeedTo128bit(1L))
        assertEquals(Seed128bit(6720814022939733433L, -2851323883594622011L), RandomSupport.upgradeSeedTo128bit(42L))
        assertEquals(Seed128bit(-110783831392733308L, 2932223646667407290L), RandomSupport.upgradeSeedTo128bit(-1L))
        assertEquals(
            Seed128bit(-7278655050202467086L, -3669656285675084719L),
            RandomSupport.upgradeSeedTo128bit(123456789L),
        )
    }

    @Test
    fun seedFromHashOfMatchesVanilla() {
        assertEquals(Seed128bit(-3162216497309240828L, -1621285313438006658L), RandomSupport.seedFromHashOf(""))
        assertEquals(
            Seed128bit(4754257565590824948L, -1113069127075346987L),
            RandomSupport.seedFromHashOf("minecraft:overworld"),
        )
        assertEquals(Seed128bit(6719722671305337462L, -5084109257958767214L), RandomSupport.seedFromHashOf("hello"))
    }
}
