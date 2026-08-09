package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.io.FileIO
import org.basaltmc.server.placeholder.AnimationPlaceholders
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class AnimationsConfigTest {
    private fun load(
        name: String,
        contents: String = ANIMATIONS_DEFAULT_TOML,
    ): AnimationsConfig {
        val directory = ConfigDirectory("build/test-data/config_anim_$name", Logger("Test"))
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(AnimationsConfig.FILE_NAME), contents)
        return AnimationsConfig.load(directory, Logger("Test"))
    }

    @Test
    fun theShippedFileAnimatesTheWebsite() {
        val config = load("shipped")
        val website = kotlin.test.assertNotNull(config["website"])

        assertTrue(website.frames.size > 1)
        assertTrue(website.frames.all { "basaltmc.org" in it.replace(Regex("<[^>]*>"), "") })
    }

    @Test
    fun theFrameComesFromTheClockSoEveryoneSeesTheSameOne() {
        val config = load("clock", "[animations.a]\nchange-interval-ms = 100\nframes = [\"one\", \"two\"]\n")
        val animation = kotlin.test.assertNotNull(config["a"])

        // Same instant, same frame: no per-player counter to drift.
        assertEquals("one", animation.frameAt(0))
        assertEquals("two", animation.frameAt(100))
        assertEquals("one", animation.frameAt(200))
        assertEquals(animation.frameAt(12_345), animation.frameAt(12_345))
    }

    @Test
    fun anIntervalBelowTheFloorIsRaisedToIt() {
        val config = load("floor", "[animations.a]\nchange-interval-ms = 1\nframes = [\"x\", \"y\"]\n")

        // Nothing redraws every millisecond; allowing it would only burn packets.
        assertTrue(kotlin.test.assertNotNull(config["a"]).changeIntervalMillis >= 50)
    }

    @Test
    fun anAnimationWithoutFramesIsSkippedRatherThanRegistered() {
        assertTrue(load("empty", "[animations.a]\nchange-interval-ms = 100\n").names.isEmpty())
    }

    @Test
    fun aFrameCanWrapLiveDataButNotAnotherAnimation() {
        val config =
            load(
                "nested",
                """
                [animations.live]
                change-interval-ms = 100
                frames = ["<%basalt_server_online% online>"]

                [animations.loop]
                change-interval-ms = 100
                frames = ["%basalt_animation_loop% forever"]
                """.trimIndent(),
            )
        val registry = PlaceholderRegistry()
        registry.register("basalt_server") { params, _ ->
            if (params == "online") PlaceholderValue.of(4) else PlaceholderValue.Unknown
        }
        val provider = AnimationPlaceholders({ config }) { registry }
        registry.register(AnimationPlaceholders.IDENTIFIER, provider)

        assertEquals("<4 online>", provider.resolve("live", PlaceholderContext.EMPTY).textOrNull)
        // An animation naming an animation is blanked before substituting, so this returns rather
        // than recursing until the stack runs out.
        assertEquals(" forever", provider.resolve("loop", PlaceholderContext.EMPTY).textOrNull)
    }

    @Test
    fun anAnimationNobodyDefinedSaysSoRatherThanBeingUnknown() {
        val provider = AnimationPlaceholders({ load("missing") }) { PlaceholderRegistry() }
        val value = provider.resolve("nope", PlaceholderContext.EMPTY)

        assertTrue(value is PlaceholderValue.Unavailable && "no animation named" in (value.reason ?: ""))
    }
}
