// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class ShieldDecorationRecipe extends CustomRecipe {
    public static final MapCodec<ShieldDecorationRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Ingredient.CODEC.fieldOf("banner").forGetter(o -> o.banner),
        Ingredient.CODEC.fieldOf("target").forGetter(o -> o.target),
        ItemStackTemplate.CODEC.fieldOf("result").forGetter(o -> o.result)).apply(i, ShieldDecorationRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, ShieldDecorationRecipe> STREAM_CODEC = StreamCodec.composite(
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.banner,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.target,
        ItemStackTemplate.STREAM_CODEC, o -> o.result,
        ShieldDecorationRecipe::new);
    public static final RecipeSerializer<ShieldDecorationRecipe> SERIALIZER = new RecipeSerializer<>(MAP_CODEC, STREAM_CODEC);
    private final Ingredient banner;
    private final Ingredient target;
    private final ItemStackTemplate result;

    public ShieldDecorationRecipe(Ingredient banner, Ingredient target, ItemStackTemplate result) {
        this.banner = banner;
        this.target = target;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() != 2) {
            return false;
        }
        boolean hasTarget = false;
        boolean hasBanner = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (banner.test(stack) && stack.getItem() instanceof net.minecraft.world.item.BannerItem) {
                if (hasBanner) {
                    return false;
                }
                hasBanner = true;
            } else {
                if (!target.test(stack) || hasTarget) {
                    return false;
                }
                if (!stack.getOrDefault(net.minecraft.core.component.DataComponents.BANNER_PATTERNS,
                    net.minecraft.world.level.block.entity.BannerPatternLayers.EMPTY).layers().isEmpty()) {
                    return false;
                }
                hasTarget = true;
            }
        }
        return hasTarget && hasBanner;
    }
    public ItemStack assemble(CraftingInput input) {
        net.minecraft.world.level.block.entity.BannerPatternLayers patterns = null;
        net.minecraft.world.item.DyeColor baseColor = net.minecraft.world.item.DyeColor.WHITE;
        ItemStack targetStack = ItemStack.EMPTY;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (banner.test(stack) && stack.getItem() instanceof net.minecraft.world.item.BannerItem bannerItem) {
                patterns = stack.get(net.minecraft.core.component.DataComponents.BANNER_PATTERNS);
                baseColor = bannerItem.getColor();
            } else if (target.test(stack)) {
                targetStack = stack;
            }
        }
        ItemStack out = TransmuteRecipe.createWithOriginalComponents(result, targetStack);
        out.set(net.minecraft.core.component.DataComponents.BANNER_PATTERNS, patterns);
        out.set(net.minecraft.core.component.DataComponents.BASE_COLOR, baseColor);
        return out;
    }
    public RecipeSerializer<ShieldDecorationRecipe> getSerializer() {
        return SERIALIZER;
    }
}
