// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.component.predicates.PotionsPredicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
public record PotionIngredient(Ingredient ingredient, Optional<PotionsPredicate> potions) implements Predicate<ItemStack> {
    public static final MapCodec<PotionIngredient> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.PotionIngredient.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PotionIngredient> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.PotionIngredient.class, "STREAM_CODEC");
    public boolean test(ItemStack input) {
        return ingredient.test(input);
    }
    public static PotionIngredient of(Item item, PotionsPredicate potions) {
        return new PotionIngredient(Ingredient.of(item), Optional.of(potions));
    }
    public static PotionIngredient of(Item item) {
        return new PotionIngredient(Ingredient.of(item), Optional.empty());
    }
    public static boolean isPotionInput(ItemStack itemStack, RecipeAccess recipeAccess) {
        return recipeAccess.propertySet(RecipePropertySet.BREWING_INPUTS).test(itemStack);
    }
}
