// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.google.gson.JsonObject;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import java.util.function.Predicate;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.RecipeHolder;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class ResourceKeyArgument<T> implements ArgumentType<ResourceKey<T>> {
    private ResourceKey<? extends Registry<T>> registryKey;
    private Predicate<T> filter = value -> true;
    public ResourceKeyArgument(ResourceKey<? extends Registry<T>> registryKey) {
        this.registryKey = registryKey;
        
    }
    public ResourceKeyArgument(ResourceKey<? extends Registry<T>> registryKey, Predicate<T> filter) {
        this.registryKey = registryKey;
        this.filter = filter;
        
    }
    public static <T> ResourceKeyArgument<T> key(ResourceKey<? extends Registry<T>> key) {
        return new ResourceKeyArgument<>(key);
    }
    public static <T> ResourceKeyArgument<T> key(ResourceKey<? extends Registry<T>> key, Predicate<T> filter) {
        return new ResourceKeyArgument<>(key, filter);
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Holder.Reference<?> lookupHolder(CommandContext<CommandSourceStack> context, String name, ResourceKey registryKey) throws CommandSyntaxException {
        ResourceKey<?> key = context.getArgument(name, ResourceKey.class);
        Registry registry = org.basaltmc.bukkit.facade.ServerAccess.registries().lookupOrThrow(registryKey);
        java.util.Optional<Holder.Reference<?>> found = (java.util.Optional<Holder.Reference<?>>) (java.util.Optional) registry.get(key.identifier());
        if (found.isEmpty()) {
            throw ResourceArgument.ERROR_UNKNOWN_RESOURCE.create(key.identifier(), registryKey.identifier());
        }
        return found.get();
    }

    @SuppressWarnings("unchecked")
    public static <T> ResourceKey<T> getRegistryKey(CommandContext<CommandSourceStack> context, String name, ResourceKey<Registry<T>> registryKey, DynamicCommandExceptionType exceptionType) throws CommandSyntaxException {
        ResourceKey<?> key = context.getArgument(name, ResourceKey.class);
        if (key.isFor(registryKey)) {
            return (ResourceKey<T>) key;
        }
        throw exceptionType.create(key.identifier());
    }
    public static Holder.Reference<Structure> getStructure(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<Structure>) (Holder.Reference) lookupHolder(context, name, net.minecraft.core.registries.Registries.STRUCTURE);
    }
    public static Holder.Reference<StructureTemplatePool> getStructureTemplatePool(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<StructureTemplatePool>) (Holder.Reference) lookupHolder(context, name, net.minecraft.core.registries.Registries.TEMPLATE_POOL);
    }
    private static final DynamicCommandExceptionType kInvalidRecipe = new DynamicCommandExceptionType(
        id -> net.minecraft.network.chat.Component.translatableEscape("recipe.notFound", id));
    private static final DynamicCommandExceptionType kInvalidAdvancement = new DynamicCommandExceptionType(
        id -> net.minecraft.network.chat.Component.translatableEscape("advancement.advancementNotFound", id));
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static RecipeHolder<?> getRecipe(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        net.minecraft.world.item.crafting.RecipeManager recipes = context.getSource().getServer().getRecipeManager();
        ResourceKey<net.minecraft.world.item.crafting.Recipe<?>> key = (ResourceKey) getRegistryKey(context, name, net.minecraft.core.registries.Registries.RECIPE, kInvalidRecipe);
        return recipes.byKey(key).filter(holder -> !holder.value().isSpecial()).orElseThrow(() -> kInvalidRecipe.create(key.identifier()));
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static AdvancementHolder getAdvancement(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        ResourceKey<net.minecraft.advancements.Advancement> key = (ResourceKey) getRegistryKey(context, name, net.minecraft.core.registries.Registries.ADVANCEMENT, kInvalidAdvancement);
        AdvancementHolder advancement = context.getSource().getServer().getAdvancements().get(key.identifier());
        if (advancement == null) {
            throw kInvalidAdvancement.create(key.identifier());
        }
        return advancement;
    }
    public ResourceKey<T> parse(StringReader reader) throws CommandSyntaxException {
        return ResourceKey.create(registryKey, net.minecraft.resources.Identifier.read(reader));
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        Registry<T> registry = net.minecraft.server.MinecraftServer.getServer().registryAccess().<T>lookupOrThrow(registryKey);
        String remaining = builder.getRemaining().toLowerCase(java.util.Locale.ROOT);
        for (net.minecraft.resources.Identifier id : registry.keySet()) {
            if (id.toString().startsWith(remaining) || id.getPath().startsWith(remaining)) {
        T value = registry.getValue(id);
        if (value != null && filter.test(value)) {
            builder.suggest(id.toString());
        }
            }
        }
        return builder.buildFuture();
    }
    public Collection<String> getExamples() {
        return java.util.List.of("foo", "foo:bar", "012");
    }
    public static class Info<T> implements ArgumentTypeInfo<ResourceKeyArgument<T>, Info<T>.Template> {
        public void serializeToNetwork(Info<T>.Template template, FriendlyByteBuf out) {
            out.writeResourceKey(template.registryKey);
        }

        public Info<T>.Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readRegistryKey());
        }

        public void serializeToJson(Info<T>.Template template, JsonObject out) {
            out.addProperty("registry", template.registryKey.identifier().toString());
        }

        public Info<T>.Template unpack(ResourceKeyArgument<T> argument) {
            return new Template(argument.registryKey);
        }

        public final class Template implements ArgumentTypeInfo.Template<ResourceKeyArgument<T>> {
            final ResourceKey<? extends Registry<T>> registryKey;

            private Template(ResourceKey<? extends Registry<T>> registryKey) {
                this.registryKey = registryKey;
            }

            public ResourceKeyArgument<T> instantiate(CommandBuildContext context) {
                return new ResourceKeyArgument<>(registryKey);
            }

            public ArgumentTypeInfo<ResourceKeyArgument<T>, ?> type() {
                return Info.this;
            }
        }
    }

}
