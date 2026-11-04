// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.NonNullList;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class BookCloningRecipe extends CustomRecipe {
    public static final MinMaxBounds.Ints ALLOWED_BOOK_GENERATION_RANGES = MinMaxBounds.Ints.between(0, 2);
    public static final MinMaxBounds.Ints DEFAULT_BOOK_GENERATION_RANGES = MinMaxBounds.Ints.between(0, 1);
    public static final MapCodec<BookCloningRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.BookCloningRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, BookCloningRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.BookCloningRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<BookCloningRecipe> SERIALIZER = null;
    private final Ingredient source;
    private final Ingredient material;
    private final MinMaxBounds.Ints allowedGenerations;
    private final ItemStackTemplate result;

    public BookCloningRecipe(Ingredient source, Ingredient material, MinMaxBounds.Ints allowedGenerations, ItemStackTemplate result) {
        this.source = source;
        this.material = material;
        this.allowedGenerations = allowedGenerations;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() < 2) {
            return false;
        }
        boolean hasSource = false;
        boolean hasMaterial = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (source.test(stack)) {
                net.minecraft.world.item.component.WrittenBookContent content = stack.get(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT);
                if (content == null || !allowedGenerations.matches(content.generation()) || hasSource) {
                    return false;
                }
                hasSource = true;
            } else if (material.test(stack)) {
                hasMaterial = true;
            } else {
                return false;
            }
        }
        return hasSource && hasMaterial;
    }
    public ItemStack assemble(CraftingInput input) {
        int copies = 0;
        ItemStack original = ItemStack.EMPTY;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (source.test(stack) && stack.has(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT)) {
                if (!original.isEmpty()) {
                    return ItemStack.EMPTY;
                }
                original = stack;
            } else if (material.test(stack)) {
                copies++;
            } else {
                return ItemStack.EMPTY;
            }
        }
        net.minecraft.world.item.component.WrittenBookContent content = original.get(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT);
        if (content == null) {
            return ItemStack.EMPTY;
        }
        ItemStack out = TransmuteRecipe.createWithOriginalComponents(result, original, copies - 1);
        out.set(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT, content.craftCopy());
        return out;
    }
    public NonNullList<ItemStack> getRemainingItems(CraftingInput input) {
        net.minecraft.core.NonNullList<ItemStack> remaining = net.minecraft.core.NonNullList.withSize(input.size(), ItemStack.EMPTY);
        for (int slot = 0; slot < remaining.size(); slot++) {
            ItemStack stack = input.getItem(slot);
            ItemStackTemplate remainder = stack.getItem().getCraftingRemainder();
            if (remainder != null) {
                remaining.set(slot, remainder.create());
            } else if (stack.has(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT)) {
                remaining.set(slot, stack.copyWithCount(1));
                break;
            }
        }
        return remaining;
    }
    public RecipeSerializer<BookCloningRecipe> getSerializer() {
        return SERIALIZER;
    }
}
