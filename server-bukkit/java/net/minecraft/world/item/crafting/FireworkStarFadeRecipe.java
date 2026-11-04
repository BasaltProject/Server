// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class FireworkStarFadeRecipe extends CustomRecipe {
    public static final MapCodec<FireworkStarFadeRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.FireworkStarFadeRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, FireworkStarFadeRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.FireworkStarFadeRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<FireworkStarFadeRecipe> SERIALIZER = null;
    private final Ingredient target;
    private final Ingredient dye;
    private final ItemStackTemplate result;

    public FireworkStarFadeRecipe(Ingredient target, Ingredient dye, ItemStackTemplate result) {
        this.target = target;
        this.dye = dye;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() < 2) {
            return false;
        }
        boolean hasDye = false;
        boolean hasTarget = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (dye.test(stack) && stack.has(net.minecraft.core.component.DataComponents.DYE)) {
                hasDye = true;
            } else if (target.test(stack) && !hasTarget) {
                hasTarget = true;
            } else {
                return false;
            }
        }
        return hasTarget && hasDye;
    }
    public ItemStack assemble(CraftingInput input) {
        it.unimi.dsi.fastutil.ints.IntList colors = new it.unimi.dsi.fastutil.ints.IntArrayList();
        ItemStack targetStack = null;
        for (ItemStack stack : input.items()) {
            if (dye.test(stack)) {
                colors.add(stack.getOrDefault(net.minecraft.core.component.DataComponents.DYE, net.minecraft.world.item.DyeColor.WHITE).getFireworkColor());
            } else if (target.test(stack)) {
                targetStack = stack;
            }
        }
        if (targetStack == null || colors.isEmpty()) {
            return ItemStack.EMPTY;
        }
        ItemStack out = TransmuteRecipe.createWithOriginalComponents(result, targetStack);
        out.update(net.minecraft.core.component.DataComponents.FIREWORK_EXPLOSION, net.minecraft.world.item.component.FireworkExplosion.DEFAULT, colors, net.minecraft.world.item.component.FireworkExplosion::withFadeColors);
        return out;
    }
    public RecipeSerializer<FireworkStarFadeRecipe> getSerializer() {
        return SERIALIZER;
    }
}
