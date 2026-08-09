package org.basaltmc.server.command

import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderText
import org.basaltmc.server.placeholder.PlaceholderSlots
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.MiniMessage
import org.basaltmc.text.resolvePlainText

/**
 * Replaces what a command answers, by the translation key it would have sent.
 *
 * Keyed by translation key rather than by command, because that is what a message *is* here: no
 * command holds its text, it sends `commands.gamemode.success.self` and the client looks it up.
 * Which also explains the cost, and why the file says so plainly: an overridden message stops being
 * a key and becomes literal text, so every client sees the operator's language instead of its own.
 *
 * Applied at the one place command feedback leaves the server ([CommandFeedback]), not inside each
 * command, so no command has to know this exists and none can forget to.
 */
public class MessageOverrides(
    private val messages: Map<String, String>,
    private val placeholders: PlaceholderRegistry,
    private val lang: LangTable,
) {
    public val isEmpty: Boolean get() = messages.isEmpty()

    /**
     * [message] with every overridden translatable swapped out, or [message] itself when nothing
     * matches, which is the normal case and costs one map lookup per node.
     */
    public fun apply(
        message: Component,
        source: CommandSource?,
    ): Component {
        if (messages.isEmpty()) return message
        return transform(message, source)
    }

    private fun transform(
        component: Component,
        source: CommandSource?,
    ): Component {
        if (component is Component.Translatable) {
            messages[component.key]?.let { text ->
                val rendered = render(text, component.args.map { transform(it, source) }, source)
                return if (component.children.isEmpty()) {
                    rendered
                } else {
                    rendered.withChildren(component.children.map { transform(it, source) })
                }
            }
            // Not overridden itself, but its arguments can be: `chat.type.admin` carries the whole
            // command message as an argument, so a transform that only walked children would leave
            // exactly the copy every other operator sees untouched.
            val withArgs = component.copy(args = component.args.map { transform(it, source) })
            return withArgs.withChildren(withArgs.children.map { transform(it, source) })
        }
        if (component.children.isEmpty()) return component
        return component.withChildren(component.children.map { transform(it, source) })
    }

    /**
     * The operator's text: arguments first, then placeholders, then MiniMessage.
     *
     * Arguments come in as components and go in as plain text. Keeping their own colours would mean
     * splicing components into a MiniMessage parse, and the result would fight whatever styling the
     * operator wrote around them; a name is a name.
     */
    private fun render(
        text: String,
        args: List<Component>,
        source: CommandSource?,
    ): Component {
        var result = text
        for ((index, argument) in args.withIndex()) {
            val token = "%${index + 1}%"
            if (token in result) result = result.replace(token, argument.resolvePlainText(lang))
        }
        val context = PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, source?.player)
        return MiniMessage.parse(PlaceholderText.substitute(result, placeholders, context))
    }
}
