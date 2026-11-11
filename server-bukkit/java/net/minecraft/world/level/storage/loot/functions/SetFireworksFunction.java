// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import java.util.List;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.FireworkExplosion;
import net.minecraft.world.item.component.Fireworks;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetFireworksFunction extends LootItemConditionalFunction {
    private final Optional<ListOperation.StandAlone<FireworkExplosion>> explosions;
    private final Optional<Integer> flightDuration;
    private Fireworks merge(Fireworks old) {
        return new Fireworks(flightDuration.orElseGet(old::flightDuration), explosions.map(operation -> operation.apply(old.explosions())).orElse(old.explosions()));
    }
    public static final MapCodec<SetFireworksFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetFireworksFunction.class, "MAP_CODEC");
    public static final Fireworks DEFAULT_VALUE = new Fireworks(0, List.of());
    protected SetFireworksFunction(Optional<Holder<LootItemCondition>> condition, Optional<ListOperation.StandAlone<FireworkExplosion>> explosions, Optional<Integer> flightDuration) {
        super(condition);
        this.explosions = explosions;
        this.flightDuration = flightDuration;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.FIREWORKS, DEFAULT_VALUE, this::merge);
        return itemStack;
    }
    public MapCodec<SetFireworksFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
