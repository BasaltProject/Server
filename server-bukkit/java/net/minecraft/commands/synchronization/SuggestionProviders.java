// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.suggestion.SuggestionProvider;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;
public class SuggestionProviders {
    private static final Map<Identifier, SuggestionProvider<SharedSuggestionProvider>> kByName = new HashMap<>();
    private static final Identifier kAskServerId = Identifier.withDefaultNamespace("ask_server");
    public static final SuggestionProvider<SharedSuggestionProvider> ASK_SERVER = register(kAskServerId, (c, p) -> c.getSource().customSuggestion(c));
    public static final SuggestionProvider<SharedSuggestionProvider> AVAILABLE_SOUNDS = register(Identifier.withDefaultNamespace("available_sounds"),
            (c, p) -> SharedSuggestionProvider.suggestResource(c.getSource().getAvailableSounds(), p));
    public static final SuggestionProvider<SharedSuggestionProvider> SUMMONABLE_ENTITIES = register(Identifier.withDefaultNamespace("summonable_entities"),
            (c, p) -> SharedSuggestionProvider.suggestResource(
                    BuiltInRegistries.ENTITY_TYPE.stream().filter(type -> type.isEnabled(c.getSource().enabledFeatures()) && type.canSummon()),
                    p, EntityType::getKey, EntityType::getDescription));
    public static final SuggestionProvider<SharedSuggestionProvider> POST_EFFECTS = register(Identifier.withDefaultNamespace("post_effects"),
            (c, p) -> SharedSuggestionProvider.suggestResource(c.getSource().getAvailablePostEffects(), p));
    @SuppressWarnings("unchecked")
    public static <S extends SharedSuggestionProvider> SuggestionProvider<S> register(Identifier name, SuggestionProvider<SharedSuggestionProvider> provider) {
        if (kByName.putIfAbsent(name, provider) != null) {
            throw new IllegalArgumentException("A command suggestion provider is already registered with the name '" + name + "'");
        }
        return (SuggestionProvider<S>) (SuggestionProvider<?>) new RegisteredSuggestion(name, provider);
    }
    @SuppressWarnings("unchecked")
    public static <S extends SharedSuggestionProvider> SuggestionProvider<S> cast(SuggestionProvider<SharedSuggestionProvider> provider) {
        return (SuggestionProvider<S>) (SuggestionProvider<?>) provider;
    }
    public static <S extends SharedSuggestionProvider> SuggestionProvider<S> getProvider(Identifier name) {
        return cast(kByName.getOrDefault(name, ASK_SERVER));
    }
    public static Identifier getName(SuggestionProvider<?> provider) {
        return provider instanceof RegisteredSuggestion registered ? registered.name : kAskServerId;
    }
    static record RegisteredSuggestion(Identifier name, SuggestionProvider<SharedSuggestionProvider> delegate) implements SuggestionProvider<SharedSuggestionProvider> {
        public CompletableFuture<Suggestions> getSuggestions(CommandContext<SharedSuggestionProvider> context, SuggestionsBuilder builder) throws com.mojang.brigadier.exceptions.CommandSyntaxException {
            return delegate.getSuggestions(context, builder);
        }
    }
}
