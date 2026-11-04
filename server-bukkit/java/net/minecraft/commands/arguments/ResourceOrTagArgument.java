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
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public class ResourceOrTagArgument<T> implements ArgumentType<ResourceOrTagArgument.Result<T>> {
    private static final Collection<String> EXAMPLES = Arrays.asList("foo", "foo:bar", "012", "#skeletons", "#minecraft:skeletons");
    private static final DynamicCommandExceptionType ERROR_INVALID_TYPE = new DynamicCommandExceptionType(value -> new LiteralMessage("Invalid registry for " + value));
    private final ResourceKey<? extends Registry<T>> registryKey;

    public ResourceOrTagArgument(CommandBuildContext context, ResourceKey<? extends Registry<T>> registryKey) {
        this.registryKey = registryKey;
    }

    public static <T> ResourceOrTagArgument<T> resourceOrTag(CommandBuildContext context, ResourceKey<? extends Registry<T>> key) {
        return new ResourceOrTagArgument<>(context, key);
    }

    private Registry<T> registry() {
        return org.basaltmc.bukkit.facade.ServerAccess.registries().lookupOrThrow(registryKey);
    }

    @SuppressWarnings("unchecked")
    public static <T> Result<T> getResourceOrTag(CommandContext<CommandSourceStack> context, String name, ResourceKey<Registry<T>> registryKey) throws CommandSyntaxException {
        Result<?> result = context.getArgument(name, Result.class);
        Optional<Result<T>> cast = result.cast(registryKey);
        return cast.orElseThrow(() -> ERROR_INVALID_TYPE.create(result.asPrintable()));
    }

    public Result<T> parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        if (reader.canRead() && reader.peek() == '#') {
            reader.skip();
            Identifier id = Identifier.read(reader);
            TagKey<T> key = TagKey.create(registryKey, id);
            HolderSet.Named<T> tag = registry().get(key).orElseThrow(() -> {
                reader.setCursor(start);
                return ERROR_INVALID_TYPE.createWithContext(reader, "#" + id);
            });
            return new TagResult<>(tag);
        }
        Identifier id = Identifier.read(reader);
        Holder.Reference<T> value = registry().get(ResourceKey.create(registryKey, id)).orElseThrow(() -> {
            reader.setCursor(start);
            return ERROR_INVALID_TYPE.createWithContext(reader, id.toString());
        });
        return new ResourceResult<>(value);
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    static record ResourceResult<T>(Holder.Reference<T> value) implements Result<T> {
        public Either<Holder.Reference<T>, HolderSet.Named<T>> unwrap() {
            return Either.left(value);
        }

        @SuppressWarnings("unchecked")
        public <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey) {
            return value.key().isFor(registryKey) ? Optional.of((Result<E>) this) : Optional.empty();
        }

        public boolean test(Holder<T> holder) {
            return holder.equals(value);
        }

        public String asPrintable() {
            return value.key().identifier().toString();
        }
    }

    static record TagResult<T>(HolderSet.Named<T> tag) implements Result<T> {
        public Either<Holder.Reference<T>, HolderSet.Named<T>> unwrap() {
            return Either.right(tag);
        }

        @SuppressWarnings("unchecked")
        public <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey) {
            return tag.key().isFor(registryKey) ? Optional.of((Result<E>) this) : Optional.empty();
        }

        public boolean test(Holder<T> holder) {
            return tag.contains(holder);
        }

        public String asPrintable() {
            return "#" + tag.key().location();
        }
    }

    public static class Info<T> implements ArgumentTypeInfo<ResourceOrTagArgument<T>, Info<T>.Template> {
        public void serializeToNetwork(Info<T>.Template template, FriendlyByteBuf out) {
            out.writeResourceKey(template.registryKey);
        }

        public Info<T>.Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readRegistryKey());
        }

        public void serializeToJson(Info<T>.Template template, JsonObject out) {
            out.addProperty("registry", template.registryKey.identifier().toString());
        }

        public Info<T>.Template unpack(ResourceOrTagArgument<T> argument) {
            return new Template(argument.registryKey);
        }

        public final class Template implements ArgumentTypeInfo.Template<ResourceOrTagArgument<T>> {
            final ResourceKey<? extends Registry<T>> registryKey;

            private Template(ResourceKey<? extends Registry<T>> registryKey) {
                this.registryKey = registryKey;
            }

            public ResourceOrTagArgument<T> instantiate(CommandBuildContext context) {
                return new ResourceOrTagArgument<>(context, registryKey);
            }

            public ArgumentTypeInfo<ResourceOrTagArgument<T>, ?> type() {
                return Info.this;
            }
        }
    }

    public interface Result<T> extends Predicate<Holder<T>> {
        Either<Holder.Reference<T>, HolderSet.Named<T>> unwrap();

        <E> Optional<Result<E>> cast(ResourceKey<? extends Registry<E>> registryKey);

        String asPrintable();
    }
}
