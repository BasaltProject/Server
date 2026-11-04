// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.player.StackedContents;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.display.SlotDisplay;
import net.minecraft.world.level.ItemLike;
public final class Ingredient implements Predicate<ItemStack>, StackedContents.IngredientInfo<Holder<Item>> {
    public static final StreamCodec<RegistryFriendlyByteBuf, Ingredient> CONTENTS_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.Ingredient.class, "CONTENTS_STREAM_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<Ingredient>> OPTIONAL_CONTENTS_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.Ingredient.class, "OPTIONAL_CONTENTS_STREAM_CODEC");
    public static final Codec<HolderSet<Item>> NON_AIR_HOLDER_SET_CODEC = org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(Item.class);
    public static final Codec<Ingredient> CODEC = org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(Item.class).xmap(Ingredient::new, ingredient -> ingredient.values);
    private final HolderSet<Item> values;

    private Ingredient(HolderSet<Item> values) {
        this.values = values;
    }
    public static boolean testOptionalIngredient(Optional<Ingredient> ingredient, ItemStack stack) {
        return ingredient.map(value -> value.test(stack)).orElseGet(stack::isEmpty);
    }
    public Stream<Holder<Item>> items() {
        return values.stream();
    }
    public boolean isEmpty() {
        return values.size() == 0 && values.unwrapKey().isEmpty();
    }
    public boolean test(ItemStack input) {
        return !input.isEmpty() && org.basaltmc.bukkit.facade.HolderSets.contains(values, input.typeHolder());
    }
    public boolean acceptsItem(Holder<Item> item) {
        return org.basaltmc.bukkit.facade.HolderSets.contains(values, item);
    }
    public boolean equals(Object o) {
        return o instanceof Ingredient other && values.equals(other.values);
    }
    public int hashCode() {
        return values.hashCode();
    }
    public static Ingredient of(ItemLike itemLike) {
        return new Ingredient(HolderSet.direct(item -> item.asItem().builtInRegistryHolder(), itemLike));
    }
    public static Ingredient of(ItemLike... items) {
        return new Ingredient(HolderSet.direct(item -> item.asItem().builtInRegistryHolder(), items));
    }
    public static Ingredient of(Stream<? extends ItemLike> stream) {
        return new Ingredient(HolderSet.direct(item -> item.asItem().builtInRegistryHolder(), stream.toList()));
    }
    public static Ingredient of(HolderSet<Item> tag) {
        return new Ingredient(tag);
    }
    public SlotDisplay display() {
        if (values.size() == 1) {
            return new SlotDisplay.ItemSlotDisplay(values.get(0));
        }
        return new SlotDisplay.TagSlotDisplay(values);
    }
    public static SlotDisplay optionalIngredientToDisplay(Optional<Ingredient> ingredient) {
        return ingredient.map(Ingredient::display).orElse(SlotDisplay.Empty.INSTANCE);
    }
    public Optional<Holder<Item>> getSingleItem() {
        return values.size() == 1 ? Optional.of(values.get(0)) : Optional.empty();
    }
}
