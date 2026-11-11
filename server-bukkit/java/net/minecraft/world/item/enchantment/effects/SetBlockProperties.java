// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.component.BlockItemStateProperties;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.Vec3;
public record SetBlockProperties(BlockItemStateProperties properties, Vec3i offset, Optional<Holder<GameEvent>> triggerGameEvent) implements EnchantmentEntityEffect {
    public static final MapCodec<SetBlockProperties> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SetBlockProperties.class, "CODEC");
    public SetBlockProperties(BlockItemStateProperties properties) {
        this(properties, Vec3i.ZERO, Optional.of(GameEvent.BLOCK_CHANGE));
    }
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        BlockPos at = BlockPos.containing(position).offset(offset);
        BlockState before = entity.level().getBlockState(at);
        BlockState after = properties.apply(before);
        if (before != after && entity.level().setBlockAndUpdate(at, after)) {
            triggerGameEvent.ifPresent(event -> serverLevel.gameEvent(entity, event, at));
        }
    }
    public MapCodec<SetBlockProperties> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
