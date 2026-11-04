// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.google.gson.JsonObject;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.timeline.Timeline;
public class ResourceArgument<T> implements ArgumentType<Holder.Reference<T>> {
    public static final Dynamic2CommandExceptionType ERROR_UNKNOWN_RESOURCE = new com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType((id, registry) -> new com.mojang.brigadier.LiteralMessage("Unknown " + registry + ": " + id));
    public static final Dynamic3CommandExceptionType ERROR_INVALID_RESOURCE_TYPE = new com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType((id, type, expected) -> new com.mojang.brigadier.LiteralMessage("Resource " + id + " is of type " + type + ", expected " + expected));
    private ResourceKey<? extends Registry<T>> registryKey;
    public ResourceArgument(CommandBuildContext context, ResourceKey<? extends Registry<T>> registryKey) {
        this.registryKey = registryKey;
        
    }
    public static <T> ResourceArgument<T> resource(CommandBuildContext context, ResourceKey<? extends Registry<T>> key) {
        return new ResourceArgument<>(context, key);
    }
    public static <T> Holder.Reference<T> getResource(CommandContext<CommandSourceStack> context, String name, ResourceKey<Registry<T>> registryKey) throws CommandSyntaxException {
        Holder.Reference<T> holder = context.getArgument(name, Holder.Reference.class);
        return holder;
    }
    public static Holder.Reference<Attribute> getAttribute(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<Attribute>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.ATTRIBUTE);
    }
    public static Holder.Reference<EntityType<?>> getSummonableEntityType(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<EntityType<?>>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.ENTITY_TYPE);
    }
    public static Holder.Reference<MobEffect> getMobEffect(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<MobEffect>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.MOB_EFFECT);
    }
    public static Holder.Reference<Enchantment> getEnchantment(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<Enchantment>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.ENCHANTMENT);
    }
    public static Holder.Reference<WorldClock> getClock(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<WorldClock>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.WORLD_CLOCK);
    }
    public static Holder.Reference<Timeline> getTimeline(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return (Holder.Reference<Timeline>) (Holder.Reference) getResource(context, name, (ResourceKey) net.minecraft.core.registries.Registries.TIMELINE);
    }
    public Holder.Reference<T> parse(StringReader reader) throws CommandSyntaxException {
        net.minecraft.resources.Identifier id = net.minecraft.resources.Identifier.read(reader);
        Registry<T> registry = net.minecraft.server.MinecraftServer.getServer().registryAccess().<T>lookupOrThrow(registryKey);
        return registry.get(id).orElseThrow(() -> ERROR_UNKNOWN_RESOURCE.create(id, registryKey.identifier()));
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        Registry<T> registry = net.minecraft.server.MinecraftServer.getServer().registryAccess().<T>lookupOrThrow(registryKey);
        String remaining = builder.getRemaining().toLowerCase(java.util.Locale.ROOT);
        for (net.minecraft.resources.Identifier id : registry.keySet()) {
            if (id.toString().startsWith(remaining) || id.getPath().startsWith(remaining)) {
        builder.suggest(id.toString());
            }
        }
        return builder.buildFuture();
    }
    public Collection<String> getExamples() {
        return java.util.List.of("foo", "foo:bar", "012");
    }
    public static class Info<T> implements ArgumentTypeInfo<ResourceArgument<T>, Info<T>.Template> {
        public void serializeToNetwork(Info<T>.Template template, FriendlyByteBuf out) {
            out.writeResourceKey(template.registryKey);
        }

        public Info<T>.Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readRegistryKey());
        }

        public void serializeToJson(Info<T>.Template template, JsonObject out) {
            out.addProperty("registry", template.registryKey.identifier().toString());
        }

        public Info<T>.Template unpack(ResourceArgument<T> argument) {
            return new Template(argument.registryKey);
        }

        public final class Template implements ArgumentTypeInfo.Template<ResourceArgument<T>> {
            final ResourceKey<? extends Registry<T>> registryKey;

            private Template(ResourceKey<? extends Registry<T>> registryKey) {
                this.registryKey = registryKey;
            }

            public ResourceArgument<T> instantiate(CommandBuildContext context) {
                return new ResourceArgument<>(context, registryKey);
            }

            public ArgumentTypeInfo<ResourceArgument<T>, ?> type() {
                return Info.this;
            }
        }
    }

}
