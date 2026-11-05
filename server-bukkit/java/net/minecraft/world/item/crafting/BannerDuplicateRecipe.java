// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.NonNullList;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class BannerDuplicateRecipe extends CustomRecipe {
    public static final MapCodec<BannerDuplicateRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Ingredient.CODEC.fieldOf("banner").forGetter(o -> o.banner),
        ItemStackTemplate.CODEC.fieldOf("result").forGetter(o -> o.result)).apply(i, BannerDuplicateRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, BannerDuplicateRecipe> STREAM_CODEC = StreamCodec.composite(
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.banner,
        ItemStackTemplate.STREAM_CODEC, o -> o.result,
        BannerDuplicateRecipe::new);
    public static final RecipeSerializer<BannerDuplicateRecipe> SERIALIZER = null;
    private final Ingredient banner;
    private final ItemStackTemplate result;

    public BannerDuplicateRecipe(Ingredient banner, ItemStackTemplate result) {
        this.banner = banner;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() != 2) {
            return false;
        }
        net.minecraft.world.item.DyeColor color = null;
        boolean hasTarget = false;
        boolean hasSource = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (!banner.test(stack) || !(stack.getItem() instanceof net.minecraft.world.item.BannerItem bannerItem)) {
                return false;
            }
            if (color == null) {
                color = bannerItem.getColor();
            } else if (color != bannerItem.getColor()) {
                return false;
            }
            int layers = layerCount(stack);
            if (layers > 6) {
                return false;
            }
            if (layers > 0) {
                if (hasSource) {
                    return false;
                }
                hasSource = true;
            } else {
                if (hasTarget) {
                    return false;
                }
                hasTarget = true;
            }
        }
        return hasSource && hasTarget;
    }
    public ItemStack assemble(CraftingInput input) {
        for (ItemStack stack : input.items()) {
            int layers = stack.isEmpty() ? 0 : layerCount(stack);
            if (layers > 0 && layers <= 6) {
                return TransmuteRecipe.createWithOriginalComponents(result, stack);
            }
        }
        return ItemStack.EMPTY;
    }
    public NonNullList<ItemStack> getRemainingItems(CraftingInput input) {
        net.minecraft.core.NonNullList<ItemStack> remaining = net.minecraft.core.NonNullList.withSize(input.size(), ItemStack.EMPTY);
        for (int slot = 0; slot < remaining.size(); slot++) {
            ItemStack stack = input.getItem(slot);
            if (stack.isEmpty()) {
                continue;
            }
            ItemStackTemplate remainder = stack.getItem().getCraftingRemainder();
            if (remainder != null) {
                remaining.set(slot, remainder.create());
            } else if (layerCount(stack) > 0) {
                remaining.set(slot, stack.copyWithCount(1));
            }
        }
        return remaining;
    }
    public RecipeSerializer<BannerDuplicateRecipe> getSerializer() {
        return SERIALIZER;
    }
    private static int layerCount(ItemStack stack) {
        return stack.getOrDefault(net.minecraft.core.component.DataComponents.BANNER_PATTERNS,
            net.minecraft.world.level.block.entity.BannerPatternLayers.EMPTY).layers().size();
    }
}
