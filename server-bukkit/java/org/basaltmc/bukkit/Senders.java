package org.basaltmc.bukkit;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import net.kyori.adventure.text.Component;
import org.bukkit.Location;
import org.bukkit.command.BlockCommandSender;
import org.bukkit.command.CommandSender;
import org.bukkit.command.ProxiedCommandSender;

/** The command sender a plugin command sees for the kind of source that ran it. */
final class Senders {
    private Senders() {}

    private static CommandSender named(CommandSender base, Class<?> face, String name, java.util.Map<String, Object> extra) {
        return (CommandSender) Proxy.newProxyInstance(face.getClassLoader(),
            new Class<?>[] {face, CommandSender.class}, (proxy, method, args) -> {
                String member = method.getName();
                if (extra.containsKey(member) && (args == null || args.length == 0)) {
                    return extra.get(member);
                }
                if (member.equals("getName") && (args == null || args.length == 0)) {
                    return name;
                }
                if (member.equals("sendMessage") || member.equals("sendRichMessage") || member.equals("sendPlainMessage")) {
                    for (Object argument : args == null ? new Object[0] : args) {
                        if (argument instanceof Component component) {
                            WorldEnvNatives.commandReply(Text.mini(component));
                        } else if (argument instanceof String text) {
                            WorldEnvNatives.commandReply(member.equals("sendPlainMessage")
                                ? Text.mini(Component.text(text)) : Text.mini(text));
                        } else if (argument instanceof String[] lines) {
                            for (String line : lines) {
                                WorldEnvNatives.commandReply(Text.mini(line));
                            }
                        }
                    }
                    return null;
                }
                if (member.equals("equals")) {
                    return proxy == args[0];
                }
                if (member.equals("hashCode")) {
                    return System.identityHashCode(proxy);
                }
                if (member.equals("isOp") || member.equals("hasPermission") || member.equals("isPermissionSet")) {
                    return true;
                }
                if (method.isDefault() && !method.getDeclaringClass().isInstance(base)) {
                    return java.lang.reflect.InvocationHandler.invokeDefault(proxy, method, args);
                }
                try {
                    return method.invoke(base, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            });
    }

    static CommandSender of(BasaltServer server, String kind, String name, String world, double x, double y, double z,
        String caller) {
        switch (kind) {
            case "player": {
                CommandSender player = server.playerByName(name);
                return player;
            }
            case "command_block": {
                BasaltWorld at = world == null || world.isEmpty() ? null : server.world(world);
                java.util.Map<String, Object> extra = new java.util.HashMap<>();
                if (at != null) {
                    extra.put("getBlock", at.getBlockAt(new Location(at, x, y, z)));
                }
                return named(server.consoleImpl(), BlockCommandSender.class, name, extra);
            }
            case "proxied": {
                int split = caller == null ? -1 : caller.indexOf(':');
                String callerKind = split < 0 ? "console" : caller.substring(0, split);
                String callerName = split < 0 ? "" : caller.substring(split + 1);
                CommandSender asked = of(server, callerKind, callerName, world, x, y, z, "");
                CommandSender callee = server.playerByName(name);
                if (asked == null || callee == null) {
                    return callee == null ? server.consoleImpl() : callee;
                }
                java.util.Map<String, Object> extra = new java.util.HashMap<>();
                extra.put("getCaller", asked);
                extra.put("getCallee", callee);
                return named(callee, ProxiedCommandSender.class, callee.getName(), extra);
            }
            default:
                return server.consoleImpl();
        }
    }
}
