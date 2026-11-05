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
        return com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
            Recipe.CommonInfo.MAP_CODEC.forGetter(r -> r.commonInfo),
            CookingBookInfo.MAP_CODEC.forGetter(r -> r.bookInfo),
            Ingredient.CODEC.fieldOf("ingredient").forGetter(SingleItemRecipe::input),
            ItemStackTemplate.CODEC.fieldOf("result").forGetter(SingleItemRecipe::result),
            com.mojang.serialization.Codec.FLOAT.optionalFieldOf("experience", 0.0F).forGetter(AbstractCookingRecipe::experience),
            com.mojang.serialization.Codec.INT.fieldOf("cookingtime").forGetter(AbstractCookingRecipe::cookingTime)).apply(i, factory::create));
    }
    public static <T extends AbstractCookingRecipe> StreamCodec<RegistryFriendlyByteBuf, T> cookingStreamCodec(Factory<T> factory) {
        return StreamCodec.composite(Recipe.CommonInfo.STREAM_CODEC, r -> r.commonInfo,
            CookingBookInfo.STREAM_CODEC, r -> r.bookInfo, Ingredient.CONTENTS_STREAM_CODEC, SingleItemRecipe::input,
            ItemStackTemplate.STREAM_CODEC, SingleItemRecipe::result, net.minecraft.network.codec.ByteBufCodecs.FLOAT, AbstractCookingRecipe::experience,
            net.minecraft.network.codec.ByteBufCodecs.INT, AbstractCookingRecipe::cookingTime, factory::create);
    }
    public static record CookingBookInfo(CookingBookCategory category, String group) implements Recipe.BookInfo<CookingBookCategory> {
        public static final MapCodec<CookingBookInfo> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.AbstractCookingRecipe.CookingBookInfo.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, CookingBookInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.AbstractCookingRecipe.CookingBookInfo.class, "STREAM_CODEC");
    }
    public interface Factory<T extends AbstractCookingRecipe> {
        T create(Recipe.CommonInfo commonInfo, CookingBookInfo cbookInfotegory, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime);
    }
}
