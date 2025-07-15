package org.basaltmc.server.plugins

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class PluginCommandSpecTest {
    @Test
    fun `a bare name is a command`() {
        val spec = PluginCommandSpec.parse("heal")
        assertEquals(listOf("heal"), spec.literals)
        assertEquals(emptyList(), spec.arguments)
    }

    @Test
    fun `words before the first argument are the path`() {
        val spec = PluginCommandSpec.parse("arena create <name: word>")
        assertEquals(listOf("arena", "create"), spec.literals)
        assertEquals("name", spec.arguments.single().name)
        assertEquals(PluginArgumentType.WORD, spec.arguments.single().type)
        assertTrue(!spec.arguments.single().optional)
    }

    @Test
    fun `square brackets are optional and carry a range`() {
        val spec = PluginCommandSpec.parse("arena create <name: word> [size: int 8..64]")
        val size = spec.arguments.last()
        assertTrue(size.optional)
        assertEquals(8L, size.min)
        assertEquals(64L, size.max)
    }

    @Test
    fun `the usage reads back the way it was written`() {
        val written = "arena create <name: word> [size: int 8..64]"
        assertEquals(written, PluginCommandSpec.parse(written).usage)
    }

    @Test
    fun `a required argument after an optional one is refused`() {
        val e = assertFailsWith<ManifestException> { PluginCommandSpec.parse("give [count: int] <item: word>") }
        assertTrue(e.message!!.contains("item"), e.message!!)
    }

    @Test
    fun `a word after an argument is refused`() {
        assertFailsWith<ManifestException> { PluginCommandSpec.parse("arena <name: word> delete") }
    }

    @Test
    fun `an argument type that does not exist yet says so`() {
        val e = assertFailsWith<ManifestException> { PluginCommandSpec.parse("tp <who: player>") }
        assertTrue(e.message!!.contains("does not have yet"), e.message!!)
    }

    @Test
    fun `an argument with no type is a word`() {
        assertEquals(
            PluginArgumentType.WORD,
            PluginCommandSpec
                .parse("say <what>")
                .arguments
                .single()
                .type,
        )
    }
}
