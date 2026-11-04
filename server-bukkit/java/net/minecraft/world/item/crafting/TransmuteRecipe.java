// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.level.Level;
public class TransmuteRecipe extends NormalCraftingRecipe {
    public static final MinMaxBounds.Ints DEFAULT_MATERIAL_COUNT = MinMaxBounds.Ints.exactly(1);
    public static final MinMaxBounds.Ints FULL_RANGE_MATERIAL_COUNT = MinMaxBounds.Ints.between(1, 8);
    public static final Codec<MinMaxBounds.Ints> MATERIAL_COUNT_BOUNDS = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.TransmuteRecipe.class, "MATERIAL_COUNT_BOUNDS");
    public static final MapCodec<TransmuteRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.TransmuteRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, TransmuteRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.TransmuteRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<TransmuteRecipe> SERIALIZER = null;
    private final Ingredient input;
    private final Ingredient material;
    private final MinMaxBounds.Ints materialCount;
    private final TransmuteResult result;
    private final boolean addMaterialCountToResult;

    public TransmuteRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo, Ingredient input, Ingredient material, MinMaxBounds.Ints materialCount, TransmuteResult result, boolean addMaterialCountToResult) {
        super(commonInfo, bookInfo);
        this.input = input;
        this.material = material;
        this.materialCount = materialCount;
        this.result = result;
        this.addMaterialCountToResult = addMaterialCountToResult;
    }
    public static ItemStack createWithOriginalComponents(ItemStackTemplate target, ItemStack input) {
        return createWithOriginalComponents(target, input, 0);
    }
    public static ItemStack createWithOriginalComponents(ItemStackTemplate target, ItemStack input, int extraCount) {
        return target.apply(target.count() + extraCount, input.getComponentsPatch());
    }
    public boolean matches(CraftingInput input, Level level) {
        int minMaterials = minMaterialCount();
        int maxMaterials = maxMaterialCount();
        if (input.ingredientCount() < minMaterials + 1 || input.ingredientCount() > maxMaterials + 1) {
            return false;
        }
        ItemStack foundInput = null;
        int materials = 0;
        for (int slot = 0; slot < input.size(); slot++) {
            ItemStack stack = input.getItem(slot);
            if (stack.isEmpty()) {
                continue;
            }
            if (this.input.test(stack)) {
                if (foundInput != null) {
                    return false;
                }
                foundInput = stack;
            } else if (material.test(stack)) {
                if (++materials > maxMaterials) {
                    return false;
                }
            } else {
                return false;
            }
        }
        if (foundInput == null || !materialCount.matches(materials)) {
            return false;
        }
        if (resultSize(materials) != 1) {
            return true;
        }
        ItemStack out = resolveResult(foundInput, 0);
        return !out.isEmpty() && !ItemStack.isSameItemSameComponents(foundInput, out);
    }
    public ItemStack assemble(CraftingInput input) {
        int materials = 0;
        ItemStack source = ItemStack.EMPTY;
        for (int slot = 0; slot < input.size(); slot++) {
            ItemStack stack = input.getItem(slot);
            if (stack.isEmpty()) {
                continue;
            }
            if (this.input.test(stack)) {
                source = stack;
            } else if (addMaterialCountToResult && material.test(stack)) {
                materials++;
            }
        }
        return source.isEmpty() ? ItemStack.EMPTY : resolveResult(source, materials);
    }
    public List<RecipeDisplay> display() {
        return List.of();
    }
    public RecipeSerializer<TransmuteRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return null;
    }
    private int minMaterialCount() {
        return materialCount.min().orElse(1);
    }
    private int maxMaterialCount() {
        return materialCount.max().orElse(8);
    }
    private int resultSize(int materials) {
        return addMaterialCountToResult ? materials + result.count() : result.count();
    }
    private ItemStack resolveResult(ItemStack source, int materials) {
        return createWithOriginalComponents(result.resolve(source.typeHolder()), source, addMaterialCountToResult ? materials : 0);
    }
}
