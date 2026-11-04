// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public abstract class SingleItemRecipe implements Recipe<SingleRecipeInput> {
    protected final Recipe.CommonInfo commonInfo;
    private final Ingredient input;
    private final ItemStackTemplate result;
    public SingleItemRecipe(Recipe.CommonInfo commonInfo, Ingredient input, ItemStackTemplate result) {
        this.commonInfo = commonInfo;
        this.input = input;
        this.result = result;
    }
    public abstract RecipeSerializer<? extends SingleItemRecipe> getSerializer();
    public abstract RecipeType<? extends SingleItemRecipe> getType();
    public boolean matches(SingleRecipeInput input, Level level) {
        return this.input.test(input.item());
    }
    public boolean showNotification() {
        return commonInfo.showNotification();
    }
    public Ingredient input() {
        return input;
    }
    protected ItemStackTemplate result() {
        return result;
    }
    public PlacementInfo placementInfo() {
        return null;
    }
    public ItemStack assemble(SingleRecipeInput input) {
        return result.create();
    }
    public static <T extends SingleItemRecipe> MapCodec<T> simpleMapCodec(Factory<T> factory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.SingleItemRecipe#simpleMapCodec");
    }
    public static <T extends SingleItemRecipe> StreamCodec<RegistryFriendlyByteBuf, T> simpleStreamCodec(Factory<T> factory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.SingleItemRecipe#simpleStreamCodec");
    }
    public interface Factory<T extends SingleItemRecipe> {
        T create(Recipe.CommonInfo commonInfo, Ingredient ingredient, ItemStackTemplate result);
    }
}
