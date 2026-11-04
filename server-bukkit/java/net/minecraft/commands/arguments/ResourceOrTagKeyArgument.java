// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.google.gson.JsonObject;
import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.datafixers.util.Either;
import java.util.Arrays;
import java.util.Collection;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Predicate;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public class ResourceOrTagKeyArgument<T> implements ArgumentType<ResourceOrTagKeyArgument.Result<T>> {
    private static final Collection<String> EXAMPLES = Arrays.asList("foo", "foo:bar", "012", "#skeletons", "#minecraft:skeletons");
    private final ResourceKey<? extends Registry<T>> registryKey;

    public ResourceOrTagKeyArgument(ResourceKey<? extends Registry<T>> registryKey) {
        this.registryKey = registryKey;
    }

    public static <T> ResourceOrTagKeyArgument<T> resourceOrTagKey(ResourceKey<? extends Registry<T>> key) {
        return new ResourceOrTagKeyArgument<>(key);
    }

    @SuppressWarnings("unchecked")
    public static <T> Result<T> getResourceOrTagKey(CommandContext<CommandSourceStack> context, String name, ResourceKey<Registry<T>> registryKey, DynamicCommandExceptionType exceptionType) throws CommandSyntaxException {
        Result<?> result = context.getArgument(name, Result.class);
        Optional<Result<T>> cast = result.cast(registryKey);
        return cast.orElseThrow(() -> exceptionType.create(result));
    }

    public Result<T> parse(StringReader reader) throws CommandSyntaxException {
        if (reader.canRead() && reader.peek() == '#') {
            int start = reader.getCursor();
            reader.skip();
            try {
                return new TagResult<>(TagKey.create(registryKey, Identifier.read(reader)));
            } catch (CommandSyntaxException failure) {
                reader.setCursor(start);
                throw failure;
            }
        }
        return new ResourceResult<>(ResourceKey.create(registryKey, Identifier.read(reader)));
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    static record ResourceResult<T>(ResourceKey<T> key) implements Result<T> {
        public Either<ResourceKey<T>, TagKey<T>> unwrap() {
            return Either.left(key);
        }

        @SuppressWarnings("unchecked")
        public <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey) {
            return key.isFor(registryKey) ? Optional.of((Result<E>) this) : Optional.empty();
        }

        public boolean test(Holder<T> holder) {
            return holder.is(key);
        }

        public String asPrintable() {
            return key.identifier().toString();
        }
    }

    static record TagResult<T>(TagKey<T> key) implements Result<T> {
        public Either<ResourceKey<T>, TagKey<T>> unwrap() {
            return Either.right(key);
        }

        @SuppressWarnings("unchecked")
        public <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey) {
            return key.isFor(registryKey) ? Optional.of((Result<E>) this) : Optional.empty();
        }

        public boolean test(Holder<T> holder) {
            return holder.is(key);
        }

        public String asPrintable() {
            return "#" + key.location();
        }
    }

    public static class Info<T> implements ArgumentTypeInfo<ResourceOrTagKeyArgument<T>, Info<T>.Template> {
        public void serializeToNetwork(Info<T>.Template template, FriendlyByteBuf out) {
            out.writeResourceKey(template.registryKey);
        }

        public Info<T>.Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readRegistryKey());
        }

        public void serializeToJson(Info<T>.Template template, JsonObject out) {
            out.addProperty("registry", template.registryKey.identifier().toString());
        }

        public Info<T>.Template unpack(ResourceOrTagKeyArgument<T> argument) {
            return new Template(argument.registryKey);
        }

        public final class Template implements ArgumentTypeInfo.Template<ResourceOrTagKeyArgument<T>> {
            final ResourceKey<? extends Registry<T>> registryKey;

            private Template(ResourceKey<? extends Registry<T>> registryKey) {
                this.registryKey = registryKey;
            }

            public ResourceOrTagKeyArgument<T> instantiate(CommandBuildContext context) {
                return new ResourceOrTagKeyArgument<>(registryKey);
            }

            public ArgumentTypeInfo<ResourceOrTagKeyArgument<T>, ?> type() {
                return Info.this;
            }
        }
    }

    public interface Result<T> extends Predicate<Holder<T>> {
        Either<ResourceKey<T>, TagKey<T>> unwrap();

        <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey);

        String asPrintable();
    }
}
