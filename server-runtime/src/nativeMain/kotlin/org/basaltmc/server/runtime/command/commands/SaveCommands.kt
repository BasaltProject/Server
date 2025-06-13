package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/**
 * Whether the periodic sweep is allowed to write, and how to force one. Held by the server, since
 * both answers belong to the thing that owns the tick loop.
 */
public class SaveControl(
    private val saveNow: () -> Unit,
) {
    public var enabled: Boolean = true
        private set

    public fun setEnabled(value: Boolean): Boolean {
        if (enabled == value) return false
        enabled = value
        return true
    }

    public fun save() {
        saveNow()
    }
}

/** parity: `SaveAllCommand`. `flush` means the same thing it does in vanilla: write and wait. */
public class SaveAllCommand(
    private val control: SaveControl,
) : Command(name = "save-all", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> saveAll(context) }
        root.literal("flush") { executes { context -> saveAll(context) } }
    }

    private fun saveAll(context: org.basaltmc.server.runtime.command.CommandContext): Int {
        context.success(Component.translatable("commands.save.saving"))
        control.save()
        context.success(Component.translatable("commands.save.success"), broadcastToAdmins = true)
        return 1
    }
}

/** parity: `SaveOffCommand`, which refuses when saving is already off rather than saying nothing. */
public class SaveOffCommand(
    private val control: SaveControl,
) : Command(name = "save-off", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            if (!control.setEnabled(false)) {
                throw CommandSyntaxException(Component.translatable("commands.save.alreadyOff"))
            }
            context.success(Component.translatable("commands.save.disabled"), broadcastToAdmins = true)
            1
        }
    }
}

/** parity: `SaveOnCommand`. */
public class SaveOnCommand(
    private val control: SaveControl,
) : Command(name = "save-on", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            if (!control.setEnabled(true)) {
                throw CommandSyntaxException(Component.translatable("commands.save.alreadyOn"))
            }
            context.success(Component.translatable("commands.save.enabled"), broadcastToAdmins = true)
            1
        }
    }
}
