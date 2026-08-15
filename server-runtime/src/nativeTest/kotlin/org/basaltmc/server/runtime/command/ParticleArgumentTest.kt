package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.ParticleOptions
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The options block behind a particle id, in the same shapes vanilla's own codecs read. */
class ParticleArgumentTest {
    private fun parse(input: String): ParsedParticle =
        ParticleArgument.parse(StringReader(input), ConsoleCommandSource(), testEnvironment())

    @Test
    fun aParticleWithNoOptionsCarriesNone() {
        val parsed = parse("minecraft:flame")
        assertEquals(Identifier.vanilla("flame"), parsed.type.id)
        assertNull(parsed.options)
    }

    @Test
    fun blockTakesTheBlockStateItDraws() {
        val parsed = parse("""minecraft:block{block_state:"minecraft:stone"}""")
        val expected = checkNotNull(Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))).stateId
        assertEquals(ParticleOptions.BlockState(expected), parsed.options)
    }

    @Test
    fun dustTakesAColourAndAScale() {
        val parsed = parse("minecraft:dust{color:16711680,scale:2.0}")
        assertEquals(ParticleOptions.Dust(16711680, 2f), parsed.options)
    }

    @Test
    fun aColourMayAlsoBeWrittenAsChannels() {
        // parity: `ExtraCodecs.RGB_COLOR_CODEC`'s alternative form, floats between zero and one.
        val parsed = parse("minecraft:dust{color:[1.0f,0.0f,0.0f],scale:1.0}")
        val dust = assertIs<ParticleOptions.Dust>(parsed.options)
        assertEquals(0xFF, (dust.color shr 16) and 0xFF)
        assertEquals(0, dust.color and 0xFFFF)
    }

    @Test
    fun itemTakesTheStackItDraws() {
        val parsed = parse("""minecraft:item{item:{id:"minecraft:apple",count:1}}""")
        val expected = checkNotNull(Items.REGISTRY.byIdentifier(Identifier.vanilla("apple"))).protocolId
        assertEquals(ParticleOptions.Item(expected, 1), parsed.options)
    }

    @Test
    fun spellDefaultsBothOfItsFields() {
        assertEquals(ParticleOptions.Spell(-1, 1f), parse("minecraft:effect").options)
    }

    @Test
    fun shriekTakesItsDelayAndTrailItsTarget() {
        assertEquals(ParticleOptions.Delay(5), parse("minecraft:shriek{delay:5}").options)
        val trail =
            assertIs<ParticleOptions.Trail>(
                parse("minecraft:trail{target:[1.0,2.0,3.0],color:255,duration:20}").options,
            )
        assertEquals(3.0, trail.targetZ)
        assertEquals(20, trail.duration)
    }

    @Test
    fun anUnknownParticleIsRefusedByName() {
        val failure = assertFailsWith<CommandSyntaxException> { parse("minecraft:not_a_particle") }
        assertTrue("particle.notFound" in failure.component.toString())
    }

    @Test
    fun aMissingRequiredFieldIsRefusedRatherThanGuessed() {
        val failure = assertFailsWith<CommandSyntaxException> { parse("minecraft:block{}") }
        assertTrue("particle.invalidOptions" in failure.component.toString())
    }
}
