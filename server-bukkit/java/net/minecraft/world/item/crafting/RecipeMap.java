// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.google.common.collect.Multimap;
import java.util.Collection;
import java.util.Map;
import java.util.stream.Stream;
import net.minecraft.core.HolderLookup;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
public class RecipeMap {
    public static final RecipeMap EMPTY = new RecipeMap(com.google.common.collect.ImmutableMultimap.of(), Map.of());
    private final Multimap<RecipeType<?>, RecipeHolder<?>> byType;
    private final Map<ResourceKey<Recipe<?>>, RecipeHolder<?>> byKey;
    private RecipeMap(Multimap<RecipeType<?>, RecipeHolder<?>> byType, Map<ResourceKey<Recipe<?>>, RecipeHolder<?>> byKey) {
        this.byType = byType;
        this.byKey = byKey;
    }
    public static RecipeMap create(HolderLookup<Recipe<?>> recipes) {
        com.google.common.collect.ImmutableMultimap.Builder<RecipeType<?>, RecipeHolder<?>> types = com.google.common.collect.ImmutableMultimap.builder();
        java.util.Map<ResourceKey<Recipe<?>>, RecipeHolder<?>> keys = new java.util.LinkedHashMap<>();
        recipes.listElements().forEach(reference -> {
            RecipeHolder<?> holder = new RecipeHolder<>(reference.key(), reference.value());
            types.put(reference.value().getType(), holder);
            keys.put(reference.key(), holder);
        });
        return new RecipeMap(types.build(), java.util.Collections.unmodifiableMap(keys));
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <I extends RecipeInput, T extends Recipe<I>> Collection<RecipeHolder<T>> byType(RecipeType<T> type) {
        return (Collection<RecipeHolder<T>>) (Collection) byType.get(type);
    }
    public Collection<RecipeHolder<?>> values() {
        return byKey.values();
    }
    public RecipeHolder<?> byKey(ResourceKey<Recipe<?>> recipeId) {
        return byKey.get(recipeId);
    }
    public <I extends RecipeInput, T extends Recipe<I>> Stream<RecipeHolder<T>> getRecipesFor(RecipeType<T> type, I container, Level level) {
        if (container.isEmpty()) {
            return Stream.empty();
        }
        return byType(type).stream().filter(holder -> holder.value().matches(container, level));
    }
}
