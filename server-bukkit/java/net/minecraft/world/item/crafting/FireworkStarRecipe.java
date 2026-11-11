// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.component.FireworkExplosion;
import net.minecraft.world.level.Level;
public class FireworkStarRecipe extends CustomRecipe {
    public static final MapCodec<FireworkStarRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.FireworkStarRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, FireworkStarRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.FireworkStarRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<FireworkStarRecipe> SERIALIZER = new RecipeSerializer<>(MAP_CODEC, STREAM_CODEC);
    private final Map<FireworkExplosion.Shape, Ingredient> shapes;
    private final Ingredient trail;
    private final Ingredient twinkle;
    private final Ingredient fuel;
    private final Ingredient dye;
    private final ItemStackTemplate result;

    public FireworkStarRecipe(Map<FireworkExplosion.Shape, Ingredient> shapes, Ingredient trail, Ingredient twinkle, Ingredient fuel, Ingredient dye, ItemStackTemplate result) {
        this.shapes = shapes;
        this.trail = trail;
        this.twinkle = twinkle;
        this.fuel = fuel;
        this.dye = dye;
        this.result = result;
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() < 2) {
            return false;
        }
        boolean hasFuel = false;
        boolean hasDye = false;
        boolean hasShape = false;
        boolean hasTrail = false;
        boolean hasTwinkle = false;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (twinkle.test(stack)) {
                if (hasTwinkle) {
                    return false;
                }
                hasTwinkle = true;
            } else if (trail.test(stack)) {
                if (hasTrail) {
                    return false;
                }
                hasTrail = true;
            } else if (fuel.test(stack)) {
                if (hasFuel) {
                    return false;
                }
                hasFuel = true;
            } else if (dye.test(stack) && stack.has(net.minecraft.core.component.DataComponents.DYE)) {
                hasDye = true;
            } else {
                if (shapeOf(stack) == null || hasShape) {
                    return false;
                }
                hasShape = true;
            }
        }
        return hasFuel && hasDye;
    }
    public ItemStack assemble(CraftingInput input) {
        FireworkExplosion.Shape shape = FireworkExplosion.Shape.SMALL_BALL;
        boolean hasTwinkle = false;
        boolean hasTrail = false;
        it.unimi.dsi.fastutil.ints.IntList colors = new it.unimi.dsi.fastutil.ints.IntArrayList();
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            FireworkExplosion.Shape found = shapeOf(stack);
            if (found != null) {
                shape = found;
            } else if (twinkle.test(stack)) {
                hasTwinkle = true;
            } else if (trail.test(stack)) {
                hasTrail = true;
            } else if (dye.test(stack)) {
                colors.add(stack.getOrDefault(net.minecraft.core.component.DataComponents.DYE, net.minecraft.world.item.DyeColor.WHITE).getFireworkColor());
            }
        }
        ItemStack out = result.create();
        out.set(net.minecraft.core.component.DataComponents.FIREWORK_EXPLOSION,
            new FireworkExplosion(shape, colors, it.unimi.dsi.fastutil.ints.IntList.of(), hasTrail, hasTwinkle));
        return out;
    }
    public RecipeSerializer<FireworkStarRecipe> getSerializer() {
        return SERIALIZER;
    }
    private FireworkExplosion.Shape shapeOf(ItemStack stack) {
        for (Map.Entry<FireworkExplosion.Shape, Ingredient> entry : shapes.entrySet()) {
            if (entry.getValue().test(stack)) {
                return entry.getKey();
            }
        }
        return null;
    }
}
