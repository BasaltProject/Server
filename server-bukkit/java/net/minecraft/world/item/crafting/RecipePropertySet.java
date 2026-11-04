// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.Collection;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
public class RecipePropertySet {
    public static final ResourceKey<? extends Registry<RecipePropertySet>> TYPE_KEY = ResourceKey.createRegistryKey(net.minecraft.resources.Identifier.withDefaultNamespace("recipe_property_set"));
    public static final ResourceKey<RecipePropertySet> SMITHING_BASE = registerKey("smithing_base");
    public static final ResourceKey<RecipePropertySet> SMITHING_TEMPLATE = registerKey("smithing_template");
    public static final ResourceKey<RecipePropertySet> SMITHING_ADDITION = registerKey("smithing_addition");
    public static final ResourceKey<RecipePropertySet> FURNACE_INPUT = registerKey("furnace_input");
    public static final ResourceKey<RecipePropertySet> BLAST_FURNACE_INPUT = registerKey("blast_furnace_input");
    public static final ResourceKey<RecipePropertySet> SMOKER_INPUT = registerKey("smoker_input");
    public static final ResourceKey<RecipePropertySet> CAMPFIRE_INPUT = registerKey("campfire_input");
    public static final ResourceKey<RecipePropertySet> BREWING_INPUTS = registerKey("brewing_inputs");
    public static final ResourceKey<RecipePropertySet> BREWING_REAGENTS = registerKey("brewing_reagents");
    public static final StreamCodec<RegistryFriendlyByteBuf, RecipePropertySet> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.RecipePropertySet.class, "STREAM_CODEC");
    public static final RecipePropertySet EMPTY = new RecipePropertySet(Set.of());
    private final Set<Holder<Item>> items;
    private RecipePropertySet(Set<Holder<Item>> items) {
        this.items = items;
    }
    private static ResourceKey<RecipePropertySet> registerKey(String name) {
        return ResourceKey.create(TYPE_KEY, net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }
    public boolean test(ItemStack itemStack) {
        return items.contains(itemStack.typeHolder());
    }
    public static RecipePropertySet create(Collection<Ingredient> ingredients) {
        Set<Holder<Item>> collected = new java.util.LinkedHashSet<>();
        for (Ingredient ingredient : ingredients) {
            ingredient.items().forEach(collected::add);
        }
        return new RecipePropertySet(collected);
    }
}
