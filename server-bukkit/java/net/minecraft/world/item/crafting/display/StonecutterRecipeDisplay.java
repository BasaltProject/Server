// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record StonecutterRecipeDisplay(SlotDisplay input, SlotDisplay result, SlotDisplay craftingStation) implements RecipeDisplay {
    public static final MapCodec<StonecutterRecipeDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.StonecutterRecipeDisplay.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, StonecutterRecipeDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.StonecutterRecipeDisplay.class, "STREAM_CODEC");
    public static final RecipeDisplay.Type<StonecutterRecipeDisplay> TYPE = new RecipeDisplay.Type<>(StonecutterRecipeDisplay.MAP_CODEC, StonecutterRecipeDisplay.STREAM_CODEC);
    public RecipeDisplay.Type<StonecutterRecipeDisplay> type() {
        return TYPE;
    }
}
