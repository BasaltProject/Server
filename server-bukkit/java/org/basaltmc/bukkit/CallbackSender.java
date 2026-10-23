package org.basaltmc.bukkit;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import java.util.function.Consumer;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;
import org.bukkit.command.CommandSender;

/** A command sender that hands what it is told to a function and otherwise acts as the console. */
final class CallbackSender {
    private CallbackSender() {}

    static CommandSender of(CommandSender console, Consumer<? super Component> feedback) {
        return (CommandSender) Proxy.newProxyInstance(CommandSender.class.getClassLoader(),
            new Class<?>[] {CommandSender.class}, (proxy, method, args) -> {
                String name = method.getName();
                if (name.equals("sendMessage") || name.equals("sendRichMessage") || name.equals("sendPlainMessage")) {
                    for (Object argument : args == null ? new Object[0] : args) {
                        if (argument instanceof Component component) {
                            feedback.accept(component);
                        } else if (argument instanceof String text) {
                            feedback.accept(name.equals("sendPlainMessage") ? Component.text(text)
                                : LegacyComponentSerializer.legacySection().deserialize(text));
                        } else if (argument instanceof String[] lines) {
                            for (String line : lines) {
                                feedback.accept(LegacyComponentSerializer.legacySection().deserialize(line));
                            }
                        } else if (argument instanceof net.kyori.adventure.text.ComponentLike like) {
                            feedback.accept(like.asComponent());
                        }
                    }
                    return null;
                }
                if (name.equals("equals")) {
                    return proxy == args[0];
                }
                if (name.equals("hashCode")) {
                    return System.identityHashCode(proxy);
                }
                try {
                    return method.invoke(console, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            });
    }
}
