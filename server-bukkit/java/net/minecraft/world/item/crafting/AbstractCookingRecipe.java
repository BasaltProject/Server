// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
public abstract class AbstractCookingRecipe extends SingleItemRecipe {
    protected final CookingBookInfo bookInfo;
    private final float experience;
    private final int cookingTime;
    public AbstractCookingRecipe(Recipe.CommonInfo commonInfo, CookingBookInfo bookInfo, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime) {
        super(commonInfo, ingredient, result);
        this.bookInfo = bookInfo;
        this.experience = experience;
        this.cookingTime = cookingTime;
    }
    public abstract RecipeSerializer<? extends AbstractCookingRecipe> getSerializer();
    public abstract RecipeType<? extends AbstractCookingRecipe> getType();
    public float experience() {
        return experience;
    }
    public int cookingTime() {
        return cookingTime;
    }
    public CookingBookCategory category() {
        return bookInfo.category();
    }
    public String group() {
        return bookInfo.group();
    }
    protected abstract Item furnaceIcon();
    public List<RecipeDisplay> display() {
        return List.of(new net.minecraft.world.item.crafting.display.FurnaceRecipeDisplay(input().display(), net.minecraft.world.item.crafting.display.SlotDisplay.AnyFuel.INSTANCE,
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay(result()), new net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay(furnaceIcon()),
            cookingTime, experience));
    }
    public static <T extends AbstractCookingRecipe> MapCodec<T> cookingMapCodec(Factory<T> factory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.AbstractCookingRecipe#cookingMapCodec");
    }
    public static <T extends AbstractCookingRecipe> StreamCodec<RegistryFriendlyByteBuf, T> cookingStreamCodec(Factory<T> factory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.AbstractCookingRecipe#cookingStreamCodec");
    }
    public static record CookingBookInfo(CookingBookCategory category, String group) implements Recipe.BookInfo<CookingBookCategory> {
        public static final MapCodec<CookingBookInfo> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.AbstractCookingRecipe.CookingBookInfo.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, CookingBookInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.AbstractCookingRecipe.CookingBookInfo.class, "STREAM_CODEC");
    }
    public interface Factory<T extends AbstractCookingRecipe> {
        T create(Recipe.CommonInfo commonInfo, CookingBookInfo cbookInfotegory, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime);
    }
}
