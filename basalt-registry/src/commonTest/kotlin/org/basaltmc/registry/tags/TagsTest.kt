package org.basaltmc.registry.tags

import org.basaltmc.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class TagsTest {
    @Test
    fun parsesPlainStringValuesAsRequiredElements() {
        val file = TagFileJson.parse("""{"values": ["minecraft:oak_log", "minecraft:spruce_log"]}""")
        assertEquals(false, file.replace)
        assertEquals(
            listOf(
                TagEntry(Identifier.vanilla("oak_log"), isTag = false, required = true),
                TagEntry(Identifier.vanilla("spruce_log"), isTag = false, required = true),
            ),
            file.entries,
        )
    }

    @Test
    fun parsesHashPrefixedValuesAsTagReferences() {
        val file = TagFileJson.parse("""{"values": ["#minecraft:logs"]}""")
        assertEquals(TagEntry(Identifier.vanilla("logs"), isTag = true, required = true), file.entries[0])
    }

    @Test
    fun parsesObjectFormWithExplicitRequiredFalse() {
        val file = TagFileJson.parse("""{"values": [{"id": "minecraft:unobtainable", "required": false}]}""")
        assertEquals(TagEntry(Identifier.vanilla("unobtainable"), isTag = false, required = false), file.entries[0])
    }

    @Test
    fun parsesReplaceFlag() {
        val file = TagFileJson.parse("""{"replace": true, "values": []}""")
        assertTrue(file.replace)
    }

    @Test
    fun resolvesATagReferencingAnotherTag() {
        val logs = Identifier.vanilla("logs")
        val planks = Identifier.vanilla("planks")
        val files =
            mapOf(
                logs to TagFile(entries = listOf(TagEntry(Identifier.vanilla("oak_log"), isTag = false))),
                planks to
                    TagFile(
                        entries =
                            listOf(
                                TagEntry(Identifier.vanilla("oak_planks"), isTag = false),
                                TagEntry(logs, isTag = true),
                            ),
                    ),
            )

        val resolved = TagSet.resolve(files) { true }

        assertEquals(setOf(Identifier.vanilla("oak_log")), resolved.getValue(logs))
        assertEquals(setOf(Identifier.vanilla("oak_planks"), Identifier.vanilla("oak_log")), resolved.getValue(planks))
    }

    @Test
    fun requiredMissingElementThrows() {
        val tag = Identifier.vanilla("logs")
        val files = mapOf(tag to TagFile(entries = listOf(TagEntry(Identifier.vanilla("ghost_log"), isTag = false))))

        assertFailsWith<TagResolutionException> { TagSet.resolve(files) { false } }
    }

    @Test
    fun optionalMissingElementIsDroppedSilently() {
        val tag = Identifier.vanilla("logs")
        val files =
            mapOf(
                tag to
                    TagFile(
                        entries =
                            listOf(
                                TagEntry(Identifier.vanilla("oak_log"), isTag = false),
                                TagEntry(Identifier.vanilla("ghost_log"), isTag = false, required = false),
                            ),
                    ),
            )

        val resolved = TagSet.resolve(files) { it == Identifier.vanilla("oak_log") }

        assertEquals(setOf(Identifier.vanilla("oak_log")), resolved.getValue(tag))
    }

    @Test
    fun cycleBetweenTagsThrows() {
        val a = Identifier.vanilla("a")
        val b = Identifier.vanilla("b")
        val files =
            mapOf(
                a to TagFile(entries = listOf(TagEntry(b, isTag = true))),
                b to TagFile(entries = listOf(TagEntry(a, isTag = true))),
            )

        assertFailsWith<TagResolutionException> { TagSet.resolve(files) { true } }
    }
}
