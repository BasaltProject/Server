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
public class ImbueRecipe extends NormalCraftingRecipe {
    public static final MapCodec<ImbueRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.ImbueRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ImbueRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.ImbueRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<ImbueRecipe> SERIALIZER = null;
    private final Ingredient source;
    private final Ingredient material;
    private final ItemStackTemplate result;

    public ImbueRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo, Ingredient source, Ingredient material, ItemStackTemplate result) {
        super(commonInfo, bookInfo);
        this.source = source;
        this.material = material;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.width() != 3 || input.height() != 3 || input.ingredientCount() != 9) {
            return false;
        }
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                Ingredient expected = x == 1 && y == 1 ? source : material;
                if (!expected.test(input.getItem(x, y))) {
                    return false;
                }
            }
        }
        return true;
    }
    public ItemStack assemble(CraftingInput input) {
        ItemStack out = result.create();
        out.set(net.minecraft.core.component.DataComponents.POTION_CONTENTS, input.getItem(1, 1).get(net.minecraft.core.component.DataComponents.POTION_CONTENTS));
        return out;
    }
    public RecipeSerializer<ImbueRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return null;
    }
    public List<RecipeDisplay> display() {
        return List.of();
    }
}
