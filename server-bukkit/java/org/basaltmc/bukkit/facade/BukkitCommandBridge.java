package org.basaltmc.bukkit.facade;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.tree.CommandNode;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.Permission;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;

/** Exposes Brigadier root literals as server commands and builds command sources from senders. */
public final class BukkitCommandBridge {
    private BukkitCommandBridge() {}

    /** A command source backed by a server command sender. */
    public record SenderSource(CommandSender sender) implements CommandSource {
        @Override
        public void sendSystemMessage(Component message) {
            sender.sendMessage(message.asAdventure());
        }
    }

    public static CommandSource sourceOf(CommandSender sender) {
        return new SenderSource(sender);
    }

    public static CommandSender senderOf(CommandSourceStack stack) {
        return stack.getCommandSource() instanceof SenderSource source ? source.sender()
            : org.bukkit.Bukkit.getServer().getConsoleSender();
    }

    public static PermissionSet permissionsOf(CommandSender sender) {
        return permission -> {
            if (permission instanceof Permission.HasCommandLevel required) {
                PermissionLevel level = sender.isOp() ? PermissionLevel.OWNERS : PermissionLevel.ALL;
                return LevelBasedPermissionSet.forLevel(level).level().isEqualOrHigherThan(required.level());
            }
            if (permission instanceof Permission.Atom atom) {
                return sender.hasPermission(atom.id().toString()) || sender.isOp() || LevelBasedPermissionSet.ALL.hasPermission(permission);
            }
            return false;
        };
    }

    public static CommandSourceStack stackOf(CommandSender sender, MinecraftServer server) {
        Entity entity = null;
        Vec3 position = Vec3.ZERO;
        ServerLevel level = null;
        Vec2 rotation = Vec2.ZERO;
        if (sender instanceof org.bukkit.entity.Player player) {
            if (player instanceof org.bukkit.craftbukkit.entity.CraftPlayer craft) {
                entity = craft.getHandle();
            }
            org.bukkit.Location where = player.getLocation();
            position = new Vec3(where.getX(), where.getY(), where.getZ());
            rotation = new Vec2(where.getPitch(), where.getYaw());
            if (where.getWorld() instanceof org.bukkit.craftbukkit.CraftWorld world) {
                level = world.getHandle();
            }
        }
        return new CommandSourceStack(sourceOf(sender), position, rotation, level, permissionsOf(sender),
            Component.literal(sender.getName()), server).withEntity(entity);
    }

    public static void expose(CommandDispatcher<CommandSourceStack> dispatcher, CommandNode<CommandSourceStack> node,
        MinecraftServer server) {
        org.bukkit.command.CommandMap map = org.bukkit.Bukkit.getServer().getCommandMap();
        String name = node.getName();
        map.register("minecraft", new BrigadierBukkitCommand(name, dispatcher, server));
    }
    /** Brigadier root literal published as a server command. */
    public static final class BrigadierBukkitCommand extends Command {
        private final String name;
        private final CommandDispatcher<CommandSourceStack> dispatcher;
        private final MinecraftServer server;

        BrigadierBukkitCommand(String name, CommandDispatcher<CommandSourceStack> dispatcher, MinecraftServer server) {
            super(name);
            this.name = name;
            this.dispatcher = dispatcher;
            this.server = server;
        }

        @Override
        public boolean execute(CommandSender sender, String label, String[] args) {
            String input = args.length == 0 ? name : name + " " + String.join(" ", args);
            CommandSourceStack stack = stackOf(sender, server).withDispatcher(dispatcher);
            try {
                dispatcher.execute(input, stack);
            } catch (CommandSyntaxException e) {
                stack.sendFailure(Component.literal(e.getMessage()));
            }
            return true;
        }

        @Override
        public List<String> tabComplete(CommandSender sender, String alias, String[] args) {
            String input = args.length == 0 ? name + " " : name + " " + String.join(" ", args);
            CommandSourceStack stack = stackOf(sender, server).withDispatcher(dispatcher);
            ParseResults<CommandSourceStack> parse = dispatcher.parse(input, stack);
            List<String> out = new ArrayList<>();
            try {
                dispatcher.getCompletionSuggestions(parse).get().getList().forEach(s -> out.add(s.getText()));
            } catch (Exception e) {
                return List.of();
            }
            return out;
        }
    }
}
