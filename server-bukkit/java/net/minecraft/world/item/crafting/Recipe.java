// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiFunction;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.level.Level;
public interface Recipe<T extends RecipeInput> {
    Codec<Recipe<?>> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.Recipe.class, "DIRECT_CODEC");
    Codec<ResourceKey<Recipe<?>>> KEY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.Recipe.class, "KEY_CODEC");
    StreamCodec<RegistryFriendlyByteBuf, Recipe<?>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.Recipe.class, "STREAM_CODEC");
    Codec<Holder<Recipe<?>>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.Recipe.class, "CODEC");
    Codec<HolderSet<Recipe<?>>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.Recipe.class, "LIST_CODEC");
    boolean matches(T input, Level level);
    ItemStack assemble(T input);
    default boolean isSpecial() {
        return false;
    }
    boolean showNotification();
    String group();
    RecipeSerializer<? extends Recipe<T>> getSerializer();
    RecipeType<? extends Recipe<T>> getType();
    PlacementInfo placementInfo();
    default List<RecipeDisplay> display() {
        return List.of();
    }
    RecipeBookCategory recipeBookCategory();
    public static record CommonInfo(boolean showNotification) {
        public static final MapCodec<CommonInfo> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.Recipe.CommonInfo.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, CommonInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.Recipe.CommonInfo.class, "STREAM_CODEC");
    }
    public interface BookInfo<CategoryType> {
        CategoryType category();
        String group();
        static <CategoryType, SelfType extends BookInfo<CategoryType>> MapCodec<SelfType> mapCodec(Codec<CategoryType> categoryCodec, CategoryType defaultCategory, Constructor<CategoryType, SelfType> constructor) {
            return com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
                categoryCodec.fieldOf("category").orElse(defaultCategory).forGetter(BookInfo::category),
                com.mojang.serialization.Codec.STRING.optionalFieldOf("group", "").forGetter(BookInfo::group)).apply(i, constructor));
        }
        static <CategoryType, SelfType extends BookInfo<CategoryType>> StreamCodec<RegistryFriendlyByteBuf, SelfType> streamCodec(StreamCodec<? super RegistryFriendlyByteBuf, CategoryType> categoryCodec, Constructor<CategoryType, SelfType> constructor) {
            return StreamCodec.composite(categoryCodec, BookInfo::category, net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8, BookInfo::group, constructor);
        }
        public interface Constructor<CategoryType, SelfType extends BookInfo<CategoryType>> extends BiFunction<CategoryType, String, SelfType> {
        }
    }
}
