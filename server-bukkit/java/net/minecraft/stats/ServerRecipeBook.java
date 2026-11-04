// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import com.mojang.serialization.Codec;
import java.util.AbstractSet;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.RecipeHolder;
import net.minecraft.world.item.crafting.display.RecipeDisplayEntry;
import org.basaltmc.bukkit.player.PlayerCalls;

/** The recipes one player has unlocked, held by the native recipe service. */
public class ServerRecipeBook extends RecipeBook {
    public static final String RECIPE_BOOK_TAG = "recipeBook";
    protected final Set<ResourceKey<Recipe<?>>> known;
    protected final Set<ResourceKey<Recipe<?>>> highlight = new HashSet<>();
    private final UUID owner;

    public ServerRecipeBook(DisplayResolver displayResolver) {
        this.owner = null;
        this.known = new HashSet<>();
    }

    public ServerRecipeBook(ServerPlayer player) {
        this.owner = player.getUUID();
        this.known = new NativeKnown(owner);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<Recipe<?>> keyOf(String id) {
        return (ResourceKey) ResourceKey.create((ResourceKey) Registries.RECIPE, Identifier.parse(id));
    }

    public void add(ResourceKey<Recipe<?>> id) {
        known.add(id);
    }

    public boolean contains(ResourceKey<Recipe<?>> id) {
        return known.contains(id);
    }

    public void remove(ResourceKey<Recipe<?>> id) {
        known.remove(id);
        highlight.remove(id);
    }

    public void removeHighlight(ResourceKey<Recipe<?>> id) {
        highlight.remove(id);
    }

    public int addRecipes(Collection<RecipeHolder<?>> recipes, ServerPlayer player) {
        int added = 0;
        for (RecipeHolder<?> recipe : recipes) {
            if (known.add(recipe.id())) {
                added++;
            }
        }
        return added;
    }

    public int removeRecipes(Collection<RecipeHolder<?>> recipes, ServerPlayer player) {
        int removed = 0;
        for (RecipeHolder<?> recipe : recipes) {
            if (known.remove(recipe.id())) {
                removed++;
            }
        }
        return removed;
    }

    public void sendInitialRecipeBook(ServerPlayer player) {
    }

    public void copyOverData(ServerRecipeBook bookToCopy) {
        known.addAll(new ArrayList<>(bookToCopy.known));
        setBookSettings(bookToCopy.getBookSettings());
    }

    public Packed pack() {
        return new Packed(getBookSettings(), new ArrayList<>(known), new ArrayList<>(highlight));
    }

    public void loadUntrusted(Packed packed, Predicate<ResourceKey<Recipe<?>>> validator) {
        setBookSettings(packed.settings());
        for (ResourceKey<Recipe<?>> id : packed.known()) {
            if (validator.test(id)) {
                known.add(id);
            }
        }
        for (ResourceKey<Recipe<?>> id : packed.highlight()) {
            if (validator.test(id)) {
                highlight.add(id);
            }
        }
    }

    private static final class NativeKnown extends AbstractSet<ResourceKey<Recipe<?>>> {
        private final UUID owner;

        NativeKnown(UUID owner) {
            this.owner = owner;
        }

        private List<ResourceKey<Recipe<?>>> all() {
            List<ResourceKey<Recipe<?>>> out = new ArrayList<>();
            if (PlayerCalls.call(owner, "known_recipes") instanceof String[] ids) {
                for (String id : ids) {
                    out.add(keyOf(id));
                }
            }
            return out;
        }

        @Override
        public Iterator<ResourceKey<Recipe<?>>> iterator() {
            return all().iterator();
        }

        @Override
        public int size() {
            return all().size();
        }

        @Override
        public boolean contains(Object o) {
            return o instanceof ResourceKey<?> key && PlayerCalls.done(owner, "has_recipe", key.identifier().toString());
        }

        @Override
        public boolean add(ResourceKey<Recipe<?>> key) {
            return PlayerCalls.count(owner, "discover_recipes", (Object) new String[] {key.identifier().toString()}) > 0;
        }

        @Override
        public boolean remove(Object o) {
            return o instanceof ResourceKey<?> key
                && PlayerCalls.count(owner, "undiscover_recipes", (Object) new String[] {key.identifier().toString()}) > 0;
        }
    }

    public static record Packed(RecipeBookSettings settings, List<ResourceKey<Recipe<?>>> known, List<ResourceKey<Recipe<?>>> highlight) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.stats.ServerRecipeBook.Packed.class, "CODEC");
    }

    public interface DisplayResolver {
        void displaysForRecipe(ResourceKey<Recipe<?>> id, Consumer<RecipeDisplayEntry> output);
    }
}
