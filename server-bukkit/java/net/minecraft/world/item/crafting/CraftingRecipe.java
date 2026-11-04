// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.NonNullList;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
public interface CraftingRecipe extends Recipe<CraftingInput> {
    default RecipeType<CraftingRecipe> getType() {
        return RecipeType.CRAFTING;
    }
    RecipeSerializer<? extends CraftingRecipe> getSerializer();
    CraftingBookCategory category();
    default NonNullList<ItemStack> getRemainingItems(CraftingInput input) {
        return defaultCraftingReminder(input);
    }
    static NonNullList<ItemStack> defaultCraftingReminder(CraftingInput input) {
        NonNullList<ItemStack> remaining = NonNullList.withSize(input.size(), ItemStack.EMPTY);
        return remaining;
    }
    default RecipeBookCategory recipeBookCategory() {
        return null;
    }
    public static record CraftingBookInfo(CraftingBookCategory category, String group) implements Recipe.BookInfo<CraftingBookCategory> {
        public static final MapCodec<CraftingBookInfo> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.CraftingRecipe.CraftingBookInfo.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, CraftingBookInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.CraftingRecipe.CraftingBookInfo.class, "STREAM_CODEC");
    }
}
