// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.ArgumentType;
import java.util.function.Function;
import java.util.function.Supplier;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.network.FriendlyByteBuf;

public class SingletonArgumentInfo<A extends ArgumentType<?>> implements ArgumentTypeInfo<A, SingletonArgumentInfo<A>.Template> {
    private final Template template;

    private SingletonArgumentInfo(Function<CommandBuildContext, A> constructor) {
        this.template = new Template(constructor);
    }

    public static <T extends ArgumentType<?>> SingletonArgumentInfo<T> contextFree(Supplier<T> constructor) {
        return new SingletonArgumentInfo<>(context -> constructor.get());
    }

    public static <T extends ArgumentType<?>> SingletonArgumentInfo<T> contextAware(Function<CommandBuildContext, T> constructor) {
        return new SingletonArgumentInfo<>(constructor);
    }

    public void serializeToNetwork(SingletonArgumentInfo<A>.Template template, FriendlyByteBuf out) {
    }

    public void serializeToJson(SingletonArgumentInfo<A>.Template template, JsonObject out) {
    }

    public SingletonArgumentInfo<A>.Template deserializeFromNetwork(FriendlyByteBuf in) {
        return template;
    }

    public SingletonArgumentInfo<A>.Template unpack(A argument) {
        return template;
    }

    public final class Template implements ArgumentTypeInfo.Template<A> {
        private final Function<CommandBuildContext, A> constructor;

        public Template(Function<CommandBuildContext, A> constructor) {
            this.constructor = constructor;
        }

        public A instantiate(CommandBuildContext context) {
            return constructor.apply(context);
        }

        public ArgumentTypeInfo<A, ?> type() {
            return SingletonArgumentInfo.this;
        }
    }
}
