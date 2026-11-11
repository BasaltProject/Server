// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.item.equipment.trim.TrimPattern;
public class SmithingTrimRecipe extends SimpleSmithingRecipe {
    public static final MapCodec<SmithingTrimRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Recipe.CommonInfo.MAP_CODEC.forGetter(o -> o.commonInfo),
        Ingredient.CODEC.fieldOf("template").forGetter(o -> o.template),
        Ingredient.CODEC.fieldOf("base").forGetter(o -> o.base),
        Ingredient.CODEC.fieldOf("addition").forGetter(o -> o.addition),
        TrimPattern.CODEC.fieldOf("pattern").forGetter(o -> o.pattern)).apply(i, SmithingTrimRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, SmithingTrimRecipe> STREAM_CODEC = StreamCodec.composite(
        Recipe.CommonInfo.STREAM_CODEC, o -> o.commonInfo,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.template,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.base,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.addition,
        TrimPattern.STREAM_CODEC, o -> o.pattern,
        SmithingTrimRecipe::new);
    public static final RecipeSerializer<SmithingTrimRecipe> SERIALIZER = new RecipeSerializer<>(MAP_CODEC, STREAM_CODEC);
    private final Ingredient template;
    private final Ingredient base;
    private final Ingredient addition;
    private final Holder<TrimPattern> pattern;

    public SmithingTrimRecipe(Recipe.CommonInfo commonInfo, Ingredient template, Ingredient base, Ingredient addition, Holder<TrimPattern> pattern) {
        super(commonInfo);
        this.template = template;
        this.base = base;
        this.addition = addition;
        this.pattern = pattern;
    }
    public ItemStack assemble(SmithingRecipeInput input) {
        return applyTrim(input.base(), input.addition(), pattern);
    }
    public static ItemStack applyTrim(ItemStack baseItem, ItemStack materialItem, Holder<TrimPattern> pattern) {
        Holder<net.minecraft.world.item.equipment.trim.TrimMaterial> material = materialItem.get(net.minecraft.core.component.DataComponents.PROVIDES_TRIM_MATERIAL);
        if (material == null) {
            return ItemStack.EMPTY;
        }
        net.minecraft.world.item.equipment.trim.ArmorTrim next = new net.minecraft.world.item.equipment.trim.ArmorTrim(material, pattern);
        if (java.util.Objects.equals(baseItem.get(net.minecraft.core.component.DataComponents.TRIM), next)) {
            return ItemStack.EMPTY;
        }
        ItemStack out = baseItem.copyWithCount(1);
        out.set(net.minecraft.core.component.DataComponents.TRIM, next);
        return out;
    }
    public Optional<Ingredient> templateIngredient() {
        return Optional.of(template);
    }
    public Ingredient baseIngredient() {
        return base;
    }
    public Optional<Ingredient> additionIngredient() {
        return Optional.of(addition);
    }
    public RecipeSerializer<SmithingTrimRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return null;
    }
    public List<RecipeDisplay> display() {
        return List.of();
    }
}
