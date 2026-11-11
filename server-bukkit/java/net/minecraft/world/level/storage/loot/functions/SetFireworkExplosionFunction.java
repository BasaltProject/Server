// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.FireworkExplosion;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetFireworkExplosionFunction extends LootItemConditionalFunction {
    private FireworkExplosion overlay(FireworkExplosion base) {
        return new FireworkExplosion(shape.orElse(base.shape()), colors.orElse(base.colors()), fadeColors.orElse(base.fadeColors()),
            trail.orElse(base.hasTrail()), twinkle.orElse(base.hasTwinkle()));
    }

    public static final MapCodec<SetFireworkExplosionFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetFireworkExplosionFunction.class, "MAP_CODEC");
    public static final FireworkExplosion DEFAULT_VALUE = new FireworkExplosion(FireworkExplosion.Shape.SMALL_BALL, IntList.of(), IntList.of(), false, false);
    private final Optional<FireworkExplosion.Shape> shape;
    private final Optional<IntList> colors;
    private final Optional<IntList> fadeColors;
    private final Optional<Boolean> trail;
    private final Optional<Boolean> twinkle;
    public SetFireworkExplosionFunction(Optional<Holder<LootItemCondition>> condition, Optional<FireworkExplosion.Shape> shape, Optional<IntList> colors, Optional<IntList> fadeColors, Optional<Boolean> hasTrail, Optional<Boolean> hasTwinkle) {
        super(condition);
        this.shape = shape;
        this.colors = colors;
        this.fadeColors = fadeColors;
        this.trail = hasTrail;
        this.twinkle = hasTwinkle;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.FIREWORK_EXPLOSION, DEFAULT_VALUE, this::overlay);
        return itemStack;
    }
    public MapCodec<SetFireworkExplosionFunction> codec() {
        return MAP_CODEC;
    }
}
