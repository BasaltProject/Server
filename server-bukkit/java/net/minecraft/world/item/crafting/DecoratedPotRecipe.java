// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class DecoratedPotRecipe extends CustomRecipe {
    public static final MapCodec<DecoratedPotRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Ingredient.CODEC.fieldOf("back").forGetter(o -> o.backPattern),
        Ingredient.CODEC.fieldOf("left").forGetter(o -> o.leftPattern),
        Ingredient.CODEC.fieldOf("right").forGetter(o -> o.rightPattern),
        Ingredient.CODEC.fieldOf("front").forGetter(o -> o.frontPattern),
        ItemStackTemplate.CODEC.fieldOf("result").forGetter(o -> o.result)).apply(i, DecoratedPotRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, DecoratedPotRecipe> STREAM_CODEC = StreamCodec.composite(
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.backPattern,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.leftPattern,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.rightPattern,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.frontPattern,
        ItemStackTemplate.STREAM_CODEC, o -> o.result,
        DecoratedPotRecipe::new);
    public static final RecipeSerializer<DecoratedPotRecipe> SERIALIZER = null;
    private final Ingredient backPattern;
    private final Ingredient leftPattern;
    private final Ingredient rightPattern;
    private final Ingredient frontPattern;
    private final ItemStackTemplate result;

    public DecoratedPotRecipe(Ingredient wallPattern, ItemStackTemplate result) {
        this(wallPattern, wallPattern, wallPattern, wallPattern, result);
    }
    public DecoratedPotRecipe(Ingredient backPattern, Ingredient leftPattern, Ingredient rightPattern, Ingredient frontPattern, ItemStackTemplate result) {
        this.backPattern = backPattern;
        this.leftPattern = leftPattern;
        this.rightPattern = rightPattern;
        this.frontPattern = frontPattern;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        return input.width() == 3 && input.height() == 3 && input.ingredientCount() == 4
            && backPattern.test(input.getItem(1, 0)) && leftPattern.test(input.getItem(0, 1))
            && rightPattern.test(input.getItem(2, 1)) && frontPattern.test(input.getItem(1, 2));
    }
    public ItemStack assemble(CraftingInput input) {
        net.minecraft.world.level.block.entity.PotDecorations decorations = new net.minecraft.world.level.block.entity.PotDecorations(
            single(input.getItem(1, 0)), single(input.getItem(0, 1)), single(input.getItem(2, 1)), single(input.getItem(1, 2)));
        return result.apply(net.minecraft.core.component.DataComponentPatch.builder()
            .set(net.minecraft.core.component.DataComponents.POT_DECORATIONS, decorations).build());
    }
    public RecipeSerializer<DecoratedPotRecipe> getSerializer() {
        return SERIALIZER;
    }
    private static java.util.Optional<ItemStackTemplate> single(ItemStack stack) {
        return stack.isEmpty() ? java.util.Optional.empty() : java.util.Optional.of(ItemStackTemplate.fromNonEmptyStack(stack, 1));
    }
}
