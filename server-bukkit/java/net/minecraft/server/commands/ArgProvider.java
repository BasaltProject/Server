// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.builder.ArgumentBuilder;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;
import net.minecraft.commands.CommandSourceStack;
public interface ArgProvider<T> {
    T access(CommandContext<CommandSourceStack> context) throws CommandSyntaxException;
    ArgumentBuilder<CommandSourceStack, ?> wrap(ArgumentBuilder<CommandSourceStack, ?> parent, Function<ArgumentBuilder<CommandSourceStack, ?>, ArgumentBuilder<CommandSourceStack, ?>> function);
    static <T> ArgProvider<T> create(String key, Supplier<ArgumentBuilder<CommandSourceStack, ?>> child, InCommandFunction<CommandContext<CommandSourceStack>, T> access) {
        return new ArgProvider<T>() {
            public T access(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
                return access.apply(context);
            }

            public ArgumentBuilder<CommandSourceStack, ?> wrap(ArgumentBuilder<CommandSourceStack, ?> parent,
                Function<ArgumentBuilder<CommandSourceStack, ?>, ArgumentBuilder<CommandSourceStack, ?>> function) {
                return parent.then(net.minecraft.commands.Commands.literal(key).then(function.apply(child.get())));
            }
        };
    }
    static <T> List<ArgProvider<T>> buildList(String argName, List<Factory<T>> factories) {
        return factories.stream().map(factory -> factory.create(argName)).toList();
    }
    public interface Factory<T> {
        ArgProvider<T> create(String arg);
    }
}
