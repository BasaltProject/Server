package org.basaltmc.server.runtime.placeholder

import org.basaltmc.server.runtime.command.testGroups
import org.basaltmc.server.runtime.config.PingRequest
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderValue
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * The provider itself, not the pieces it is built from.
 *
 * `Decimals` had its own tests and passed them while the provider never called it: the suffix was
 * advertised in `names()` and did nothing. These assertions are about what a file asking for
 * `%basalt_player_x_1%` actually gets back, which is the thing that was broken.
 */
class PlayerPlaceholdersTest {
    private val provider = PlayerPlaceholders({ testGroups() })

    /** A ping carries no player, so anything about one is unanswerable but still *recognised*. */
    private val pingContext =
        PlaceholderContext.EMPTY.with(PlaceholderSlots.PING, PingRequest(776, "example.com", "127.0.0.1", null))

    private fun resolve(name: String) = provider.resolve(name, pingContext)

    @Test
    fun aPrecisionSuffixIsRecognisedOnANumericPlaceholder() {
        // Unavailable, not Unknown: "there is no player here" means the name was understood, and
        // that is the whole difference this test exists to catch.
        for (name in listOf("x_1", "y_2", "z_0", "health_3")) {
            assertTrue(resolve(name) is PlaceholderValue.Unavailable, "$name should be a known placeholder")
        }
    }

    @Test
    fun aPrecisionSuffixOnSomethingThatIsNotAPlaceholderIsStillUnknown() {
        assertTrue(resolve("nonsense_1") is PlaceholderValue.Unknown)
        assertTrue(resolve("nonsense") is PlaceholderValue.Unknown)
    }

    @Test
    fun anExistingNameEndingInAWordIsNotReadAsAPrecision() {
        // `health_rounded` is its own placeholder and must keep winning over the suffix rule.
        assertTrue(resolve("health_rounded") is PlaceholderValue.Unavailable)
    }

    @Test
    fun theSuffixIsAdvertisedSoItCanBeFound() {
        assertTrue(provider.names().any { "<decimals>" in it })
    }

    @Test
    fun whatAPingCanAnswerItStillAnswers() {
        // The suffix rule is tried last and must not have swallowed the ordinary names.
        assertTrue(resolve("protocol") is PlaceholderValue.Text)
        assertTrue(resolve("host") is PlaceholderValue.Text)
    }
}
