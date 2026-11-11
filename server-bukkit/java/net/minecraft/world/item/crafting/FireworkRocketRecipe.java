// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class FireworkRocketRecipe extends CustomRecipe {
    public static final MapCodec<FireworkRocketRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Ingredient.CODEC.fieldOf("shell").forGetter(o -> o.shell),
        Ingredient.CODEC.fieldOf("fuel").forGetter(o -> o.fuel),
        Ingredient.CODEC.fieldOf("star").forGetter(o -> o.star),
        ItemStackTemplate.CODEC.fieldOf("result").forGetter(o -> o.result)).apply(i, FireworkRocketRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, FireworkRocketRecipe> STREAM_CODEC = StreamCodec.composite(
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.shell,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.fuel,
        Ingredient.CONTENTS_STREAM_CODEC, o -> o.star,
        ItemStackTemplate.STREAM_CODEC, o -> o.result,
        FireworkRocketRecipe::new);
    public static final RecipeSerializer<FireworkRocketRecipe> SERIALIZER = new RecipeSerializer<>(MAP_CODEC, STREAM_CODEC);
    private final Ingredient shell;
    private final Ingredient fuel;
    private final Ingredient star;
    private final ItemStackTemplate result;

    public FireworkRocketRecipe(Ingredient shell, Ingredient fuel, Ingredient star, ItemStackTemplate result) {
        this.shell = shell;
        this.fuel = fuel;
        this.star = star;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() < 2) {
            return false;
        }
        boolean hasShell = false;
        int fuels = 0;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (shell.test(stack)) {
                if (hasShell) {
                    return false;
                }
                hasShell = true;
            } else if (fuel.test(stack)) {
                if (++fuels > 3) {
                    return false;
                }
            } else if (!star.test(stack)) {
                return false;
            }
        }
        return hasShell && fuels >= 1;
    }
    public ItemStack assemble(CraftingInput input) {
        java.util.List<net.minecraft.world.item.component.FireworkExplosion> explosions = new java.util.ArrayList<>();
        int fuels = 0;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (fuel.test(stack)) {
                fuels++;
            } else if (star.test(stack)) {
                net.minecraft.world.item.component.FireworkExplosion explosion = stack.get(net.minecraft.core.component.DataComponents.FIREWORK_EXPLOSION);
                if (explosion != null) {
                    explosions.add(explosion);
                }
            }
        }
        return result.apply(net.minecraft.core.component.DataComponentPatch.builder()
            .set(net.minecraft.core.component.DataComponents.FIREWORKS, new net.minecraft.world.item.component.Fireworks(fuels, explosions)).build());
    }
    public RecipeSerializer<FireworkRocketRecipe> getSerializer() {
        return SERIALIZER;
    }
}
