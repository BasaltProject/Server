package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class RegistryTest {
    @Test
    fun registersAndLooksUpByIdAndIdentifier() {
        val registry = Registry<String>()
        val stone = Identifier.vanilla("stone")
        registry.register(1, stone, "stone-value")

        assertEquals("stone-value", registry.byId(1))
        assertEquals("stone-value", registry.byIdentifier(stone))
        assertEquals(1, registry.idOf(stone))
        assertEquals(stone, registry.identifierOf(1))
    }

    @Test
    fun idsDoNotNeedToBeContiguous() {
        val registry = Registry<String>()
        registry.register(0, Identifier.vanilla("air"), "air")
        registry.register(85, Identifier.vanilla("bedrock"), "bedrock")

        assertEquals(2, registry.size)
        assertEquals("air", registry.byId(0))
        assertEquals("bedrock", registry.byId(85))
        assertNull(registry.byId(1))
    }

    @Test
    fun rejectsADuplicateId() {
        val registry = Registry<String>()
        registry.register(1, Identifier.vanilla("a"), "a")
        assertFailsWith<IllegalArgumentException> { registry.register(1, Identifier.vanilla("b"), "b") }
    }

    @Test
    fun rejectsADuplicateIdentifier() {
        val registry = Registry<String>()
        registry.register(1, Identifier.vanilla("a"), "a")
        assertFailsWith<IllegalArgumentException> { registry.register(2, Identifier.vanilla("a"), "b") }
    }

    @Test
    fun valuesReturnsEveryRegisteredValue() {
        val registry = Registry<String>()
        registry.register(0, Identifier.vanilla("a"), "a")
        registry.register(1, Identifier.vanilla("b"), "b")
        assertEquals(listOf("a", "b"), registry.values().toList())
    }
}
