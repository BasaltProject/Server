// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.level.Level;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public record SummonEntityEffect(HolderSet<EntityType<?>> entityTypes, boolean joinTeam) implements EnchantmentEntityEffect {
    public static final MapCodec<SummonEntityEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SummonEntityEffect.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        BlockPos pos = BlockPos.containing(position);
        if (!Level.isInSpawnableBounds(pos)) {
            return;
        }
        Optional<Holder<EntityType<?>>> pick = entityTypes().getRandomElement(serverLevel.getRandom());
        if (pick.isEmpty()) {
            return;
        }
        Entity spawned = pick.get().value().spawn(serverLevel, pos, EntitySpawnReason.TRIGGERED);
        if (spawned == null) {
            return;
        }
        if (spawned instanceof LightningBolt bolt && item.owner() instanceof ServerPlayer player) {
            bolt.setCause(player);
        }
        if (joinTeam && entity.getTeam() != null) {
            serverLevel.getScoreboard().addPlayerToTeam(spawned.getScoreboardName(), entity.getTeam());
        }
        spawned.snapTo(position.x, position.y, position.z, spawned.getYRot(), spawned.getXRot());


    }
    public MapCodec<SummonEntityEffect> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
