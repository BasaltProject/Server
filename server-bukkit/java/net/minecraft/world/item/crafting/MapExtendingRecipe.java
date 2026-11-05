// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class MapExtendingRecipe extends CustomRecipe {
    public static final MapCodec<MapExtendingRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Ingredient.CODEC.fieldOf("map").forGetter(o -> o.map),
        Ingredient.CODEC.fieldOf("material").forGetter(o -> o.material),
        TransmuteResult.CODEC.fieldOf("result").forGetter(o -> o.result)).apply(i, MapExtendingRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, MapExtendingRecipe> STREAM_CODEC = StreamCodec.composite(
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.map,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.material,
        TransmuteResult.STREAM_CODEC, o -> o.result,
        MapExtendingRecipe::new);
    public static final RecipeSerializer<MapExtendingRecipe> SERIALIZER = null;
    private final ShapedRecipePattern pattern;
    private final Ingredient map;
    private final Ingredient material;
    private final TransmuteResult result;

    public MapExtendingRecipe(Ingredient map, Ingredient material, TransmuteResult result) {
        this.map = map;
        this.material = material;
        this.result = result;
        this.pattern = ShapedRecipePattern.of(java.util.Map.of('#', material, 'x', map), "###", "#x#", "###");
    }
    public boolean matches(CraftingInput input, Level level) {
        if (!pattern.matches(input)) {
            return false;
        }
        ItemStack filled = findFilledMap(input);
        if (filled.isEmpty()) {
            return false;
        }
        net.minecraft.world.level.saveddata.maps.MapItemSavedData data = net.minecraft.world.item.MapItem.getSavedData(filled, level);
        return data != null && data.scale < 4;
    }
    public ItemStack assemble(CraftingInput input) {
        ItemStack source = findFilledMap(input);
        ItemStack out = TransmuteRecipe.createWithOriginalComponents(result.resolve(source.typeHolder()), source);
        out.set(net.minecraft.core.component.DataComponents.MAP_POST_PROCESSING, net.minecraft.world.item.component.MapPostProcessing.SCALE);
        return out;
    }
    public RecipeSerializer<MapExtendingRecipe> getSerializer() {
        return SERIALIZER;
    }
    private static ItemStack findFilledMap(CraftingInput input) {
        for (ItemStack stack : input.items()) {
            if (stack.has(net.minecraft.core.component.DataComponents.MAP_ID)) {
                return stack;
            }
        }
        return ItemStack.EMPTY;
    }
}
