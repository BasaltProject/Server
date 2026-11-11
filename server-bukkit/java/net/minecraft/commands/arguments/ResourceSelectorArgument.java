// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import com.google.gson.JsonObject;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.ResourceKey;
public class ResourceSelectorArgument<T> implements ArgumentType<Collection<Holder.Reference<T>>> {
    public static final Dynamic2CommandExceptionType ERROR_NO_MATCHES = new Dynamic2CommandExceptionType((pattern, registry) -> Component.translatableEscape("argument.resource_selector.not_found", pattern, registry));
    private final HolderLookup<T> registryLookup;
    private final ResourceKey<? extends Registry<T>> registryKey;
    private String lastPattern = "";

    private static <T> Collection<Holder.Reference<T>> select(StringReader reader, HolderLookup<T> registry) {
        int start = reader.getCursor();
        while (reader.canRead() && (Identifier.isAllowedInIdentifier(reader.peek()) || reader.peek() == '*' || reader.peek() == '?')) {
            reader.skip();
        }
        String raw = reader.getString().substring(start, reader.getCursor());
        String pattern = raw.contains(":") ? raw : "minecraft:" + raw;
        return registry.listElements().filter(element -> wildcard(pattern, element.key().identifier().toString())).toList();
    }

    private static boolean wildcard(String pattern, String text) {
        int p = 0;
        int t = 0;
        int star = -1;
        int mark = 0;
        while (t < text.length()) {
            if (p < pattern.length() && (pattern.charAt(p) == '?' || pattern.charAt(p) == text.charAt(t))) {
                p++;
                t++;
            } else if (p < pattern.length() && pattern.charAt(p) == '*') {
                star = p++;
                mark = t;
            } else if (star >= 0) {
                p = star + 1;
                t = ++mark;
            } else {
                return false;
            }
        }
        while (p < pattern.length() && pattern.charAt(p) == '*') {
            p++;
        }
        return p == pattern.length();
    }
    private ResourceSelectorArgument(CommandBuildContext context, ResourceKey<? extends Registry<T>> registryKey) {
        this.registryKey = registryKey;
        this.registryLookup = context.lookupOrThrow(registryKey);
        
    }
    public Collection<Holder.Reference<T>> parse(StringReader reader) throws CommandSyntaxException {
        Collection<Holder.Reference<T>> found = select(reader, registryLookup);
        if (found.isEmpty()) {
            throw ERROR_NO_MATCHES.createWithContext(reader, lastPattern, registryKey.identifier());
        }
        return found;
    }
    public static <T> Collection<Holder.Reference<T>> parse(StringReader reader, HolderLookup<T> registry) {
        return select(reader, registry);
    }
    public static <T> ResourceSelectorArgument<T> resourceSelector(CommandBuildContext context, ResourceKey<? extends Registry<T>> registry) {
        return new ResourceSelectorArgument<>(context, registry);
    }
    public static <T> Collection<Holder.Reference<T>> getSelectedResources(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Collection.class);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return SharedSuggestionProvider.listSuggestions(context, builder, registryKey, SharedSuggestionProvider.ElementSuggestionType.ELEMENTS);
    }
    public Collection<String> getExamples() {
        return java.util.List.of("minecraft:*", "*:foo", "*");
    }
    public static class Info<T> implements ArgumentTypeInfo<ResourceSelectorArgument<T>, Info<T>.Template> {
        public void serializeToNetwork(Info<T>.Template template, FriendlyByteBuf out) {
            out.writeResourceKey(template.registryKey);
        }
        public Info<T>.Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readRegistryKey());
        }
        public void serializeToJson(Info<T>.Template template, JsonObject out) {
            out.addProperty("registry", template.registryKey.identifier().toString());
        }
        public Info<T>.Template unpack(ResourceSelectorArgument<T> argument) {
            return new Template(argument.registryKey);
        }
        public final class Template implements ArgumentTypeInfo.Template<ResourceSelectorArgument<T>> {
            final ResourceKey<? extends Registry<T>> registryKey;

            private Template(ResourceKey<? extends Registry<T>> registryKey) {
                this.registryKey = registryKey;
                
            }
            public ResourceSelectorArgument<T> instantiate(CommandBuildContext context) {
                return new ResourceSelectorArgument<>(context, registryKey);
            }
            public ArgumentTypeInfo<ResourceSelectorArgument<T>, ?> type() {
                return Info.this;
            }
        }
    }
}
