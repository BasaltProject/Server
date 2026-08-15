package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.wire.text.Component

/**
 * parity: `TagCommand`.
 *
 * Tags live on the live session, so they are lost on disconnect. Vanilla persists them with the
 * entity; here that needs a column in `players.db`, which is a schema change and a separate piece
 * of work: stated rather than quietly half-done.
 */
public class TagCommand : Command(name = "tag", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", EntitiesArgument(single = false)) {
            literal("add") {
                argument("name", WordArgument) { executes { context -> add(context) } }
            }
            literal("remove") {
                argument("name", WordArgument) {
                    // parity: only the tags the targets actually carry, which is what vanilla lists.
                    suggestsFromServer { context ->
                        context.matching(
                            context
                                .argument<List<CommandTarget>>("targets")
                                .orEmpty()
                                .flatMap { it.tags }
                                .distinct(),
                        )
                    }
                    executes { context -> remove(context) }
                }
            }
            literal("list") { executes { context -> list(context) } }
        }
    }

    private fun add(context: CommandContext): Int {
        val targets = context.get<List<CommandTarget>>("targets")
        val tag = context.get<String>("name")
        val changed = targets.count { tagsOf(it).add(tag) }
        if (changed == 0) {
            throw CommandSyntaxException(
                Component.translatable(
                    if (targets.size == 1) "commands.tag.add.failed.single" else "commands.tag.add.failed.multiple",
                ),
            )
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.tag.add.success.single",
                    Component.text(tag),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.tag.add.success.multiple",
                    Component.text(tag),
                    Component.text(changed.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return changed
    }

    private fun remove(context: CommandContext): Int {
        val targets = context.get<List<CommandTarget>>("targets")
        val tag = context.get<String>("name")
        val changed = targets.count { tagsOf(it).remove(tag) }
        if (changed == 0) {
            throw CommandSyntaxException(
                Component.translatable(
                    if (targets.size == 1) {
                        "commands.tag.remove.failed.single"
                    } else {
                        "commands.tag.remove.failed.multiple"
                    },
                ),
            )
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.tag.remove.success.single",
                    Component.text(tag),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.tag.remove.success.multiple",
                    Component.text(tag),
                    Component.text(changed.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return changed
    }

    /** Tags live on the player session or on the entity, whichever this target is. */
    private fun tagsOf(target: CommandTarget): MutableSet<String> =
        when (target) {
            is CommandTarget.Player -> target.session.tags
            is CommandTarget.Entity -> target.entity.tags
        }

    private fun list(context: CommandContext): Int {
        val targets = context.get<List<CommandTarget>>("targets")
        val tags = targets.flatMap { tagsOf(it) }.distinct().sorted()
        context.success(
            when {
                tags.isEmpty() && targets.size == 1 ->
                    Component.translatable("commands.tag.list.single.empty", Component.text(targets.single().name))
                tags.isEmpty() ->
                    Component.translatable("commands.tag.list.multiple.empty", Component.text(targets.size.toString()))
                targets.size == 1 ->
                    Component.translatable(
                        "commands.tag.list.single.success",
                        Component.text(targets.single().name),
                        Component.text(tags.size.toString()),
                        Component.text(tags.joinToString(", ")),
                    )
                else ->
                    Component.translatable(
                        "commands.tag.list.multiple.success",
                        Component.text(targets.size.toString()),
                        Component.text(tags.size.toString()),
                        Component.text(tags.joinToString(", ")),
                    )
            },
        )
        return tags.size
    }
}
