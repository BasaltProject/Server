// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.level.Level;
public class DyeRecipe extends NormalCraftingRecipe {
    public static final MapCodec<DyeRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.DyeRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, DyeRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.DyeRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<DyeRecipe> SERIALIZER = null;
    private final Ingredient target;
    private final Ingredient dye;
    private final ItemStackTemplate result;

    public DyeRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo, Ingredient target, Ingredient dye, ItemStackTemplate result) {
        super(commonInfo, bookInfo);
        this.target = target;
        this.dye = dye;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() < 2) {
            return false;
        }
        boolean hasTarget = false;
        boolean hasDye = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (target.test(stack)) {
                if (hasTarget) {
                    return false;
                }
                hasTarget = true;
            } else if (dye.test(stack) && stack.has(net.minecraft.core.component.DataComponents.DYE)) {
                hasDye = true;
            } else {
                return false;
            }
        }
        return hasTarget && hasDye;
    }
    public ItemStack assemble(CraftingInput input) {
        java.util.List<net.minecraft.world.item.DyeColor> colors = new java.util.ArrayList<>();
        ItemStack targetStack = ItemStack.EMPTY;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (target.test(stack)) {
                if (!targetStack.isEmpty()) {
                    return ItemStack.EMPTY;
                }
                targetStack = stack;
            } else if (dye.test(stack)) {
                colors.add(stack.getOrDefault(net.minecraft.core.component.DataComponents.DYE, net.minecraft.world.item.DyeColor.WHITE));
            } else {
                return ItemStack.EMPTY;
            }
        }
        if (targetStack.isEmpty() || colors.isEmpty()) {
            return ItemStack.EMPTY;
        }
        ItemStack out = TransmuteRecipe.createWithOriginalComponents(result, targetStack);
        out.set(net.minecraft.core.component.DataComponents.DYED_COLOR,
            net.minecraft.world.item.component.DyedItemColor.applyDyes(targetStack.get(net.minecraft.core.component.DataComponents.DYED_COLOR), colors));
        return out;
    }
    public RecipeSerializer<DyeRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return null;
    }
    public List<RecipeDisplay> display() {
        return List.of();
    }
}
